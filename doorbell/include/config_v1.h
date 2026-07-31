#pragma once
#include <stdint.h>
// Pin map and timings for prototype 1.  Two ESP32-C6 boards.
// Every GPIO choice here comes from PLAN_v2.md §4.1 (what the C6 forbids) and
// §10.4 (the final allocation), so the pins you wire in step 1 are the pins the
// finished panel uses.  Nothing gets moved later.

// ===========================================================================
//  Timings — PLAN §2.5.  Two timers, deliberately different values.
// ===========================================================================
static const uint32_t TIMER1_MS   = 30000;   // doorbell window AND idle->sleep.
                                             // ONE constant: §2.5 says these are
                                             // the same diagram box, and keeping
                                             // them separate lets them drift.
static const uint32_t TIMER2_MS   = 180000;  // voice session cap (was 15000 — §2.5)
static const uint32_t PTT_HOLD_MS = 2000;    // hold button 2 this long to talk
static const uint32_t DEBOUNCE_MS = 40;
static const uint8_t  RAGE_LIMIT  = 3;
static const uint8_t  IMAGE_COUNT = 3;       // == RAGE_LIMIT, one constant (§2.3.1)

// ===========================================================================
//  ESP32-C6 pin rules — PLAN §4.1.  Read before moving anything.
// ---------------------------------------------------------------------------
//    IO0–IO7    the ONLY deep-sleep wake pins (LP_GPIO0–7). Buttons live here.
//    IO4–IO7    also JTAG (MTMS/MTDI/MTCK/MTDO) — noisier at boot.
//    IO8, IO9   strapping (boot mode). IO8 is the DevKitC-1's RGB LED.
//    IO12, IO13 USB D-/D+.
//    IO15       strapping (ROM log control).
//    IO16, IO17 UART0 = the serial console. Do not reuse.
// ===========================================================================

// ---- The bench link: UART1 (PLAN §4.3) ------------------------------------
// IO0/IO1 are the two pins §10.4 leaves spare, and they are clean: LP_GPIO,
// no strapping role, no JTAG role.  The shipped link is the radio, so this
// UART is a bench crutch — which is exactly why it gets the spare pins and
// not good ones.
#define LINK_TX_GPIO   1
#define LINK_RX_GPIO   0
#define LINK_BAUD      921600     // 92 kB/s ceiling vs 16.3 kB/s needed

// ---- The radio ------------------------------------------------------------
#define ESPNOW_CHANNEL 1          // both nodes MUST agree

// >>> FILL THIS IN BEFORE STEP 3 <<<
// Step 0 prints each board's STA MAC.  Put the RECEIVER's MAC here and reflash
// both.  We unicast rather than broadcast on purpose: ESP-NOW only returns a
// real delivery ack for unicast, and that ack rate is the number PLAN §5.1
// wants you to watch.  Broadcast would report 100% success no matter what.
// Board B = 20:6E:F1:17:07:40, the board that ran voice_rx for the step 2
// baseline.  Board A (the sender) is 20:6E:F1:17:03:B8 -- swap these if you
// ever reverse the roles, since this must name the RECEIVER.
#define LINK_PEER_MAC_INIT { 0x20, 0x6E, 0xF1, 0x17, 0x07, 0x40 }

// ---- Zigbee, the fallback -------------------------------------------------
// The C6 carries an 802.15.4 radio, and Zigbee is the fallback for the DOORBELL
// EVENT path only.  It cannot carry the intercom, and the reason is a hard
// frame limit rather than a tuning problem:
//
//   VoiceMsg                 224 bytes, 73/s  = 130 kbps
//   802.15.4 max PHY frame   127 bytes        -> ~100 usable after MAC headers
//
// So every chunk would need splitting across three frames, each paying its own
// header, CSMA backoff and ack, against a 250 kbps raw PHY that yields perhaps
// 50-100 kbps in practice.  That is roughly half of what the voice path needs.
// Events are a few bytes seconds apart and fit comfortably; voice does not.
// Halving the audio (8-bit companded, ~65 kbps) is what would change this.

