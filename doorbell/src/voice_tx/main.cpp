// STEPS 2 & 3 (sender) — voice-rate traffic.  One sketch, two transports.
//
//   step 2:  pio run -e voice_tx_uart   -t upload -t monitor --upload-port COMx
//   step 3:  pio run -e voice_tx_espnow -t upload -t monitor --upload-port COMx
//
// The ONLY difference is -DLINK_SERIAL vs -DLINK_ESPNOW.  Nothing below this
// comment block knows which one it got.  That is PLAN_v2.md §4.3's seam doing
// the job it was promoted to do.
//
// WHY THERE IS NO MICROPHONE HERE
//   The INMP441 is not on the desk yet, and waiting for it would leave the
//   riskiest part of the project untested.  So this generates the samples an
//   8 kHz mic WOULD have produced - a continuous 440 Hz sine - and paces them
//   at the real cadence: 110 samples every 13.75 ms, 72.7 packets/s, 130 kbps.
//   The link cannot tell the difference.  When the mic arrives, step 4 deletes
//   fillSine() and drops i2s_read() in its place; everything else stands.
//
//   The sine is not arbitrary either: when the MAX98357A arrives, this exact
//   stream should come out of the speaker as a clean, steady A4.  A buzzing or
//   warbling tone at that point means the transport, not the amp.
//
// PASS: "sent" holds at ~73/s, failed=0, and late-max stays well under 13750us.
//       On ESP-NOW, ack-fail is the number PLAN §5.1 tells you to watch.

#include <Arduino.h>
#include <math.h>
#include "config_v1.h"
#include "protocol.h"
#include "link.h"

static const float kTwoPi  = 6.28318530718f;
static const float kToneHz = 440.0f;
static const float kStep   = kTwoPi * kToneHz / (float)VOICE_SAMPLE_RATE;
static const int16_t kAmp  = 8000;            // ~1/4 full scale, headroom to spare

static float    phase   = 0.0f;
static uint16_t seq     = 0;
static uint32_t nextDue = 0;

// Stats for the once-a-second line.  Printing per packet would itself blow the
// 13.75 ms budget at 115200 baud, which is a trap worth stating out loud.
static uint32_t sent = 0, failed = 0, resyncs = 0;
static int32_t  worstLate = 0;
static uint32_t windowStart = 0, windowSent = 0;

static void fillSine(int16_t *dst, uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    dst[i] = (int16_t)(sinf(phase) * (float)kAmp);
    phase += kStep;
    if (phase >= kTwoPi) phase -= kTwoPi;     // continuous across packets
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[voice-tx] synthetic 440 Hz source at the real audio cadence");
#ifdef LINK_SERIAL
  Serial.printf("[voice-tx] transport = UART1  GPIO%d->peer GPIO%d @ %d baud\n",
                LINK_TX_GPIO, LINK_RX_GPIO, LINK_BAUD);
#else
  Serial.printf("[voice-tx] transport = ESP-NOW  channel %d\n", ESPNOW_CHANNEL);
  if (linkPeerUnset()) {
    Serial.println("[voice-tx] !! LINK_PEER_MAC_INIT is still all zeroes.");
    Serial.println("[voice-tx] !! Put the RECEIVER's MAC (step 0 printed it)");
    Serial.println("[voice-tx] !! into include/config_v1.h and reflash.");
  }
#endif
  Serial.printf("[voice-tx] %u samples/chunk, %lu us/chunk, %u B/packet\n",
                VOICE_SAMPLES, (unsigned long)VOICE_CHUNK_US, (unsigned)sizeof(VoiceMsg));
  Serial.println("[voice-tx] expect ~73 packets/s, ~130 kbps payload");

  if (!linkBegin()) {
    Serial.println("[voice-tx] linkBegin() FAILED - stopping");
    while (true) delay(1000);
  }

  // Announce the session once, so the receiver can zero its counters and so
  // the type field is exercised rather than assumed.
  VoiceMsg hello = {};
  hello.version = PROTO_VERSION;
  hello.type    = MSG_VOICE_BEGIN;
  hello.seq     = 0;
  linkSend(&hello, sizeof hello);

  nextDue     = micros() + VOICE_CHUNK_US;
  windowStart = millis();
}

void loop() {
  // ---- once-a-second report -----------------------------------------------
  const uint32_t nowMs = millis();
  if (nowMs - windowStart >= 1000) {
    const uint32_t elapsed = nowMs - windowStart;
    Serial.printf("[voice-tx] %lu pkt/s  sent=%lu failed=%lu  late-max=%ldus  resync=%lu",
                  (unsigned long)(windowSent * 1000UL / elapsed),
                  (unsigned long)sent, (unsigned long)failed,
                  (long)worstLate, (unsigned long)resyncs);
#ifdef LINK_ESPNOW
    Serial.printf("  ack-ok=%lu ack-FAIL=%lu",
                  (unsigned long)linkAckOk, (unsigned long)linkAckFail);
#endif
    Serial.println();
    windowStart = nowMs;
    windowSent  = 0;
    worstLate   = 0;
  }

  // ---- pacing -------------------------------------------------------------
  // Deadline scheduling, not delay(): nextDue advances by exactly one chunk
  // period each time, so a slow iteration is absorbed by the next rather than
  // permanently shifting the cadence. This is how the I2S DMA will behave.
  const int32_t late = (int32_t)(micros() - nextDue);
  if (late < 0) return;
  if (late > worstLate) worstLate = late;

  nextDue += VOICE_CHUNK_US;

  // If we have fallen more than ~10 chunks behind, we will never catch up by
  // sprinting - something blocked for >137 ms. Resync and count it, rather
  // than emitting a burst that misrepresents the link's real behaviour.
  if ((int32_t)(micros() - nextDue) > (int32_t)(10 * VOICE_CHUNK_US)) {
    nextDue = micros() + VOICE_CHUNK_US;
    resyncs++;
  }

  // ---- one chunk ----------------------------------------------------------
  VoiceMsg m;
  m.version = PROTO_VERSION;
  m.type    = MSG_VOICE_CHUNK;
  m.seq     = ++seq;
  fillSine(m.pcm, VOICE_SAMPLES);

  if (linkSend(&m, sizeof m)) { sent++; windowSent++; }
  else                        { failed++; }
}
