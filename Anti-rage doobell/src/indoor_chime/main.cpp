// Anti-Rage Doorbell — INDOOR CHIME node (ESP8266)
//
// The "other end" of the wall. Listens on ESP-NOW and:
//   M2 : on MSG_DOORBELL -> beep the local buzzer and log the press/lock state.
//   M3 : on MSG_VOICE_CHUNK -> play PCM through a MAX98357A over I2S (SCAFFOLD/TODO).
//
// NOTE: build requires the ESP8266 platform, which is not installed by default:
//   pio pkg install -e indoor_chime   (needs network)
// This file has therefore NOT been compiler-verified in this environment.

#include <ESP8266WiFi.h>
#include <espnow.h>
#include "config.h"
#include "protocol.h"

static void chimeBeep() {
  // ESP8266 core provides tone(); a short two-note ding-dong.
  tone(CHIME_BUZZER_PIN, 988, 180);
  delay(200);
  tone(CHIME_BUZZER_PIN, 784, 280);
  delay(300);
  noTone(CHIME_BUZZER_PIN);
}

// ESP8266 ESP-NOW receive callback signature.
static void onRecv(uint8_t* /*mac*/, uint8_t* data, uint8_t len) {
  if (len < 2 || data[0] != PROTO_VERSION) return;
  const uint8_t type = data[1];

  if (type == MSG_DOORBELL && len >= (uint8_t)sizeof(DoorbellMsg)) {
    DoorbellMsg m;
    memcpy(&m, data, sizeof(m));
    Serial.printf("[chime] DOORBELL press#%u locked=%u seq=%lu\n",
                  m.pressCount, m.locked, (unsigned long)m.seq);
    chimeBeep();
  } else if (type == MSG_VOICE_CHUNK) {
    // TODO M3: memcpy VoiceMsg, write pcm[] to MAX98357A via i2s_write_sample().
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);
  pinMode(CHIME_BUZZER_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_set_channel(ESPNOW_CHANNEL);            // must match the door panel

  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);   // receive-only for M2
  esp_now_register_recv_cb(onRecv);
  Serial.println("Indoor chime ready");
}

void loop() {
  // Event-driven: everything happens in onRecv().
}
