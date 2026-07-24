// DIAG: deep sleep + ext1 button wake (door panel, ESP32-S3)
// The S3 has NO ext0 and no deep-sleep GPIO wake on this core -> we use ext1 ANY_HIGH,
// which is exactly why the buttons must be ACTIVE-HIGH.
// Run: pio run -e diag_sleep_wake -t upload -t monitor
// PASS: boots, waits 5s, sleeps. A button press reboots it with cause=EXT1 and boot# increments.
#include <Arduino.h>
#include <esp_sleep.h>
#include "config.h"

RTC_DATA_ATTR int bootCount = 0;   // survives deep sleep

void setup() {
  Serial.begin(115200);
  delay(400);
  bootCount++;
  esp_sleep_wakeup_cause_t c = esp_sleep_get_wakeup_cause();
  Serial.printf("\n[sleep] boot #%d  wake-cause=%d %s\n", bootCount, (int)c,
                c == ESP_SLEEP_WAKEUP_EXT1 ? "<- EXT1 button, GOOD" : "<- power-on/reset");

  pinMode(BTN1_GPIO, INPUT_PULLDOWN);
  pinMode(BTN2_GPIO, INPUT_PULLDOWN);

  Serial.println("[sleep] going to deep sleep in 5s - press BTN1 or BTN2 to wake");
  delay(5000);

  uint64_t mask = (1ULL << BTN1_GPIO) | (1ULL << BTN2_GPIO);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH);
  Serial.println("[sleep] sleeping now.");
  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {}
