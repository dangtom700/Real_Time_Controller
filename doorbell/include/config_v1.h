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
#define LINK_PEER_MAC_INIT { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// ===========================================================================
//  Guest node (door panel) — wired in later steps, listed now so the map is
//  in one place.  From PLAN §10.4: 14 of 16 usable signals, 2 spare.
// ===========================================================================
#define BTN1_GPIO      2    // LP_GPIO2 — doorbell, deep-sleep wake
#define BTN2_GPIO      3    // LP_GPIO3 — intercom PTT, deep-sleep wake
#define BUZZER_GPIO    5    // LEDC tone
#define LED1_GPIO      6
#define LED2_GPIO      7

// INMP441 microphone, I2S RX — step 4, when the part arrives.
#define MIC_SCK_GPIO   10
#define MIC_WS_GPIO    11
#define MIC_SD_GPIO    4

// ILI9341 panel, SPI — no MISO (write-only) and no SDCS (no card). §10.4.
#define TFT_MOSI_GPIO  18
#define TFT_SCK_GPIO   19
#define TFT_CS_GPIO    20
#define TFT_DC_GPIO    21
#define TFT_RST_GPIO   22
#define TFT_BL_GPIO    23

// ===========================================================================
//  Host node (indoor chime) — 6 of 16 signals.
//  MAX98357A amp, I2S TX — step 5, when the part arrives.
// ===========================================================================
#define AMP_BCLK_GPIO  10
#define AMP_LRC_GPIO   11
#define AMP_DIN_GPIO   4
#define CHIME_BUZZER_GPIO 5
