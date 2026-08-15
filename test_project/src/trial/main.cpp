#include <Arduino.h>
//Includes the Arduino Stepper Library
#include <Stepper.h>

// Defines the number of steps per rotation
const int stepsPerRevolution = 2048;

// Driver inputs in PHYSICAL order IN1-IN2-IN3-IN4, used by the coil check
const int coilPins[4] = {2, 3, 4, 5};

// Creates an instance of stepper class (28BYJ-48 – 5V Stepper Motor)
// Pins entered in sequence IN1-IN3-IN2-IN4 for proper step sequence.
// With IN1..IN4 on pins 2,3,4,5 that swap means 2, 4, 3, 5 -- NOT 2, 3, 4, 5.
Stepper myStepper = Stepper(stepsPerRevolution, 2, 4, 3, 5);

// Energises each coil on its own so you can confirm the wiring.
// Watch the four LEDs on the ULN2003 board: they must light 1,2,3,4 in order.
void checkCoils() {
  Serial.println(F("Coil check - driver LEDs should light in order 1,2,3,4"));
  for (int i = 0; i < 4; i++) {
    Serial.print(F("  IN"));
    Serial.print(i + 1);
    Serial.print(F(" = pin "));
    Serial.println(coilPins[i]);
    digitalWrite(coilPins[i], HIGH);
    delay(700);
    digitalWrite(coilPins[i], LOW);
    delay(300);
  }
}

// One quarter turn at a given speed, pausing so the rotor settles
// before the next reversal.
void trial(const char *label, int rpm, int steps) {
  Serial.print(label);
  Serial.print(F(" at "));
  Serial.print(rpm);
  Serial.println(F(" RPM"));

  myStepper.setSpeed(rpm);
  myStepper.step(steps);

  delay(1500);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n28BYJ-48 test: direction and speed isolated"));
  checkCoils();
}

void loop() {
  // Same speed both ways. If both turn now, direction was never the problem.
  trial("CW ", 5, stepsPerRevolution / 4);
  trial("CCW", 5, -(stepsPerRevolution / 4));

  // Same distance each way at rising speed, to find where it stalls.
  for (int rpm = 8; rpm <= 14; rpm += 3) {
    trial("CW ", rpm, stepsPerRevolution / 4);
    trial("CCW", rpm, -(stepsPerRevolution / 4));
  }

  Serial.println(F("--- cycle complete ---\n"));
  delay(3000);
}
