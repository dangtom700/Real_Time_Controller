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

// ===========================================================================
//  Guest node (door panel) — wired in later steps, listed now so the map is
//  in one place.  PLAN §10.4 budgeted 14 of 16 usable signals with 2 spare;
//  the analog mic gives two back, so it is now 12 of 16 with 4 spare.
// ===========================================================================
#define BTN1_GPIO      2    // LP_GPIO2 — doorbell, deep-sleep wake
#define BTN2_GPIO      3    // LP_GPIO3 — intercom PTT, deep-sleep wake
#define BUZZER_GPIO    5    // LEDC tone
#define LED1_GPIO      6
#define LED2_GPIO      7

// GY-MAX4466 electret mic amp, ANALOG out — step 4, when the part arrives.
// Not the INMP441 this plan first assumed.  The MAX4466 emits an analog audio
// signal, so it lands on the ADC rather than on I2S: MIC_SCK and MIC_WS stop
// existing and IO10/IO11 come free on this node.
//
// IO4 is forced, not chosen.  The C6 has ONE ADC unit and only GPIO0-6 are
// analog-capable (A0..A6).  Of those IO0/IO1 are the bench link, IO2/IO3 are
// the deep-sleep wake buttons, IO5 is the buzzer and IO6 is LED1.  IO4 is what
// is left.  It doubles as JTAG MTMS (§4.1), so it is noisy at boot — fine for
// an audio input biased mid-rail, but do not trust a reading taken during reset.
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
#define MIC_OUT_GPIO   4          // A4 / ADC1_CH4

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
#define AMP_BCLK_GPIO  10
#define AMP_LRC_GPIO   11
#define AMP_DIN_GPIO   4
#define CHIME_BUZZER_GPIO 5
