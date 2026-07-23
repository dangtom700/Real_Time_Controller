// Anti-Rage Doorbell — DOOR PANEL node (Arduino Nano ESP32 / ESP32-S3)
//
// Milestones implemented here (see README.md):
//   M1 doorbell core : button debounce, GPIO deep-sleep wake, 30s press window,
//                      press counter with 3-press sound lockout, ding-dong, OLED frames.
//   M2 link          : broadcasts MSG_DOORBELL over ESP-NOW to the indoor chime.
//   M3 intercom      : SCAFFOLD only — hold button-2 opens a session; INMP441 -> ESP-NOW
//                      -> MAX98357A streaming is marked TODO and not yet wired to hardware.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "protocol.h"

// ---------------- Display ----------------
static const int OLED_W = 128, OLED_H = 64;
static Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
static bool oledOK = false;

// ---------------- State ----------------
enum State { ST_ACTIVE, ST_INTERCOM };
static State    state        = ST_ACTIVE;
static uint8_t  pressCount   = 0;
static bool     locked       = false;
static uint32_t windowStart  = 0;   // timer-1 (anti-rage window)
static uint32_t lastActivity = 0;   // idle -> sleep timer
static uint32_t intercomStart = 0;  // timer-2
static uint32_t seqNo        = 0;

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// forward decls
static void handleDoorbellPress();
static void enterIntercom();
static void runIntercom(bool btn2Held);
static void goToSleep();

// ---------------- Buzzer (LEDC tone, core 2.x / 3.x safe) ----------------
static void toneInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BUZZER_GPIO, 2000, 10);
#else
  ledcSetup(0, 2000, 10);
  ledcAttachPin(BUZZER_GPIO, 0);
#endif
}
static void toneFreq(uint32_t f) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(BUZZER_GPIO, f);
#else
  ledcWriteTone(0, f);
#endif
}
static void toneOff() { toneFreq(0); }

// "ding-dong": two descending tones. Muted once the rage-lockout engages.
static void playDingDong() {
  if (locked) return;
  toneFreq(988); delay(180);   // ding (B5)
  toneFreq(784); delay(280);   // dong (G5)
  toneOff();
}

// ---------------- Display frames ----------------
static void showFrame(uint8_t count) {
  if (!oledOK) return;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  if (locked || count >= RAGE_LIMIT) {
    oled.setTextSize(2);
    oled.setCursor(16, 8);  oled.print(F("WAIT."));
    oled.setTextSize(1);
    oled.setCursor(8, 44);  oled.print(F("rage limit reached"));
  } else {
    // Placeholder "brainrot" escalation by press count (M4 swaps in real GIFs).
    oled.setTextSize(3);
    oled.setCursor(0, 4);
    for (uint8_t i = 0; i < count; i++) oled.print('!');
    oled.setTextSize(1);
    int16_t x = (int16_t)((count * 37) % (OLED_W - 8));
    oled.fillRect(x, 44, 8, 8, SSD1306_WHITE);   // bouncing block stand-in
  }
  oled.display();
}

// ---------------- ESP-NOW ----------------
#if ESP_ARDUINO_VERSION_MAJOR >= 3
static void onRecv(const esp_now_recv_info_t*, const uint8_t*, int) {
#else
static void onRecv(const uint8_t*, const uint8_t*, int) {
#endif
  // M3 SCAFFOLD: talk-back audio (MSG_VOICE_*) from the indoor node lands here.
  // Not decoded/played in v1.
}

static void broadcastDoorbell() {
  DoorbellMsg m{};
  m.version    = PROTO_VERSION;
  m.type       = MSG_DOORBELL;
  m.pressCount = pressCount;
  m.locked     = locked ? 1 : 0;
  m.seq        = ++seqNo;
  esp_now_send(BROADCAST, reinterpret_cast<const uint8_t*>(&m), sizeof(m));
}

static void espnowSetup() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed"); return; }
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

// ---------------- Deep sleep ----------------
static void goToSleep() {
  toneOff();
  if (oledOK) {
    oled.clearDisplay(); oled.display();
    oled.ssd1306_command(SSD1306_DISPLAYOFF);
  }
  // ESP32-S3 has no ext0, and this core (IDF 4.4) has no deep-sleep GPIO wake —
  // use ext1. Its only "any pin" mode here is ANY_HIGH, so the buttons are wired
  // ACTIVE-HIGH (idle LOW via external pull-down, pressed HIGH). Add EXTERNAL
  // pull-down resistors: internal pulls are not guaranteed held through deep sleep.
  uint64_t mask = (1ULL << BTN1_GPIO) | (1ULL << BTN2_GPIO);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH);
  Serial.println("Sleeping...");
  Serial.flush();
  esp_deep_sleep_start();
}

