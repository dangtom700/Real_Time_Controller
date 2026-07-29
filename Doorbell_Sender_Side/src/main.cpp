// Anti-Rage Doorbell — SENDER SIDE ("door panel") on an Arduino Mega 2560.
//
// PURPOSE
//   Bench rig. This proves the sender's components and its state machine on
//   known-good 5 V hardware before the design is compacted onto an ESP32-S3.
//   Everything here except the radio and deep sleep is the real firmware.
//
// UNDER TEST
//   button 1 (doorbell) · button 2 (intercom PTT) · 2.2" 240x320 ILI9341 TFT ·
//   passive buzzer · the 30 s anti-rage window · the 3-press sound lockout ·
//   the DoorbellMsg / VoiceMsg wire format.
//
// NOT UNDER TEST (deliberately — see Doorbell_Shared/README.md)
//   The radio: there isn't one. Packets go out over serial as '#' frames for
//   Doorbell_Receiver_Side to consume. Deep sleep: AVR power-down and the S3's
//   ext1 wake share no semantics, so proving one proves nothing about the other.
//   Idle is therefore a logged SIM_SLEEP state, not a real power-down.
//
// DISPLAY WIRING
//   This breakout is marked "VIN: 3.3-5V, LOGIC: 3.3-5V" on the back - it has its
//   own regulator and level shifters, so the Mega's 5 V signals connect straight
//   through with no dividers. A bare ILI9341 panel WITHOUT that marking is 3.3 V
//   logic only and would need them; read the back of the board, not this comment.
//
// RUN
//   pio run -t upload -t monitor
// PASS
//   Press button 1 -> ding-dong, the count goes green, one '#01 01 ..' frame per
//   press. Third press inside 30 s -> silence + a red WAIT screen + locked=1 in
//   the frame. Hold button 2 for 2 s -> a VOICE_BEGIN frame, chunk samples, then
//   VOICE_END on release.

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <stdlib.h>
#include "config_mega.h"
#include "protocol.h"
#include "sim_link.h"     // TX only; LINK_ENABLE_RX intentionally not defined
#include "synth.h"

static Adafruit_ILI9341 tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

// ---------------- State ----------------
enum State { ST_ACTIVE, ST_INTERCOM, ST_SIM_SLEEP };
static State    state         = ST_ACTIVE;
static uint8_t  pressCount    = 0;
static bool     locked        = false;
static uint32_t windowStart   = 0;   // timer-1 (anti-rage window)
static uint32_t lastActivity  = 0;   // idle -> simulated sleep
static uint32_t intercomStart = 0;   // timer-2
static uint32_t seqNo         = 0;

// Voice session bookkeeping.
static uint16_t voiceSeq         = 0;
static uint32_t voiceNextChunkAt = 0;
static uint16_t voicePhase       = 0;
static uint16_t voicePhaseInc    = 0;

// Serial cannot carry 8 kHz audio, so only the first few chunks are framed for
// copy-paste; the rest are counted so the REQUIRED link rate is still reported.
static const uint8_t VOICE_FRAMES_EMITTED = 3;
static uint16_t voiceChunksSuppressed = 0;

// One VoiceMsg carries VOICE_SAMPLES samples, so a chunk is due every
// VOICE_SAMPLES / VOICE_SAMPLE_RATE seconds = 13.75 ms at 110 / 8000.
static const uint16_t VOICE_CHUNK_MS = (uint16_t)((1000UL * VOICE_SAMPLES) / VOICE_SAMPLE_RATE);

// ---------------- Buzzer ----------------
// tone() owns Timer2 on the Mega. Timer0 (millis) is untouched.
static void playDingDong() {
  if (locked) return;                       // the rage lockout mutes the sound
  tone(TX_BUZZER_PIN, 988); delay(180);     // ding (B5)
  tone(TX_BUZZER_PIN, 784); delay(280);     // dong (G5)
  noTone(TX_BUZZER_PIN);
}

