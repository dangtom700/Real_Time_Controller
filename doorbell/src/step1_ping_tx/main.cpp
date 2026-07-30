// STEP 1 (sender) — the wire.
//
// Wiring:  board A GPIO1  ->  board B GPIO0        (link TX -> link RX)
//          board A GND    ->  board B GND          (REQUIRED - see README)
//          both boards on USB, two serial monitors open
// Run:     pio run -e step1_ping_tx -t upload -t monitor --upload-port COMx
// PASS:    this board prints "sent #n" once a second, and board B prints a
//          matching "got #n" with no gaps.
//
// One DoorbellMsg per second is slow on purpose.  Step 1 is asking "are these
// two boards physically connected and do they agree on the byte order", and a
// 1 Hz packet you can watch by eye answers that better than a firehose does.
// Step 2 turns the rate up to the real thing.

#include <Arduino.h>
#include "config_v1.h"
#include "protocol.h"
#include "link.h"

static uint32_t seq = 0;

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[step1-tx] DoorbellMsg over UART1, 1 Hz");
  Serial.printf("[step1-tx] TX=GPIO%d  RX=GPIO%d  %d baud\n",
                LINK_TX_GPIO, LINK_RX_GPIO, LINK_BAUD);
  Serial.printf("[step1-tx] sizeof(DoorbellMsg)=%u\n", (unsigned)sizeof(DoorbellMsg));
  linkBegin();
}

void loop() {
  DoorbellMsg m = {};
  m.version    = PROTO_VERSION;
  m.type       = MSG_DOORBELL;
  // Walk 1,2,3,3,3... exactly as the clamp in PLAN §2.3.1 will, so the
  // receiver is looking at realistic values rather than a free-running int.
  m.pressCount = (uint8_t)(seq % 6 < RAGE_LIMIT ? (seq % 6) + 1 : RAGE_LIMIT);
  m.locked     = (m.pressCount >= RAGE_LIMIT) ? 1 : 0;
  m.seq        = ++seq;

  const bool ok = linkSend(&m, sizeof m);
  Serial.printf("[step1-tx] sent #%lu  press=%u locked=%u%s\n",
                (unsigned long)m.seq, m.pressCount, m.locked,
                ok ? "" : "   <-- SEND FAILED");
  delay(1000);
}
