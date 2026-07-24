// DIAG: buzzer (indoor chime, ESP8266)
// Run: pio run -e diag_chime_buzzer -t upload -t monitor
// PASS: repeating ding-dong from the chime buzzer on GPIO14(D5).
#include <Arduino.h>
#include "config.h"

void setup() {
  Serial.begin(115200);
  pinMode(CHIME_BUZZER_PIN, OUTPUT);
  Serial.println("[chime] buzzer test on GPIO14(D5)");
}

void loop() {
  tone(CHIME_BUZZER_PIN, 988, 180); delay(220);
  tone(CHIME_BUZZER_PIN, 784, 280); delay(320);
  noTone(CHIME_BUZZER_PIN);
  delay(1200);
}
