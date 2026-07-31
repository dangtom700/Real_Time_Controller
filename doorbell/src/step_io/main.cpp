// STEP IO — the panel's inputs and its noise, before either is wired into the
// state machine.  Board A (guest node) only.
//
// Wiring:  BTN1 -> IO1 -> GND        normally open, INPUT_PULLUP
//          BTN2 -> IO2 -> GND        normally open, INPUT_PULLUP
//          passive buzzer -> IO3 -> GND
// Run:     pio run -e step_io -t upload -t monitor --upload-port COM9
// PASS:    a three-note sweep at boot; ONE line per press, never a burst;
//          pressCount walking 1,2,3 and then locking; BTN2 held 2 s prints TALK.
//
// This deliberately reproduces the semantics step 1 faked.  The pressCount that
// walked 1,2,3,3,3,3 over the wire was synthetic; here it comes from a real
// thumb, clamped by the same RAGE_LIMIT and expired by the same TIMER1_MS.  If
// the numbers here behave like the numbers there did, the input layer can be
// dropped under the link with nothing else changing.
//
// FAIL and what it means:
//   a press prints several lines   -> bounce is outlasting DEBOUNCE_MS. The
//                                     bounce-window figure on each line tells
//                                     you what to raise it to.
//   reads PRESSED and never lets go-> switch wired to 3V3 rather than GND, or
//                                     the pull-up is not on (check INPUT_PULLUP)
//   floating / random presses      -> same: no pull-up, so the pin is an aerial
//   silence from the buzzer        -> read the two self-tests at boot. They
//                                     shake IO3 by two different routes, so
//                                     which one is silent says where to look.
//                                     Both silent is usually a 3-pin module
//                                     with VCC unconnected: signal and GND
//                                     alone will not drive one that has a
//                                     transistor on it.
//   one pitch regardless of note   -> ACTIVE buzzer, not passive. Those
//                                     self-oscillate and ignore frequency.
//
// IO1 is also LINK_TX_GPIO, so this sketch and the step 1/2 UART link cannot
// both be wired at once.  Nothing here touches the link, so that is fine.

#include <Arduino.h>
#include "driver/gpio.h"      // gpio_set_drive_capability, for the buzzer
#include "config_v1.h"

// ---------------------------------------------------------------------------
//  Debounce.  Level-change timestamping rather than a blocking delay: a delay
//  in here would be indistinguishable from a stuck button, and would also make
//  the hold timing below lie.
// ---------------------------------------------------------------------------
// The hardware contract: normally-open switch to GND with INPUT_PULLUP, so a
// released button reads HIGH.  That is FIXED, not sampled.
//
// An earlier version of this sketch calibrated the released level from whatever
// the pin read at boot.  That was useful while the wiring was still suspect,
// and it is a trap the moment it is not: boot with a button held, or rewire
// without resetting, and the polarity silently inverts — every press then gets
// counted when you let go, which reads as "the button works but feels wrong"
// rather than as a fault.  Encode the contract and complain when the hardware
// disagrees with it.
static const bool RELEASED = HIGH;

struct Button {
  uint8_t     pin;
  const char *name;
  bool        stable;
  bool        lastRaw;
  uint32_t    lastChangeMs;   // when the RAW level last moved
  uint32_t    pressedAtMs;
  bool        holdFired;
  uint32_t    rawEdges;       // every raw transition, settled or not
  uint32_t    edgesAtAccept;  // rawEdges when the last edge was accepted
};

static Button btn1 = {BTN1_GPIO, "BTN1", RELEASED, RELEASED, 0, 0, false, 0, 0};
static Button btn2 = {BTN2_GPIO, "BTN2", RELEASED, RELEASED, 0, 0, false, 0, 0};

static bool traceRaw = true;   // every raw edge, until the wiring is trusted

// Raw transitions swallowed by the debounce window — the real bounce measure.
// The elapsed time is not: on a clean edge it only ever reports DEBOUNCE_MS.
static uint32_t bounceEdges(const Button &b) {
  const uint32_t n = b.rawEdges - b.edgesAtAccept;
  return n > 1 ? n - 1 : 0;
}

