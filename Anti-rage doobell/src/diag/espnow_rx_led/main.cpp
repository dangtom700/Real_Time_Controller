// DIAG: ESP-NOW receiver -> LED (indoor chime, ESP8266)
// This is your "LED responds to ESP-NOW" test. Pair with diag_espnow_tx on the door panel.
// Run: pio run -e diag_espnow_rx_led -t upload -t monitor
// PASS: onboard LED blinks and serial prints each packet + sender MAC.
//       Nothing -> boards on different channels (both must be ESPNOW_CHANNEL).
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "config.h"

// onboard LED is GPIO2 on most NodeMCU boards, and ACTIVE-LOW.
static const uint8_t LED = LED_BUILTIN;

static void onRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  digitalWrite(LED, LOW);        // LED on
  Serial.printf("[rx] %u bytes from %02X:%02X:%02X:%02X:%02X:%02X\n",
                len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  delay(60);
  digitalWrite(LED, HIGH);       // LED off
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.printf("\n[rx] MAC=%s\n", WiFi.macAddress().c_str());
  wifi_set_channel(ESPNOW_CHANNEL);           // MUST match the transmitter
  if (esp_now_init() != 0) { Serial.println("[rx] esp_now_init FAILED"); return; }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(onRecv);
  Serial.printf("[rx] listening on channel %d ...\n", ESPNOW_CHANNEL);
}

void loop() {}
