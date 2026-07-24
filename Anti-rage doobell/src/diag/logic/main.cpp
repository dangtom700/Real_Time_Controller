// DIAG: anti-rage state logic, no hardware needed (door panel, ESP32-S3)
// Simulate presses over the serial monitor to validate the counting/lockout/window
// timing without wiring any buttons. Uses the REAL constants from config.h.
// Run: pio run -e diag_logic -t upload -t monitor
// Then type 'p' + Enter to simulate a press. Send 3 within 30s -> lockout (muted).
//      Wait >30s idle -> window resets and unlocks.
#include <Arduino.h>
#include "config.h"

static uint8_t  count = 0;
static bool     locked = false;
static uint32_t windowStart = 0;
static bool     windowOpen = false;

static void press() {
  uint32_t now = millis();
  if (!windowOpen || now - windowStart > RAGE_WINDOW_MS) {
    count = 0; locked = false; windowStart = now; windowOpen = true;
    Serial.println("  (opened a fresh 30s window)");
  }
  if (count < RAGE_LIMIT) count++;
  if (count >= RAGE_LIMIT) locked = true;
  Serial.printf("  press -> count=%u  locked=%s  sound=%s\n",
                count, locked ? "YES" : "no", locked ? "MUTED" : "ding-dong");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("\n[logic] window=%lums  limit=%u presses\n",
                (unsigned long)RAGE_WINDOW_MS, RAGE_LIMIT);
  Serial.println("[logic] type 'p' + Enter to simulate a doorbell press.");
}

void loop() {
  if (Serial.available()) { char c = Serial.read(); if (c == 'p' || c == 'P') press(); }
  if (windowOpen && millis() - windowStart > RAGE_WINDOW_MS) {
    windowOpen = false; count = 0; locked = false;
    Serial.println("  (window expired -> count reset, unlocked)");
  }
}
