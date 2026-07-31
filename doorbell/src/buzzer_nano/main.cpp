// BUZZER BENCH TEST — Arduino Nano ESP32 (ESP32-S3), not the C6.
//
// The point of running this on a different board is that it shares nothing with
// board A: different silicon, different pin, different LEDC unit, different
// drive strength. If the buzzer sounds here it is a good element and the fault
// is on the C6 side; if it stays silent on both boards it is the buzzer.
//
// Wiring:  buzzer +  ->  D4
//          buzzer -  ->  GND
//
// Run:     pio run -e buzzer_nano -t upload -t monitor --upload-port COMx
//          The Nano ESP32 uploads over DFU. Double-tap RESET to enter the
//          bootloader first; the COM port changes when it does.
//
// PIN NUMBERING TRAP — read before changing the pin.  This board's PlatformIO
// manifest sets BOARD_USES_HW_GPIO_NUMBERS, which turns OFF the Arduino pin
// remap that the Arduino IDE uses. With it off, the constant D4 resolves to
// GPIO7, not GPIO4. So digitalWrite(4, ...) drives a pin that is not the one
// silkscreened D4, and the buzzer stays quiet for a reason that has nothing to
// do with the buzzer. Always use the D4 symbol, never the literal 4.
//
// The three tests are the same ones step_io runs on the C6, so the results are
// directly comparable.

#include <Arduino.h>
#include "driver/gpio.h"

static const uint8_t BUZZ = D4;      // = GPIO7 on this board. See above.

static bool ledcOk = false;

static void buzzerBegin() {
  gpio_set_drive_capability((gpio_num_t)BUZZ, GPIO_DRIVE_CAP_3);
  ledcOk = ledcAttach(BUZZ, 2000, 10);
  Serial.printf("[buzz] LEDC attach on D4 (GPIO%u): %s\n",
                (unsigned)BUZZ, ledcOk ? "OK" : "FAILED");
}

static void beep(uint16_t hz, uint16_t ms) {
  if (!ledcOk) return;
  ledcWriteTone(BUZZ, hz);
  delay(ms);
  ledcWriteTone(BUZZ, 0);
}

static void runTests() {
  Serial.println("[buzz] test 1/3: LEDC tone, 440 / 880 / 1320 Hz");
  beep(440, 150);
  beep(880, 150);
  beep(1320, 200);
  delay(300);

  Serial.println("[buzz] test 2/3: bit-banged 1 kHz, LEDC detached");
  ledcDetach(BUZZ);
  pinMode(BUZZ, OUTPUT);
  const uint32_t end = millis() + 300;
  while ((int32_t)(millis() - end) < 0) {
    digitalWrite(BUZZ, HIGH);
    delayMicroseconds(500);
    digitalWrite(BUZZ, LOW);
    delayMicroseconds(500);
  }
  digitalWrite(BUZZ, LOW);
  delay(300);

  Serial.println("[buzz] test 3/3: steady DC HIGH, 400 ms");
  digitalWrite(BUZZ, HIGH);
  delay(400);
  digitalWrite(BUZZ, LOW);

  buzzerBegin();   // re-attach for the next round
  Serial.println("[buzz] --- repeating in 3 s ---");
}

void setup() {
  Serial.begin(115200);
  delay(1500);        // native USB CDC: give the host time to attach

  Serial.println();
  Serial.println("=====================================================");
  Serial.println(" BUZZER TEST - Arduino Nano ESP32");
  Serial.println("=====================================================");
  Serial.printf("buzzer on D4, which is GPIO%u on this board\n", (unsigned)BUZZ);
  Serial.println("1 and 2 ring, 3 clicks -> passive element, healthy");
  Serial.println("3 rings a steady tone  -> ACTIVE buzzer, not passive");
  Serial.println("all three silent       -> the element itself is the problem");
  Serial.println();

  buzzerBegin();
}

void loop() {
  runTests();
  delay(3000);
}
