#include <Arduino.h>
#include <Stepper.h>

// Defines the number of steps per rotation
const int stepsPerRevolution = 2048;

// Driver inputs in PHYSICAL order IN1-IN2-IN3-IN4, used by the coil check
const int coilPins[4] = {2, 3, 4, 5};
const int RPM = 20;

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

void step(int rpm, int steps) {
  myStepper.setSpeed(rpm);
  myStepper.step(steps);
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n28BYJ-48 test: direction and speed isolated"));
  checkCoils();
}

void loop() {
    // pi forward, pi/2 backward, and repeat
    step(RPM, stepsPerRevolution / 2);
    step(RPM, -(stepsPerRevolution / 4));
}