// ===========================================================================
//  Board C link — guest node (A) to the Arduino Nano ESP32 (C).
// ---------------------------------------------------------------------------
//  Simplex: A talks, C listens, C never replies.  Nothing C knows needs to
//  travel back, and one direction costs one wire instead of two.
//
//  TWO lines, not one, because C deep sleeps.  A byte arriving on RX cannot
//  wake an ESP32-S3 from deep sleep at all — only GPIO, timer and touch can —
//  and even from light sleep the first characters are eaten waking the UART.
//  So a single wire carrying both "wake" and "here is the message" loses the
//  message every time.  Assert WAKE, wait PANEL_WAKE_MS for C to boot, then
//  send.
//
//  Board C side:  message -> D0 (GPIO44)   wake -> D2 (GPIO5, RTC-capable)
//  Use Serial1 remapped onto D0/D1 there; UART0 is free because that board's
//  console runs over native USB CDC, but remapping keeps the two apart.
//
//  Fit a 10k pulldown on the wake line at C.  Every GPIO floats between reset
//  and the first pinMode, and a floating wake line means A's own boot wakes C
//  for no reason — the same defensive pulldown the amp's SD pin gets.
// ===========================================================================
#define PANEL_TX_GPIO    0        // A -> C, UART TX. Old LINK_RX pin, now free.
#define PANEL_WAKE_GPIO  10       // A -> C, wake. IO10/11 are the only free
                                  // pins with no strapping or JTAG role, so the
                                  // line does not glitch while A boots.
#define PANEL_BAUD       115200
#define PANEL_WAKE_MS    300      // deep-sleep boot time to allow before sending

// ===========================================================================
//  Guest node (door panel) — wired in later steps, listed now so the map is
//  in one place.  PLAN §10.4 budgeted 14 of 16 usable signals with 2 spare;
//  the analog mic gives two back, so it is now 12 of 16 with 4 spare.
// ===========================================================================
// As physically wired on the bench, 2026-07-30.  Both buttons are still inside
// LP_GPIO0-7, which is the constraint that actually matters: nothing outside
// that range can wake the C6 from deep sleep.
//
// NOTE: BTN1 sits on IO1, which is LINK_TX_GPIO.  That retires the step 1/2
// UART bench link on this board — the two cannot coexist.  Acceptable because
// the shipped transport is the radio and the UART baseline is already recorded
// in README.md, but it does mean you cannot re-measure the wire for comparison
// without lifting this button.  Move BTN1 to IO7 if you want that back.
#define BTN1_GPIO      1    // LP_GPIO1 — doorbell, deep-sleep wake
#define BTN2_GPIO      2    // LP_GPIO2 — intercom PTT, deep-sleep wake

// NOT POPULATED.  The buzzer moved to the Arduino Nano ESP32 that carries the
// OLED.  It never sounded from IO3 despite the LEDC path being correct: the
// same element, same wiring topology and same code (drive-cap -> ledcAttach ->
// ledcWriteTone) produced three clean tones from the Nano's D4.  Kept defined
// so step_io still builds as a pin-level test, but nothing is on this pin.
//
// UNRESOLVED: whether ledcAttach on IO3 was reporting OK or FAILED was never
// captured, so it is not known whether IO3 can drive anything at all.  Treat
// IO3 as suspect before reusing it for something else.
//
// Consequence of the move: the doorbell beep is now a message to another board
// rather than a local write, so it inherits that link's latency and failure
// modes.  It also means the panel can no longer make a sound while the C6 is
// the only thing awake, which matters for the deep-sleep story in §2.5.
#define BUZZER_GPIO    3
#define LED1_GPIO      7    // not yet wired; moved off IO6, now the mic input
#define LED2_GPIO      5    // not yet wired