// ---------------- Display ----------------
// The ILI9341 draws straight to the panel - there is no frame buffer, so none of
// the Mega's 8 KB of SRAM is spent on the screen. The trade is that a full
// fillScreen() pushes 240*320*16 bits down the SPI bus, ~0.15 s at TFT_SPI_HZ.
// That is why only state CHANGES repaint, never the loop.

// The breakout documents its backlight pin as PWM-safe, so sleep/wake ramps it
// instead of snapping it off. D6 is Timer4, which nothing else here uses.
static void backlightFade(uint8_t from, uint8_t to) {
  const int16_t step = (to > from) ? 8 : -8;
  int16_t v = from;
  while ((step > 0) ? (v < to) : (v > to)) {
    analogWrite(TFT_BL_PIN, (uint8_t)v);
    delay(12);
    v += step;
  }
  analogWrite(TFT_BL_PIN, to);
}

static void tftCenter(const __FlashStringHelper *s, uint8_t size, int16_t y, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((TFT_W - (int16_t)w) / 2, y);
  tft.print(s);
}

static void tftCenter(const char *s, uint8_t size, int16_t y, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  tft.setTextSize(size);
  tft.setTextColor(color);
  tft.getTextBounds(s, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((TFT_W - (int16_t)w) / 2, y);
  tft.print(s);
}

// Escalation by press count - the colour IS the "brainrot" cue for v1.
static uint16_t escalationColor(uint8_t count) {
  if (count <= 1) return ILI9341_GREEN;
  if (count == 2) return ILI9341_ORANGE;
  return ILI9341_RED;
}

// A row of RAGE_LIMIT pips: filled = presses spent, outlined = presses left.
static void drawPips(uint8_t count) {
  const int16_t pw = 56, ph = 20, gap = 14;
  const int16_t total = RAGE_LIMIT * pw + (RAGE_LIMIT - 1) * gap;
  int16_t x = (TFT_W - total) / 2;
  const int16_t y = 236;
  for (uint8_t i = 0; i < RAGE_LIMIT; i++) {
    if (i < count) tft.fillRect(x, y, pw, ph, escalationColor((uint8_t)(i + 1)));
    else           tft.drawRect(x, y, pw, ph, ILI9341_DARKGREY);
    x += pw + gap;
  }
}

static void showFrame(uint8_t count) {
  if (locked || count >= RAGE_LIMIT) {
    tft.fillScreen(ILI9341_RED);
    tftCenter(F("WAIT."), 6, 96, ILI9341_WHITE);
    tftCenter(F("rage limit reached"), 2, 176, ILI9341_YELLOW);
    drawPips(RAGE_LIMIT);
    return;
  }

  tft.fillScreen(ILI9341_BLACK);
  if (count == 0) {
    tftCenter(F("READY"), 4, 130, ILI9341_DARKGREY);
    drawPips(0);
    return;
  }

  tftCenter(F("DING DONG"), 3, 40, ILI9341_CYAN);
  char n[4];
  itoa(count, n, 10);
  tftCenter(n, 9, 120, escalationColor(count));
  drawPips(count);
}

static void showBanner(const __FlashStringHelper *l1, const __FlashStringHelper *l2,
                       uint16_t bg, uint16_t fg) {
  tft.fillScreen(bg);
  tftCenter(l1, 4, 120, fg);
  tftCenter(l2, 2, 180, ILI9341_WHITE);
}

// ---------------- Doorbell ----------------
static void sendDoorbell() {
  DoorbellMsg m;
  m.version    = PROTO_VERSION;
  m.type       = MSG_DOORBELL;
  m.pressCount = pressCount;
  m.locked     = locked ? 1 : 0;
  m.seq        = ++seqNo;
  linkSend(&m, sizeof m);
}

static void handleDoorbellPress() {
  lastActivity = millis();
  if (millis() - windowStart > RAGE_WINDOW_MS) {   // window expired -> fresh window
    pressCount = 0; locked = false; windowStart = millis();
  }
  if (pressCount < RAGE_LIMIT) pressCount++;
  if (pressCount >= RAGE_LIMIT) locked = true;     // 3rd press mutes the sound

  playDingDong();                                  // no-op while locked
  showFrame(pressCount);
  sendDoorbell();

  Serial.print(F("[tx] doorbell press #"));
  Serial.print(pressCount);
  Serial.print(F("  locked="));
  Serial.print(locked ? 1 : 0);
  Serial.print(F("  seq="));
  Serial.println(seqNo);
}

// ---------------- Intercom (push to talk) ----------------
static void sendVoiceControl(uint8_t type) {
  // BEGIN/END carry no audio; send only the header so the frame stays short.
  VoiceMsg m;
  m.version = PROTO_VERSION;
  m.type    = type;
  m.seq     = voiceSeq;
  linkSend(&m, (uint8_t)(sizeof(m) - sizeof(m.pcm)));
}

static void enterIntercom() {
  state                 = ST_INTERCOM;
  intercomStart         = millis();
  lastActivity          = millis();
  voiceSeq              = 0;
  voiceChunksSuppressed = 0;
  voicePhase            = 0;
  voicePhaseInc         = synthPhaseInc(440, VOICE_SAMPLE_RATE);   // 440 Hz stand-in
  voiceNextChunkAt      = millis();

  noTone(TX_BUZZER_PIN);
  Serial.println(F("[tx] intercom OPEN - no mic on this bench, sending a 440 Hz sine"));
  showBanner(F("INTERCOM"), F("hold btn2 to talk"), ILI9341_NAVY, ILI9341_CYAN);
  sendVoiceControl(MSG_VOICE_BEGIN);
}

static void streamVoiceChunk() {
  VoiceMsg m;
  m.version = PROTO_VERSION;
  m.type    = MSG_VOICE_CHUNK;
  m.seq     = ++voiceSeq;
  for (uint8_t i = 0; i < VOICE_SAMPLES; i++) {
    m.pcm[i] = synthNext16(voicePhase, voicePhaseInc);
  }

  if (voiceSeq <= VOICE_FRAMES_EMITTED) {
    linkSend(&m, sizeof m);          // real, paste-able frames
  } else {
    voiceChunksSuppressed++;         // generated on schedule, just not printed
  }
}

static void exitIntercom(const __FlashStringHelper *why) {
  sendVoiceControl(MSG_VOICE_END);

  // The number that actually matters for the ESP-NOW port: sustained packet rate
  // and bitrate. TESTING.md flags this as the one risk with no script yet.
  const uint16_t pps = (uint16_t)(1000U / VOICE_CHUNK_MS);
  const uint32_t bps = (uint32_t)pps * sizeof(VoiceMsg) * 8UL;
  Serial.print(F("[tx] intercom CLOSED ("));
  Serial.print(why);
  Serial.print(F(") chunks="));
  Serial.print(voiceSeq);
  Serial.print(F(" suppressed="));
  Serial.println(voiceChunksSuppressed);
  Serial.print(F("[tx] link must sustain ~"));
  Serial.print(pps);
  Serial.print(F(" pkt/s x "));
  Serial.print((unsigned)sizeof(VoiceMsg));
  Serial.print(F(" B = ~"));
  Serial.print(bps / 1000UL);
  Serial.println(F(" kbps"));

  state       = ST_ACTIVE;
  windowStart = millis();
  pressCount  = 0;
  locked      = false;
  showFrame(0);
}

static void runIntercom(bool btn2Held) {
  lastActivity = millis();

  // Pace chunks at the true audio rate so the reported figures are honest.
  while ((int32_t)(millis() - voiceNextChunkAt) >= 0) {
    streamVoiceChunk();
    voiceNextChunkAt += VOICE_CHUNK_MS;
  }

  if (!btn2Held)                                       exitIntercom(F("released"));
  else if (millis() - intercomStart > INTERCOM_MAX_MS) exitIntercom(F("timeout"));
}

// ---------------- Simulated sleep ----------------
// The real node deep-sleeps and wakes on ext1. An AVR power-down would prove
// nothing about that, and it drops the USB serial the bench depends on, so idle
// is modelled instead: backlight off, screen black, sound off. The first press
// re-arms and counts, exactly like the ESP32 node's wake press.
static void enterSimSleep() {
  noTone(TX_BUZZER_PIN);
  backlightFade(TFT_BL_ON, 0);
  tft.fillScreen(ILI9341_BLACK);
  state = ST_SIM_SLEEP;
  Serial.println(F("[tx] SIM_SLEEP (ESP32 would deep-sleep here; press a button to wake)"));
}

static void wakeFromSimSleep() {
  backlightFade(0, TFT_BL_ON);
  state       = ST_ACTIVE;
  pressCount  = 0;
  locked      = false;
  windowStart = lastActivity = millis();
  Serial.println(F("[tx] woke by button"));
  handleDoorbellPress();          // the wake press counts, same as the ESP32 node
}

// ---------------- Arduino entry points ----------------
void setup() {
  Serial.begin(115200);
  delay(50);

  pinMode(TX_BTN1_PIN, BTN_PIN_MODE);
  pinMode(TX_BTN2_PIN, BTN_PIN_MODE);
  pinMode(TX_BUZZER_PIN, OUTPUT);

  // The Mega reverts to SPI slave if its hardware SS (D53) is left an input,
  // even though we drive CS ourselves. Pin it high and forget about it.
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);

  // The microSD shares this SPI bus. An unselected card with a floating CS will
  // happily drive MISO and corrupt the display's traffic - park it deselected.
  pinMode(TFT_SD_CS_PIN, OUTPUT);
  digitalWrite(TFT_SD_CS_PIN, HIGH);

  pinMode(TFT_BL_PIN, OUTPUT);
  analogWrite(TFT_BL_PIN, TFT_BL_ON);

  tft.begin(TFT_SPI_HZ);
  tft.setRotation(TFT_ROTATION);

  // The ILI9341 is effectively write-only in this wiring, so there is no honest
  // "is it there?" probe - a missing panel simply swallows the SPI writes. The
  // self-diagnostic below reads 0xC0 on a healthy display IF MISO is connected;
  // anything else means MISO is unwired or the panel is not responding. It is
  // information, not a gate.
  Serial.print(F("[tx] ILI9341 self-diag = 0x"));
  Serial.print(tft.readcommand8(ILI9341_RDSELFDIAG), HEX);
  Serial.println(F("  (0xC0 = healthy; 0x00 = MISO unwired or no panel)"));

  windowStart = lastActivity = millis();

  Serial.println(F("[tx] SENDER bench ready (Mega 2560)"));
  Serial.println(F("[tx] btn1=doorbell  btn2=hold 2s for intercom"));
  Serial.println(F("[tx] lines starting with '#' are packets - paste them into the receiver"));
  showFrame(0);
}

