#pragma once
#include <stdint.h>
// Pin map + timing for the Arduino Mega 2560 BENCH RIG.
//
// One Mega stands in for both nodes of the Anti-Rage Doorbell so every component
// can be proven on known-good 5 V hardware before the design is compacted onto an
// ESP32. Flash ONE side at a time (you only have one board):
//
//     Doorbell_Sender_Side    buttons + OLED + buzzer + anti-rage state machine
//     Doorbell_Receiver_Side  buzzer + LM386/speaker + 2 status LEDs
//
// Timing constants are copied verbatim from "Anti-rage doobell/include/config.h"
// so the state machine behaves identically on both platforms.

// ---- Timing (milliseconds) — same values as the ESP32 build ----
static const uint32_t RAGE_WINDOW_MS  = 30000; // timer-1: anti-rage press window
static const uint8_t  RAGE_LIMIT      = 3;     // presses before the sound locks out
static const uint32_t IDLE_SLEEP_MS   = 30000; // idle time before (simulated) sleep
static const uint32_t DEBOUNCE_MS     = 40;    // button debounce
static const uint32_t PTT_HOLD_MS     = 2000;  // timer-2: hold button-2 this long to talk
static const uint32_t INTERCOM_MAX_MS = 15000; // intercom auto-timeout safety cap

// ============================================================================
//  SENDER side — "door panel" role on the Mega
// ============================================================================
#define TX_BTN1_PIN     2    // doorbell button
#define TX_BTN2_PIN     3    // intercom push-to-talk button
#define TX_BUZZER_PIN   8    // passive buzzer, local ding-dong feedback

// ---- 2.2" 240x320 TFT, ILI9341, hardware SPI ----
// The Mega's SPI is fixed: SCK = D52, MOSI = D51, MISO = D50, hardware SS = D53.
// Only the control lines below are ours to choose. They are kept clear of the
// receiver sketch's pins (8, 11, 22, 24) so the whole bench can stay wired while
// you flash one side or the other.
//
// This breakout is marked "VIN: 3.3-5V, LOGIC: 3.3-5V" on the back, i.e. it
// carries its own regulator AND level shifters. Wire the Mega's 5 V signals
// straight through - no dividers. (Bare ILI9341 panels without that marking are
// 3.3 V logic only and WILL be damaged by 5 V; always read the back of the board.)
#define TFT_CS_PIN      10
#define TFT_DC_PIN      9
#define TFT_RST_PIN     7
#define TFT_SD_CS_PIN   4    // microSD on the same breakout (M4 GIF playback)

// Backlight. The board documents this pin as PWM-safe, so D6 (Timer4 - free in
// this sketch; tone() is on Timer2 and millis() on Timer0) drives it with
// analogWrite for dimming rather than a hard on/off.
#define TFT_BL_PIN      6
#define TFT_BL_ON       255  // full brightness; lower it if the panel is dazzling

// Portrait 240x320 suits a door panel. Set to 1 for 320x240 landscape.
#define TFT_ROTATION    0
#define TFT_W           240
#define TFT_H           320

// 8 MHz is the Mega's ceiling (F_CPU/2) and the on-board level shifters drive
// clean edges, so there is no reason to run slower. If you ever see torn or
// speckled pixels - long jumper wires, a breadboard with poor contacts - drop to
// 4000000 before suspecting anything else.
#define TFT_SPI_HZ      8000000UL

// Buttons are wired ACTIVE-LOW here: one leg to the pin, one to GND, and the
// AVR's internal pull-up does the rest. No external resistors on the bench.
//
// !! PORTING NOTE !! The ESP32-S3 node is the opposite — active-HIGH with
// EXTERNAL pull-downs, because ext1 deep-sleep wake only offers ANY_HIGH. That
// is a wiring change, not a logic change: flip the two macros below and the
// firmware is correct on either board.
#define BTN_PIN_MODE     INPUT_PULLUP
#define BTN_ACTIVE_LEVEL LOW

// ============================================================================
//  RECEIVER side — "indoor chime" role on the Mega
// ============================================================================
#define RX_BUZZER_PIN   8    // passive buzzer, the v1 ding-dong

// LM386 amplifier -> 8 ohm 1 W speaker. This is the bench stand-in for the
// MAX98357A I2S DAC on the real chime node. Pin 11 is OC1A, so Timer1 can drive
// it as a 62.5 kHz PWM carrier that the LM386's input RC network averages back
// into an analog waveform. See WIRING_MEGA.md for the input filter.
//
// !! Pin 11 and pin 12 are Timer1's. Do NOT call analogWrite() on either while
// !! the audio engine is running; it will fight for the same registers.
#define RX_SPEAKER_PWM_PIN 11

// Two "activation status" LEDs (anode -> pin via 220-330 ohm, cathode -> GND).
// Plain GPIO on the far header, clear of every timer.
#define RX_LED_DOORBELL_PIN 22  // pulses per doorbell packet; SOLID once locked
#define RX_LED_VOICE_PIN    24  // on for the duration of an intercom session

// ---- Audio engine ----
// Timer1 = 8-bit fast PWM carrier on OC1A.  Timer3 = 8 kHz CTC sample clock.
// Timer2 is deliberately left alone: Arduino's tone() owns it for the buzzer.
// Timer0 is left alone: it owns millis()/delay().
#define AUDIO_RING_BYTES 256   // uint8_t indices wrap naturally; 255 usable = ~32 ms
#define AUDIO_MID_RAIL   128   // silence sits at half duty
