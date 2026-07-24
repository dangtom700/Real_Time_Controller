// DIAG: passive buzzer via LEDC (door panel, ESP32-S3)
// Run: pio run -e diag_buzzer -t upload -t monitor
// PASS: repeating "ding-dong" then a rising frequency sweep. Silence -> check buzzer +/GND on GPIO7(D4).
#include <Arduino.h>
#include "config.h"

static void toneHz(uint32_t f) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(BUZZER_GPIO, f);
#else
  ledcWriteTone(0, f);
#endif
}

void setup() {
  Serial.begin(115200);
  delay(300);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BUZZER_GPIO, 2000, 10);
#else
  ledcSetup(0, 2000, 10);
  ledcAttachPin(BUZZER_GPIO, 0);
#endif
  Serial.println("[buzzer] GPIO7(D4): ding-dong + sweep loop");
}

void loop() {
  toneHz(988); delay(180);            // ding
  toneHz(784); delay(280);            // dong
  toneHz(0);   delay(600);
  for (uint32_t f = 400; f <= 3000; f += 50) { toneHz(f); delay(12); }
  toneHz(0);
  delay(1500);
}
