// DIAG: ESP-NOW transmitter (door panel, ESP32-S3)
// Prints this board's MAC + channel, then broadcasts a counter once per second and
// reports the per-packet send status (this is your reliability check, not just on/off).
// Pair with diag_espnow_rx_led on the ESP8266.
// Run: pio run -e diag_espnow_tx -t upload -t monitor
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "config.h"

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint32_t sent = 0, ok = 0;

// core 2.x send-callback signature
static void onSent(const uint8_t*, esp_now_send_status_t s) {
  if (s == ESP_NOW_SEND_SUCCESS) ok++;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.printf("\n[tx] MAC=%s  channel=%d\n", WiFi.macAddress().c_str(), ESPNOW_CHANNEL);
  if (esp_now_init() != ESP_OK) { Serial.println("[tx] esp_now_init FAILED"); return; }
  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, BROADCAST, 6);
  p.channel = ESPNOW_CHANNEL;
  p.encrypt = false;
  esp_now_add_peer(&p);
  Serial.println("[tx] broadcasting counter every 1s...");
}

void loop() {
  uint32_t payload = ++sent;
  esp_now_send(BROADCAST, (const uint8_t*)&payload, sizeof(payload));
  delay(1000);
  Serial.printf("[tx] sent=%lu  acked=%lu (%.0f%%)\n",
                (unsigned long)sent, (unsigned long)ok, sent ? 100.0 * ok / sent : 0);
}