// ---------------- Doorbell / intercom logic ----------------
static void handleDoorbellPress() {
  lastActivity = millis();
  if (millis() - windowStart > RAGE_WINDOW_MS) {   // window expired -> fresh window
    pressCount = 0; locked = false; windowStart = millis();
  }
  if (pressCount < RAGE_LIMIT) pressCount++;
  if (pressCount >= RAGE_LIMIT) locked = true;     // 3rd press locks (mutes) the sound
  playDingDong();                                  // no-op while locked
  showFrame(pressCount);
  broadcastDoorbell();
  Serial.printf("Doorbell press #%u  locked=%d\n", pressCount, locked);
}

static void enterIntercom() {
  state = ST_INTERCOM;
  intercomStart = millis();
  lastActivity  = millis();
  Serial.println("[intercom] session OPEN (SCAFFOLD - voice not streamed in v1)");
  if (oledOK) {
    oled.clearDisplay(); oled.setTextSize(1); oled.setCursor(0, 0);
    oled.print(F("INTERCOM")); oled.setCursor(0, 12); oled.print(F("hold btn2 to talk"));
    oled.display();
  }
  // TODO M3: send MSG_VOICE_BEGIN, start INMP441 I2S RX at 8 kHz mono.
}

static void runIntercom(bool btn2Held) {
  lastActivity = millis();
  // TODO M3: read INMP441 samples -> pack into VoiceMsg -> esp_now_send(BROADCAST, ...).
  if (!btn2Held || millis() - intercomStart > INTERCOM_MAX_MS) {
    Serial.println("[intercom] session CLOSED");
    // TODO M3: send MSG_VOICE_END, stop I2S RX.
    state = ST_ACTIVE;
    windowStart = millis(); pressCount = 0; locked = false;
    if (oledOK) showFrame(0);
  }
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(BTN1_GPIO, INPUT_PULLDOWN);   // active-HIGH (see goToSleep / ext1 wake)
  pinMode(BTN2_GPIO, INPUT_PULLDOWN);
  toneInit();

  Wire.begin();
  oledOK = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOK) Serial.println("OLED init failed (running headless)");

  espnowSetup();

  const bool wokeByButton = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1);
  pressCount = 0; locked = false;
  windowStart = lastActivity = millis();

  if (wokeByButton) {
    Serial.println("Woke by button");
    handleDoorbellPress();      // the wake press counts as the first hit
  } else {
    Serial.println("Cold boot / power-on");
    showFrame(0);
  }
}

void loop() {
  static uint32_t b1Edge = 0; static bool b1Last = false;
  static uint32_t b2DownAt = 0; static bool b2Down = false;

  // ---- Button 1: doorbell (press edge; active-HIGH) ----
  const bool b1 = (digitalRead(BTN1_GPIO) == HIGH);
  if (b1 != b1Last && millis() - b1Edge > DEBOUNCE_MS) {
    b1Edge = millis();
    b1Last = b1;
    if (b1 && state == ST_ACTIVE) handleDoorbellPress();
  }

  // ---- Button 2: hold 2s -> intercom (active-HIGH) ----
  const bool b2 = (digitalRead(BTN2_GPIO) == HIGH);
  if (b2 && !b2Down) { b2Down = true;  b2DownAt = millis(); }
  if (!b2 && b2Down) { b2Down = false; }
  if (b2Down && state == ST_ACTIVE && millis() - b2DownAt >= PTT_HOLD_MS) {
    enterIntercom();
  }
  if (state == ST_INTERCOM) runIntercom(b2);

  // ---- idle -> deep sleep ----
  if (state == ST_ACTIVE && millis() - lastActivity > IDLE_SLEEP_MS) {
    goToSleep();
  }
}
