// STEP 1 (receiver) — the wire.
//
// Wiring:  see step1_ping_tx/main.cpp.  This board's GPIO0 takes the other
//          board's GPIO1, and the grounds MUST be tied together.
// Run:     pio run -e step1_ping_rx -t upload -t monitor --upload-port COMy
// PASS:    "got #1, #2, #3 ..." with no gaps and no CRC errors, and the
//          pressCount walking 1,2,3,3,3,3 in step with the sender's log.
//
// FAIL and what it means:
//   nothing at all      -> TX/RX not crossed, or no common ground
//   CRC errors climbing -> baud mismatch, or a long/noisy jumper
//   gaps in the seq     -> should not happen on a UART. If it does, suspect
//                          the RX buffer (link.h raises it to 4096) or that
//                          this loop is being blocked by something.

#include <Arduino.h>
#include "config_v1.h"
#include "protocol.h"
#include "link.h"

static uint32_t expected = 1;
static uint32_t received = 0, lost = 0, bad = 0;

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[step1-rx] listening on UART1");
  Serial.printf("[step1-rx] TX=GPIO%d  RX=GPIO%d  %d baud\n",
                LINK_TX_GPIO, LINK_RX_GPIO, LINK_BAUD);
  linkBegin();
}

void loop() {
  uint8_t buf[LINK_MAX_PAYLOAD];
  const uint8_t n = linkPoll(buf, sizeof buf);
  if (n == 0) return;

  if (n != sizeof(DoorbellMsg)) {
    bad++;
    Serial.printf("[step1-rx] unexpected length %u (want %u)\n",
                  n, (unsigned)sizeof(DoorbellMsg));
    return;
  }

  DoorbellMsg m;
  memcpy(&m, buf, sizeof m);

  if (m.version != PROTO_VERSION || m.type != MSG_DOORBELL) {
    bad++;
    Serial.printf("[step1-rx] bad header ver=%u type=%u\n", m.version, m.type);
    return;
  }

  received++;
  if (m.seq > expected) {
    lost += (m.seq - expected);
    Serial.printf("[step1-rx] ** LOST %lu **\n", (unsigned long)(m.seq - expected));
  }
  expected = m.seq + 1;

  Serial.printf("[step1-rx] got #%lu  press=%u locked=%u   (rx=%lu lost=%lu crc=%lu)\n",
                (unsigned long)m.seq, m.pressCount, m.locked,
                (unsigned long)received, (unsigned long)lost,
                (unsigned long)linkErrCrc);
}
