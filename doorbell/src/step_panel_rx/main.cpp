// STEP PANEL (receiver) — Arduino Nano ESP32, board C.  OLED board's buzzer.
//
//   pio run -e step_panel_rx -t upload --upload-port COMx
//   Uploads over DFU: double-tap RESET first, and the COM port changes.
//
// Pairs with step_panel_tx on board A.  Three wires:
//
//     A IO0   ──> C D2 (GPIO5)    message, 115200 8N1
//     A IO10  ──> C D3 (GPIO6)    wake ── 10 kΩ ── GND
//     A GND   ─── C GND
//
// PIN NUMBERING TRAP: this board's manifest sets BOARD_USES_HW_GPIO_NUMBERS,
// which turns OFF the Arduino pin remap.  D4 is GPIO7, D3 is GPIO6, D2 is
// GPIO5.  Use the Dx symbols (config_v1.h defines PANEL_C_*), never literals —
// digitalWrite(4, ...) drives a pin that is not the one silkscreened D4, and
// the buzzer stays quiet for a reason that has nothing to do with the buzzer.
//
// THE REPORT IS LAYERED ON PURPOSE.  The amp bring-up wasted four sketches
// because "no output" was read as one fault when it could have been several,
// so this separates the layers and prints all of them every second:
//
//     wake     the wake line's level, read directly.  Proves that wire alone.
//     raw      bytes seen on RX.  Non-zero means the UART wire and baud are
//              right, even if nothing decodes.
//     frames   payloads that passed sync + length + CRC.
//     crc      frames that synced and then failed the check — wiring noise,
//              wrong baud, or a half-sent frame.
//
// raw=0 is a wiring or baud fault.  raw climbing with frames=0 is a framing or
// baud-mismatch fault.  frames climbing with the buzzer silent is the buzzer.
// Those are three different problems and this tells them apart without a
// second sketch.
//
// PASS: a boot self-test beep, then one burst per message with pressCount
//       beeps, walking 1, 2, 3 and then holding at 3 with the locked tone.

#include <Arduino.h>
#include "driver/gpio.h"
#include "config_v1.h"
#include "protocol.h"
#include "panel_link.h"

static bool     buzzerReady = false;
static uint32_t framesOk = 0, badHeader = 0, lastReportMs = 0;

// Not tone().  That routes through a FreeRTOS task whose only failure report is
// log_e(), which is compiled out at the default log level — so a failed attach
// gives silence and no message, which looks exactly like a wiring fault.  Drive
// LEDC directly and print whether the attach worked.  Same reasoning as
// step_io, and the same reason the buzzer's move to this board was provable.
static void buzzerBegin() {
  // A magnetic element wants 30-50 mA, past a GPIO's default 20 mA.  CAP_3
  // raises it to roughly 40 mA.  A piezo is capacitive and does not care.
  gpio_set_drive_capability((gpio_num_t)PANEL_C_BUZZ_PIN, GPIO_DRIVE_CAP_3);
  buzzerReady = ledcAttach(PANEL_C_BUZZ_PIN, 2000, 10);
  Serial.printf("[panel-rx] buzzer LEDC attach on D4 (GPIO%u): %s\n",
                (unsigned)PANEL_C_BUZZ_PIN, buzzerReady ? "OK" : "FAILED");
}

static void beep(uint16_t hz, uint16_t ms) {
  if (!buzzerReady) return;
  ledcWriteTone(PANEL_C_BUZZ_PIN, hz);
  delay(ms);
  ledcWriteTone(PANEL_C_BUZZ_PIN, 0);
}

// Make the MESSAGE CONTENT audible, not just its arrival.  One beep per press
// means a decode fault sounds different from a link fault, and the rage clamp
// can be heard holding at three without reading the log.
static void ringFor(const DoorbellMsg &m) {
  for (uint8_t i = 0; i < m.pressCount && i < RAGE_LIMIT; i++) {
    beep(1320, 90);
    delay(70);
  }
  if (m.locked) beep(440, 250);      // the lockout, deliberately a lower note
}

void setup() {
  Serial.begin(115200);
  delay(1500);            // native USB CDC: give the host time to attach

  Serial.println("\n[panel-rx] board C — panel link receiver + buzzer");
  Serial.printf("[panel-rx] RX=D2(GPIO%u)  WAKE=D3(GPIO%u)  BUZZ=D4(GPIO%u)  %d baud\n",
                (unsigned)PANEL_C_RX_PIN, (unsigned)PANEL_C_WAKE_PIN,
                (unsigned)PANEL_C_BUZZ_PIN, PANEL_BAUD);

  buzzerBegin();
  panelBeginRx();

  // Ring once at boot, before any message can have arrived.  If this is silent
  // the buzzer is the fault and nothing about the link is worth reading yet —
  // which is the check the amp bring-up did not have and should have.
  Serial.println("[panel-rx] self-test beep");
  beep(880, 120);

  lastReportMs = millis();
}

void loop() {
  uint8_t buf[PANEL_MAX_PAYLOAD];
  uint8_t n;
  while ((n = panelPoll(buf, sizeof buf)) != 0) {
    if (n != sizeof(DoorbellMsg)) { badHeader++; continue; }

    DoorbellMsg m;
    memcpy(&m, buf, sizeof m);
    if (m.version != PROTO_VERSION || m.type != MSG_DOORBELL) { badHeader++; continue; }

    framesOk++;
    Serial.printf("[panel-rx] seq=%lu press=%u locked=%u\n",
                  (unsigned long)m.seq, m.pressCount, m.locked);
    ringFor(m);
  }

  const uint32_t now = millis();
  if (now - lastReportMs >= 1000) {
    lastReportMs = now;
    Serial.printf("[panel-rx] wake=%d  raw=%lu  frames=%lu  crc=%lu  bad=%lu\n",
                  digitalRead(PANEL_C_WAKE_PIN),
                  (unsigned long)panelRawBytes, (unsigned long)framesOk,
                  (unsigned long)panelErrCrc, (unsigned long)badHeader);
  }
}