// GY-MAX4466 electret mic amp, ANALOG out — step 4, when the part arrives.
// Not the INMP441 this plan first assumed.  The MAX4466 emits an analog audio
// signal, so it lands on the ADC rather than on I2S: MIC_SCK and MIC_WS stop
// existing and IO10/IO11 come free on this node.
//
// Only GPIO0-6 are analog-capable on the C6 (A0..A6, one ADC unit), so the mic
// had to land in that range.  IO6 is the pick and it is a better one than the
// IO4 first written here: IO4 doubles as JTAG MTMS and is noisy at boot, while
// IO6 has no strapping or JTAG role at all.  IO6 was LED1; the LED moved to
// IO7, which costs nothing since neither LED is wired yet.
//
// POWER IT FROM 3.3V, NOT 5V.  The MAX4466 biases its output at VCC/2 and
// swings around it.  At 5V that centres on 2.5V with peaks heading toward 5V,
// into a pin whose absolute max is ~3.6V.  Its supply range is 2.4-5.5V, so
// 3.3V is well in spec and puts the centre at 1.65V — right inside the ADC's
// 0-3.1V window at 11 dB attenuation.
//
// Sample it with analogContinuous() (ADC DMA), not analogRead().  The whole
// design rests on a 13.75 ms deadline and analogRead() jitter would land
// straight in the gap-max that step 2 calibrated.
#define MIC_OUT_GPIO   6          // A6 / ADC1_CH6

// ILI9341 panel, SPI — no MISO (write-only) and no SDCS (no card). §10.4.
#define TFT_MOSI_GPIO  18
#define TFT_SCK_GPIO   19
#define TFT_CS_GPIO    20
#define TFT_DC_GPIO    21
#define TFT_RST_GPIO   22
#define TFT_BL_GPIO    23

// ===========================================================================
//  Host node (indoor chime) — 6 of 16 signals.  UNCHANGED by the mic swap.
//  MAX98357A amp, I2S TX — step 5, when the part arrives.
//
//  Keep this amp digital.  The C6 has no DAC at all (unlike the original ESP32
//  and the S2), so an analog amp such as an LM386 would have to be fed PWM
//  through an RC reconstruction filter.  I2S straight into the MAX98357A skips
//  that whole problem, and the part is happy on the 5V rail (2.5-5.5V).
// ===========================================================================
// All three are as physically wired, 2026-07-31.  BCLK is off IO8 at last,
// which was the point: IO8 is the DevKitC-1's addressable RGB LED (the variant
// header defines PIN_RGB_LED 8, which is what step0_hello blinks via
// RGB_BUILTIN) and it is also a boot-mode strapping pin on the C6.  A
// continuous bit clock into a strapping pin invites intermittent boot failures,
// and the LED sits on the same net as a load.
//
// The clocks landed the opposite way round from what this file first said —
// LRC on IO11, BCLK on IO10 — and the wire wins, because the swap costs
// nothing.  Every I2S signal on the C6 reaches its pad through the GPIO matrix,
// so no pin is special to a particular signal, and IO10 and IO11 are equally
// clean: no strapping role, no JTAG role, and the analog mic freed both.
//
// DIN sits on IO4, which is JTAG MTMS and noisy at boot.  Harmless here because
// DIN is an output and the amp is held muted through boot by the pulldown on SD
// below, so whatever the pin does before setup() runs is never amplified.
#define AMP_LRC_GPIO   11
#define AMP_BCLK_GPIO  10
#define AMP_DIN_GPIO   4

// SD_MODE is enable AND channel select, so it cannot be left floating:
//     < 0.16 V  shutdown        0.77 - 1.4 V  (L+R)/2
//   0.16-0.77 V  right channel      > 1.4 V   left channel
//
// Drive HIGH to enable on the LEFT channel, LOW to mute.  Muting between
// sessions matters here: a Class D amp hisses when idle and this intercom is
// silent most of the time.
//
// Fit a 10k pulldown from SD to GND as well.  Two reasons.  It holds the amp
// off through boot and reset, before setup() can drive the pin, which kills the
// power-on pop.  And 10k specifically, not 100k: many breakouts carry a ~1M
// pullup to VIN, against which 100k divides to ~0.45 V — inside the RIGHT
// CHANNEL band, so the amp would come up enabled on the wrong channel instead
// of muted.  10k lands at ~0.05 V, under the shutdown threshold regardless.
//
// Software must send I2S on the LEFT slot to match.
#define AMP_SD_GPIO    3    // HIGH = on (left channel), LOW = muted

// GAIN needs no wire at all.  Unconnected is the 9 dB setting, which is right
// for voice.  Tie it to GND for 15 dB if the chime is too quiet.
#define CHIME_BUZZER_GPIO 5
