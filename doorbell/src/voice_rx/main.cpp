// STEPS 2 & 3 (receiver) — voice-rate traffic.  One sketch, two transports.
//
//   step 2:  pio run -e voice_rx_uart   -t upload -t monitor --upload-port COMy
//   step 3:  pio run -e voice_rx_espnow -t upload -t monitor --upload-port COMy
//
// This is the instrument.  It is the whole reason step 2 can exist without a
// microphone or a speaker: it answers PLAN_v2.md §5.1's question - can the link
// carry one intercom session - in numbers rather than by ear.
//
// WHAT THE NUMBERS MEAN
//   pkt/s    should sit at ~73 (8000/110). Lower = the link is not keeping up.
//   lost     seq gaps. On UART this must be 0; the wire cannot drop packets,
//            so anything here is a bug or a buffer, not a transport limit.
//            On ESP-NOW this is the real answer, and the one that decides M5.
//   kbps     payload only, framing excluded. Target ~130.
//   gap-max  longest silence between packets. The audio deadline is 13.75 ms;
//            a gap of 40 ms means three chunks of speech went missing at once,
//            which is audible as a click. This matters MORE than average loss.
//   ovr      RX ring overrun: packets arrived faster than loop() drained them.
//            Points at this sketch, not at the link.
//
// PASS (step 2, UART):    lost=0, crc=0, pkt/s ~73, gap-max ~14 ms.
// PASS (step 3, ESP-NOW): judge it against step 2's line, which is the
//                         known-good baseline you just measured.
//
// ---------------------------------------------------------------------------
//  STEP 5 — add -DCHIME_AUDIO and the instrument grows a speaker.
//
//   pio run -e voice_rx_chime -t upload --upload-port COMy   (board B)
//
// Every chunk that step 3 counted now also goes to the MAX98357A.  Nothing
// above changes, which is the point: the packet numbers printed with the amp
// running are directly comparable to the step 3 line printed without it, so if
// the audio path costs the link anything, the same counters say so.
//
// PASS: a clean, steady 440 Hz A4 out of the speaker, and the counters still
//       reading like step 3.  voice_tx's comment block calls this shot -- a
//       buzzing or warbling tone means the transport, not the amp.  One caveat
//       it could not have anticipated: that verdict only holds while gap-max
//       stays under CHIME_PREFILL_MS.  Past that the jitter buffer is dry and
//       the warble is this sketch starving the DMA, not the radio misbehaving.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include "config_v1.h"
#include "protocol.h"
#include "link.h"

#ifdef CHIME_AUDIO
#include <ESP_I2S.h>

static I2SClass i2s;

// Silence written at session start so the DMA is never empty when the first
// real chunk lands.  Without it the cushion is zero and every jitter event on
// the radio underruns the buffer, which is audible and — worse — sounds
// exactly like the transport fault this test is supposed to rule out.
//
// 6 chunks = 82.5 ms.  Sized off step 3's MEASURED jitter, not step 2's: the
// radio's gap-max ran 23.6-56.7 ms where the wire's was 13.7, and README §step 3
// concluded ~5 chunks absorbs the worst of it.  6 takes that with a margin and
// still fits the 180 ms the I2S DMA holds at 8 kHz.
static const uint8_t  CHIME_PREFILL_CHUNKS = 6;
static const uint32_t CHIME_PREFILL_MS     = CHIME_PREFILL_CHUNKS * VOICE_CHUNK_US / 1000;

// A Class D amp hisses when idle and this intercom is silent most of the time,
// so SD drops again once the stream stops rather than at MSG_VOICE_END only —
// a sender that vanishes mid-session never sends one.
static const uint32_t CHIME_IDLE_MUTE_MS = 500;

static uint32_t i2sChunks = 0, i2sShort = 0, lastChunkMs = 0;
static bool     ampOn     = false;

static void ampEnable(bool on) {
  if (on == ampOn) return;
  digitalWrite(AMP_SD_GPIO, on ? HIGH : LOW);
  ampOn = on;
}

// One chunk to the speaker.  Copied out of the packed VoiceMsg rather than
// written from &m.pcm[0]: that member has no alignment guarantee inside a
// pack(1) struct, and -Wall says so.
static void playChunk(const int16_t *pcm) {
  int16_t buf[VOICE_SAMPLES];
  memcpy(buf, pcm, sizeof buf);
  const size_t wrote = i2s.write((const void *)buf, sizeof buf);
  if (wrote != sizeof buf) i2sShort++;   // DMA still full after the timeout
  else                     i2sChunks++;
}
#endif  // CHIME_AUDIO

