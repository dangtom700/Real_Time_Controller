// DIAG: passive buzzer on Arduino Mega 2560 — bench check of the buzzer part itself,
// independent of the ESP32. Same tune as diag_buzzer so the two are directly comparable.
// Run: pio run -e diag_buzzer_mega -t upload -t monitor
// PASS: repeating "ding-dong" then a rising frequency sweep. Silence -> check buzzer +/GND on D8.
#include <Arduino.h>
#include "config.h"

void setup() {
  Serial.begin(115200);
  pinMode(MEGA_BUZZER_PIN, OUTPUT);
  Serial.println("[buzzer] D8: ding-dong + sweep loop");
}

void loop() {
  tone(MEGA_BUZZER_PIN, 988); delay(180);   // ding
  tone(MEGA_BUZZER_PIN, 784); delay(280);   // dong
  noTone(MEGA_BUZZER_PIN);    delay(600);
  for (uint16_t f = 400; f <= 3000; f += 50) { tone(MEGA_BUZZER_PIN, f); delay(12); }
  noTone(MEGA_BUZZER_PIN);
  delay(1500);
}
