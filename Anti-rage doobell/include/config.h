#pragma once
// Shared configuration for both nodes of the Anti-Rage Doorbell.
// See README.md for the full spec and the state machine these values drive.

// ---- Timing (milliseconds) ----
static const uint32_t RAGE_WINDOW_MS  = 30000; // timer-1: anti-rage press window
static const uint8_t  RAGE_LIMIT      = 3;     // presses before the sound locks out
static const uint32_t IDLE_SLEEP_MS   = 30000; // idle time before deep sleep
static const uint32_t DEBOUNCE_MS     = 40;    // button debounce
static const uint32_t PTT_HOLD_MS     = 2000;  // timer-2: hold button-2 this long to talk
static const uint32_t INTERCOM_MAX_MS = 15000; // intercom auto-timeout safety cap

// ---- ESP-NOW ----
// Both nodes MUST use the same channel. ESP32-S3 <-> ESP8266 interop needs it fixed.
#define ESPNOW_CHANNEL 1

// ============================================================================
//  Door panel — Arduino Nano ESP32 (ESP32-S3)
//  These are RAW GPIO numbers. The build forces "By GPIO number" mode
//  (-DBOARD_USES_HW_GPIO_NUMBERS in platformio.ini) so pinMode/digitalRead AND the
//  ext1 deep-sleep mask all speak raw GPIO. The comment after each line is the
//  matching Nano ESP32 silkscreen label — wire to THAT physical header pin.
// ============================================================================
#define BTN1_GPIO    5   // silk D2  — doorbell button (active-HIGH, RTC-capable -> wakes)
#define BTN2_GPIO    6   // silk D3  — intercom button (active-HIGH, RTC-capable -> wakes)
#define BUZZER_GPIO  7   // silk D4  — passive buzzer (v1 ding-dong via LEDC tone)
// OLED SSD1306 rides the default I2C bus (Wire): SDA = GPIO11 (silk A4), SCL = GPIO12 (A5), addr 0x3C.

// INMP441 mic (I2S RX) — milestone 3 (voice intercom).
#define MIC_SD_GPIO    8   // silk D5
#define MIC_WS_GPIO    9   // silk D6
#define MIC_SCK_GPIO   10  // silk D7
// MAX98357A amp (I2S TX) — milestone 3. Moved clear of GPIO11/12 (the OLED's I2C pins).
#define AMP_DIN_GPIO   21  // silk D10
#define AMP_LRC_GPIO   18  // silk D9
#define AMP_BCLK_GPIO  17  // silk D8

// ============================================================================
//  Indoor chime — ESP8266
// ============================================================================
#define CHIME_BUZZER_PIN 14  // GPIO14 = D5. ESP8266 I2S-out pins are fixed (M3).