// Returns true on the instant the button becomes pressed.
static bool pollPressed(Button &b, bool &released) {
  released = false;
  const bool     raw = digitalRead(b.pin);
  const uint32_t now = millis();

  if (raw != b.lastRaw) {
    b.lastRaw      = raw;
    b.lastChangeMs = now;
    b.rawEdges++;
    // A pin that is merely floating produces a flood here and never settles.
    // That flood is the diagnosis, so it is not rate-limited.
    if (traceRaw) {
      Serial.printf("      [raw] %s IO%d -> %s  @%lu ms\n",
                    b.name, b.pin, raw ? "HIGH" : "LOW", (unsigned long)now);
    }
  }

  if (raw == b.stable || (now - b.lastChangeMs) < DEBOUNCE_MS) return false;

  b.stable        = raw;
  b.edgesAtAccept = b.rawEdges;

  if (b.stable != RELEASED) {
    b.pressedAtMs = now;
    b.holdFired   = false;
    return true;
  }
  released = true;
  return false;
}

static bool isPressed(const Button &b) { return b.stable != RELEASED; }

// ---------------------------------------------------------------------------
//  Doorbell state — PLAN §2.3.1, the same clamp step 1 sent over the wire.
// ---------------------------------------------------------------------------
static uint8_t  pressCount   = 0;
static bool     locked       = false;
static uint32_t lastPressMs  = 0;

// Not tone().  That routes through a FreeRTOS task whose only failure report is
// log_e(), which is compiled out at the default log level — so a failed
// ledcAttach gives you silence and no message, which looks exactly like a
// wiring fault.  Drive LEDC directly and print whether the attach worked.
static bool buzzerReady = false;

static void buzzerBegin() {
  // A magnetic buzzer element wants 30-50 mA, which is past a GPIO's default
  // 20 mA drive.  CAP_3 raises it to roughly 40 mA — still marginal, but it is
  // the difference between "faint" and "nothing" often enough to be worth one
  // line.  A piezo element is capacitive and does not care.
  gpio_set_drive_capability((gpio_num_t)BUZZER_GPIO, GPIO_DRIVE_CAP_3);

  buzzerReady = ledcAttach(BUZZER_GPIO, 2000, 10);   // 10-bit, retuned per note
  Serial.printf("[io] buzzer LEDC attach on IO%d: %s\n",
                BUZZER_GPIO, buzzerReady ? "OK" : "FAILED");
}

static void beep(uint16_t hz, uint16_t ms) {
  if (!buzzerReady) return;
  ledcWriteTone(BUZZER_GPIO, hz);    // 50% duty at hz
  delay(ms);
  ledcWriteTone(BUZZER_GPIO, 0);
}

// Two independent ways to shake the same pin.  Which one is silent tells you
// where to look, and that is worth 600 ms at boot.
static void buzzerSelfTest() {
  Serial.println("[io] buzzer test 1/2: LEDC tone, 440 / 880 / 1320 Hz");
  beep(440, 150);
  beep(880, 150);
  beep(1320, 200);

  Serial.println("[io] buzzer test 2/3: bit-banged 1 kHz, LEDC out of the way");
  ledcDetach(BUZZER_GPIO);
  pinMode(BUZZER_GPIO, OUTPUT);
  const uint32_t end = millis() + 300;
  while ((int32_t)(millis() - end) < 0) {
    digitalWrite(BUZZER_GPIO, HIGH);
    delayMicroseconds(500);
    digitalWrite(BUZZER_GPIO, LOW);
    delayMicroseconds(500);
  }
  digitalWrite(BUZZER_GPIO, LOW);

  // Steady DC. This is the one that says what KIND of buzzer is attached, and
  // it also gives a multimeter something to read: IO3 should sit at 3.3 V for
  // the whole 400 ms.
  Serial.println("[io] buzzer test 3/3: steady DC HIGH for 400 ms");
  digitalWrite(BUZZER_GPIO, HIGH);
  delay(400);
  digitalWrite(BUZZER_GPIO, LOW);

  buzzerBegin();                     // re-attach for normal use

  Serial.println("[io] --- how to read the three tests ---");
  Serial.println("[io] 1 and 2 ring, 3 clicks  -> passive, working. Done.");
  Serial.println("[io] 3 rings a steady tone   -> ACTIVE buzzer. It ignores");
  Serial.println("[io]                            frequency; drive it with");
  Serial.println("[io]                            digitalWrite, not tone/LEDC.");
  Serial.println("[io] only 2 rings            -> LEDC config, not the buzzer.");
  Serial.println("[io] all three silent, and IO3 measures 3.3 V during test 3");
  Serial.println("[io]                         -> the pin works, the element");
  Serial.println("[io]                            does not. A magnetic buzzer");
  Serial.println("[io]                            needs more current than a");
  Serial.println("[io]                            GPIO gives: add an NPN with");
  Serial.println("[io]                            a flyback diode across it.");
  Serial.println("[io] all three silent, IO3 flat -> wiring or a dead element.");
}

