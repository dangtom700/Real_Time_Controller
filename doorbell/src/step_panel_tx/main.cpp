// STEP PANEL (sender) — board A, the guest panel, talking to board C.
//
//   pio run -e step_panel_tx -t upload --upload-port COMx      (board A)
//
// Pairs with step_panel_rx on the Arduino Nano ESP32.  Three wires:
//
//     A IO0   ──> C D2 (GPIO5)    message, 115200 8N1
//     A IO10  ──> C D3 (GPIO6)    wake ── 10 kΩ ── GND
//     A GND   ─── C GND
//
// This proves the link BEFORE the OLED or the state machine depends on it.
// The doorbell semantics are the same ones step 1 sent over the A<->B wire and
// step_io produced from a real thumb: pressCount walks 1, 2, 3 and then clamps
// at RAGE_LIMIT, and TIMER1_MS expires the window.  Sending the identical
// sequence here means C's decoder is being fed exactly what the finished panel
// will feed it, so nothing about the message changes when the button moves
// behind it.
//
// WHAT IS ACTUALLY BEING TESTED IS THE ORDER, NOT THE BYTES.  panelSendWaking()
// asserts wake, waits PANEL_WAKE_MS, and only then transmits.  Get that
// backwards and it still passes on a bench where C is already awake, then fails
// in the field every time C has gone to sleep.  So this sketch deliberately
// keeps the wake line LOW between messages rather than parking it high, which
// would make the sequencing untestable.
//
// PASS: C prints one decoded press per message, with pressCount walking
//       1,2,3,3,3... and its raw byte count rising in step.

#include <Arduino.h>
#include "config_v1.h"
#include "protocol.h"
#include "panel_link.h"

static const uint32_t SEND_EVERY_MS = 4000;

static uint8_t  pressCount = 0;
static bool     locked     = false;
static uint32_t seq        = 0;
static uint32_t nextSendMs = 0;

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[panel-tx] board A -> board C, simplex UART + wake");
  Serial.printf("[panel-tx] TX=IO%d  WAKE=IO%d  %d baud, wake lead %lu ms\n",
                PANEL_TX_GPIO, PANEL_WAKE_GPIO, PANEL_BAUD,
                (unsigned long)PANEL_WAKE_MS);
  Serial.printf("[panel-tx] one DoorbellMsg (%u B) every %lu ms\n",
                (unsigned)sizeof(DoorbellMsg), (unsigned long)SEND_EVERY_MS);

  panelBeginTx();
  nextSendMs = millis() + 1000;
}

void loop() {
  const uint32_t now = millis();
  if ((int32_t)(now - nextSendMs) < 0) return;
  nextSendMs = now + SEND_EVERY_MS;

  // The §2.3.1 clamp, identical to step_io's and step1_ping_tx's.  Walking the
  // same 1,2,3,3,3 pattern is what makes C's output comparable to theirs.
  if (!locked) {
    if (pressCount < RAGE_LIMIT) pressCount++;
    if (pressCount >= RAGE_LIMIT) locked = true;
  }

  DoorbellMsg m = {};
  m.version    = PROTO_VERSION;
  m.type       = MSG_DOORBELL;
  m.pressCount = pressCount;
  m.locked     = locked ? 1 : 0;
  m.seq        = ++seq;

  const bool ok = panelSendWaking(&m, sizeof m);
  Serial.printf("[panel-tx] seq=%lu press=%u locked=%u -> %s\n",
                (unsigned long)m.seq, m.pressCount, m.locked,
                ok ? "sent" : "SEND FAILED");
}
