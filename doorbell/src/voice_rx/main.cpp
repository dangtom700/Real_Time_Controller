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

#include <Arduino.h>
#include "config_v1.h"
#include "protocol.h"
#include "link.h"

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
    if (m.type == MSG_VOICE_END)   { Serial.println("[voice-rx] session END"); continue; }
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
    Serial.println();

    if (started && windowRx == 0) Serial.println("[voice-rx] ...nothing arriving");

    windowStart = nowMs;
    windowRx = windowLost = 0;
    gapMaxUs = 0;
  }
}