void setup() {
  Serial.begin(115200);
  delay(400);

  pinMode(BTN1_GPIO, INPUT_PULLUP);
  pinMode(BTN2_GPIO, INPUT_PULLUP);

  Serial.println();
  Serial.println("=====================================================");
  Serial.println(" STEP IO - buttons and buzzer");
  Serial.println("=====================================================");
  Serial.printf("BTN1 (doorbell) : IO%d   BTN2 (PTT) : IO%d\n",
                BTN1_GPIO, BTN2_GPIO);
  Serial.printf("buzzer          : IO%d   passive, tone-driven\n", BUZZER_GPIO);
  Serial.printf("debounce        : %lu ms\n", (unsigned long)DEBOUNCE_MS);
  Serial.printf("PTT hold        : %lu ms\n", (unsigned long)PTT_HOLD_MS);
  Serial.printf("rage limit      : %u presses\n", RAGE_LIMIT);
  Serial.printf("press window    : %lu ms of quiet resets the count\n",
                (unsigned long)TIMER1_MS);

  const bool idle1 = digitalRead(BTN1_GPIO);
  const bool idle2 = digitalRead(BTN2_GPIO);

  Serial.printf("\nidle levels     : BTN1=%s  BTN2=%s   (both should be HIGH)\n",
                idle1 ? "HIGH" : "LOW", idle2 ? "HIGH" : "LOW");

  if (!idle1 || !idle2) {
    Serial.println();
    // ASCII only: this banner is read over a console that mangles non-ASCII.
    Serial.println("*** IDLE IS LOW - that is backwards for a normally-open  ***");
    Serial.println("*** switch to GND, which should idle HIGH on the pull-up. ***");
    Serial.println("*** Three things do this, in order of likelihood:        ***");
    Serial.println("***  1. 4-pin tactile switch wired across an internally  ***");
    Serial.println("***     connected pair. Those two pins are ALWAYS joined ***");
    Serial.println("***     - use diagonally opposite corners instead.       ***");
    Serial.println("***  2. the switch is normally CLOSED, not open.         ***");
    Serial.println("***  3. the pin is shorted to GND somewhere else.        ***");
    Serial.println("*** Polarity is NOT adjusted to match - that would hide  ***");
    Serial.println("*** the fault and count presses on release. Fix the wire.***");
    Serial.println("*** The [raw] trace below is the answer: press a button. ***");
    Serial.println("***   no [raw] lines at all  -> case 1 or 3, a hard short***");
    Serial.println("***   clean HIGH then LOW    -> case 2, an NC switch     ***");
    Serial.println("***   a flood of [raw] lines -> the pull-up is not on    ***");
    Serial.println();
  }

  Serial.println();
  buzzerBegin();
  buzzerSelfTest();

  Serial.println("[io] ready. press the buttons.");
}

void loop() {
  const uint32_t now = millis();

  // ---- the doorbell window (TIMER1_MS) ------------------------------------
  if (pressCount > 0 && (now - lastPressMs) >= TIMER1_MS) {
    Serial.printf("[io] window expired after %lu ms - count reset\n",
                  (unsigned long)TIMER1_MS);
    pressCount = 0;
    locked     = false;
  }

  // ---- BTN1: doorbell ------------------------------------------------------
  bool released = false;
  if (pollPressed(btn1, released)) {
    lastPressMs = now;
    if (pressCount < RAGE_LIMIT) pressCount++;
    locked = (pressCount >= RAGE_LIMIT);

    Serial.printf("[io] BTN1 press   count=%u locked=%u   (bounce: %lu extra edges)\n",
                  pressCount, locked ? 1 : 0, (unsigned long)bounceEdges(btn1));

    // A locked-out visitor gets a different noise from a welcome one.
    if (locked) { beep(220, 300); }
    else        { beep(1200, 80); }
  }

  // ---- BTN2: push to talk --------------------------------------------------
  if (pollPressed(btn2, released)) {
    Serial.printf("[io] BTN2 press   hold it %lu ms to talk   (bounce: %lu extra edges)\n",
                  (unsigned long)PTT_HOLD_MS, (unsigned long)bounceEdges(btn2));
  }
  if (released) {
    if (btn2.holdFired) Serial.println("[io] BTN2 release - TALK ends");
    else                Serial.printf("[io] BTN2 released early after %lu ms - no talk\n",
                                      (unsigned long)(now - btn2.pressedAtMs));
  }
  if (isPressed(btn2) && !btn2.holdFired &&
      (now - btn2.pressedAtMs) >= PTT_HOLD_MS) {
    btn2.holdFired = true;
    Serial.println("[io] BTN2 held >>> TALK <<<");
    beep(660, 100);
  }
}
