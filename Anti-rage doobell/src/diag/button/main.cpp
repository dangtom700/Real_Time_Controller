// DIAG: button read (door panel, ESP32-S3)
// Confirms BTN1/BTN2 are wired ACTIVE-HIGH and debounce works.
// Run: pio run -e diag_button -t upload -t monitor
// PASS: pressing a button prints "PRESS", releasing prints "release".
//       If it reads PRESS while untouched -> missing/backwards pull-down.
#include <Arduino.h>
#include "config.h"

static uint32_t e1 = 0, e2 = 0;
static bool s1 = false, s2 = false;

void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(BTN1_GPIO, INPUT_PULLDOWN);
  pinMode(BTN2_GPIO, INPUT_PULLDOWN);
  Serial.println("\n[button] BTN1=GPIO5(D2)  BTN2=GPIO6(D3). Expect LOW when idle, HIGH when pressed.");
}

void loop() {
  bool b1 = (digitalRead(BTN1_GPIO) == HIGH);
  if (b1 != s1 && millis() - e1 > 30) { e1 = millis(); s1 = b1; Serial.printf("[BTN1] %s\n", b1 ? "PRESS" : "release"); }
  bool b2 = (digitalRead(BTN2_GPIO) == HIGH);
  if (b2 != s2 && millis() - e2 > 30) { e2 = millis(); s2 = b2; Serial.printf("[BTN2] %s\n", b2 ? "PRESS" : "release"); }
}