void loop() {
  static uint32_t b1Edge = 0;   static bool b1Last = false;
  static uint32_t b2DownAt = 0; static bool b2Down = false;

  const bool b1 = (digitalRead(TX_BTN1_PIN) == BTN_ACTIVE_LEVEL);
  const bool b2 = (digitalRead(TX_BTN2_PIN) == BTN_ACTIVE_LEVEL);

  // ---- wake first: in SIM_SLEEP either button re-arms the node ----
  if (state == ST_SIM_SLEEP) {
    if (b1 || b2) {
      b1Last = b1; b1Edge   = millis();
      b2Down = b2; b2DownAt = millis();
      wakeFromSimSleep();
    }
    return;
  }

  // ---- Button 1: doorbell, on the press edge ----
  if (b1 != b1Last && millis() - b1Edge > DEBOUNCE_MS) {
    b1Edge = millis();
    b1Last = b1;
    if (b1 && state == ST_ACTIVE) handleDoorbellPress();
  }

  // ---- Button 2: hold PTT_HOLD_MS to open the intercom ----
  if (b2 && !b2Down) { b2Down = true;  b2DownAt = millis(); }
  if (!b2 && b2Down) { b2Down = false; }
  if (b2Down && state == ST_ACTIVE && millis() - b2DownAt >= PTT_HOLD_MS) {
    enterIntercom();
  }
  if (state == ST_INTERCOM) runIntercom(b2);

  // ---- idle -> simulated sleep ----
  if (state == ST_ACTIVE && millis() - lastActivity > IDLE_SLEEP_MS) {
    enterSimSleep();
  }
}