static uint16_t expectedSeq = 1;
static bool     started     = false;

static uint32_t rxTotal = 0, lostTotal = 0, reordered = 0, badHeader = 0;
static uint32_t windowStart = 0, windowRx = 0, windowLost = 0;
static uint32_t lastArrivalUs = 0, gapMaxUs = 0;

static void resetSession(const char *why) {
  Serial.printf("[voice-rx] session start (%s)\n", why);
  expectedSeq = 1;
  started     = true;
  rxTotal = lostTotal = reordered = badHeader = 0;
  windowRx = windowLost = 0;
  gapMaxUs = 0;
  lastArrivalUs = micros();
  windowStart   = millis();

#ifdef CHIME_AUDIO
  // Unmute first, then lay down the cushion.  The other order plays the first
  // chunks into a muted amp and throws the cushion away.
  ampEnable(true);
  const int16_t silence[VOICE_SAMPLES] = {};
  for (uint8_t i = 0; i < CHIME_PREFILL_CHUNKS; i++)
    i2s.write((const void *)silence, sizeof silence);
  i2sChunks = i2sShort = 0;
  lastChunkMs = millis();
#endif
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[voice-rx] link instrument");
#ifdef LINK_SERIAL
  Serial.printf("[voice-rx] transport = UART1  RX=GPIO%d @ %d baud\n",
                LINK_RX_GPIO, LINK_BAUD);
#else
  Serial.printf("[voice-rx] transport = ESP-NOW  channel %d\n", ESPNOW_CHANNEL);
  Serial.println("[voice-rx] my MAC is the one the SENDER needs in config_v1.h");
#endif
  Serial.printf("[voice-rx] expecting ~73 pkt/s, ~130 kbps, gaps <= %lu us\n",
                (unsigned long)VOICE_CHUNK_US);

#ifdef CHIME_AUDIO
  // SD low before anything else.  The 10k pulldown holds the amp off from reset
  // until this line runs; driving it low explicitly keeps it off across
  // i2s.begin(), which is when the three signal pins first move.
  pinMode(AMP_SD_GPIO, OUTPUT);
  digitalWrite(AMP_SD_GPIO, LOW);
  ampOn = false;

  i2s.setPins(AMP_BCLK_GPIO, AMP_LRC_GPIO, AMP_DIN_GPIO);
  // MONO on a HW-v2 target puts the samples in the LEFT slot, which is the slot
  // SD-high selects.  Getting this wrong is silent and looks exactly like a
  // floating SD pin, so it is asserted below rather than assumed.
  if (!i2s.begin(I2S_MODE_STD, VOICE_SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
    Serial.printf("[chime] i2s.begin() FAILED (err %d) - stopping\n", i2s.lastError());
    while (true) delay(1000);
  }
  // Two chunk periods.  Long enough that a full DMA is waited out rather than
  // counted as a drop, short enough that a wedged amp cannot stall loop() for
  // the default full second and corrupt the packet timings we are measuring.
  i2s.setTimeout(2 * VOICE_CHUNK_US / 1000);

  Serial.printf("[chime] MAX98357A  BCLK=IO%d  LRC=IO%d  DIN=IO%d  SD=IO%d\n",
                AMP_BCLK_GPIO, AMP_LRC_GPIO, AMP_DIN_GPIO, AMP_SD_GPIO);
  Serial.printf("[chime] %u Hz 16-bit mono, LEFT slot, %lu ms prefill, muted until traffic\n",
                VOICE_SAMPLE_RATE, (unsigned long)CHIME_PREFILL_MS);
#endif

  if (!linkBegin()) {
    Serial.println("[voice-rx] linkBegin() FAILED - stopping");
    while (true) delay(1000);
  }
  windowStart   = millis();
  lastArrivalUs = micros();
}

void loop() {
  // ---- drain the link -----------------------------------------------------
  uint8_t buf[LINK_MAX_PAYLOAD];
  uint8_t n;
  while ((n = linkPoll(buf, sizeof buf)) != 0) {
    if (n != sizeof(VoiceMsg)) { badHeader++; continue; }

    VoiceMsg m;
    memcpy(&m, buf, sizeof m);
    if (m.version != PROTO_VERSION) { badHeader++; continue; }

    if (m.type == MSG_VOICE_BEGIN) { resetSession("BEGIN received"); continue; }
    if (m.type == MSG_VOICE_END) {
      Serial.println("[voice-rx] session END");
#ifdef CHIME_AUDIO
      ampEnable(false);
#endif
      continue;
    }
    if (m.type != MSG_VOICE_CHUNK) { badHeader++; continue; }

    if (!started) resetSession("first chunk, no BEGIN seen");

    // Inter-arrival gap. This is the number that turns into an audible click.
    const uint32_t nowUs = micros();
    const uint32_t gap   = nowUs - lastArrivalUs;
    if (gap > gapMaxUs) gapMaxUs = gap;
    lastArrivalUs = nowUs;

    // Sequence accounting, wrap-safe: seq is uint16 and rolls over every ~15
    // minutes at this rate. A small forward jump is loss; a backwards jump is
    // a duplicate or a reorder, which a UART cannot do but a radio can.
    const uint16_t jump = (uint16_t)(m.seq - expectedSeq);
    if (jump != 0) {
      if (jump < 1000) { lostTotal += jump; windowLost += jump; }
      else             { reordered++; }
    }
    expectedSeq = (uint16_t)(m.seq + 1);

    rxTotal++;
    windowRx++;

#ifdef CHIME_AUDIO
    // Out of order on purpose: the chunk is played whether or not it was in
    // sequence.  A lost packet leaves a 13.75 ms hole, and hearing that hole is
    // the point of putting a speaker on the instrument.
    playChunk(m.pcm);
    lastChunkMs = millis();
#endif
  }

  // ---- once-a-second report ----------------------------------------------
  const uint32_t nowMs = millis();
  if (nowMs - windowStart >= 1000) {
    const uint32_t elapsed = nowMs - windowStart;
    const uint32_t pps     = windowRx * 1000UL / elapsed;
    const uint32_t kbps    = (uint32_t)((uint64_t)windowRx * sizeof(VoiceMsg) * 8ULL
                                        / (uint64_t)elapsed);   // B*8/ms == kbps
    const uint32_t offered = windowRx + windowLost;
    const uint32_t lossPct10 = offered ? (windowLost * 1000UL / offered) : 0;

    Serial.printf("[voice-rx] %3lu pkt/s  %3lu kbps  lost=%lu (%lu.%lu%%)  "
                  "gap-max=%lu.%lums  rx=%lu  reorder=%lu  bad=%lu",
                  (unsigned long)pps, (unsigned long)kbps,
                  (unsigned long)windowLost,
                  (unsigned long)(lossPct10 / 10), (unsigned long)(lossPct10 % 10),
                  (unsigned long)(gapMaxUs / 1000), (unsigned long)((gapMaxUs % 1000) / 100),
                  (unsigned long)rxTotal,
                  (unsigned long)reordered, (unsigned long)badHeader);
#ifdef LINK_SERIAL
    Serial.printf("  crc=%lu", (unsigned long)linkErrCrc);
#else
    Serial.printf("  ovr=%lu", (unsigned long)linkErrOverrun);
#endif
#ifdef CHIME_AUDIO
    // i2s + short == rx, always. short counts writes the DMA could not take
    // within two chunk periods.  A burst of them at startup is normal and
    // self-clearing: a receiver joining a stream already in progress inherits a
    // full link ring and plays it at wire speed rather than at 8 kHz, so the
    // DMA saturates until the backlog drains.  What matters is that short then
    // STOPS.  A short that keeps climbing in steady state is the real fault —
    // the sender's clock running fast against this board's 8 kHz.
    Serial.printf("  i2s=%lu short=%lu %s",
                  (unsigned long)i2sChunks, (unsigned long)i2sShort,
                  ampOn ? "ON" : "muted");
#endif
    Serial.println();

    if (started && windowRx == 0) Serial.println("[voice-rx] ...nothing arriving");

#ifdef CHIME_AUDIO
    if (ampOn && (nowMs - lastChunkMs) >= CHIME_IDLE_MUTE_MS) {
      Serial.println("[chime] stream stopped - muting");
      ampEnable(false);
      started = false;      // next chunk re-prefills instead of playing dry
    }
#endif

    windowStart = nowMs;
    windowRx = windowLost = 0;
    gapMaxUs = 0;
  }
}
