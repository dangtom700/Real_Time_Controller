#include <Arduino.h>
#include <Stepper.h>

// Defines the number of steps per rotation
const int stepsPerRevolution = 2048;

// Driver inputs in PHYSICAL order IN1-IN2-IN3-IN4, used by the coil check
const int coilPins[4] = {2, 3, 4, 5};
// A 28BYJ-48 on 5V stalls near 15 RPM, so there is nothing to learn above
// about 25. Sweeping to 100 in steps of 10 put only one point (10) under
// the limit, which is why it looked like 10 was the only value that worked.
const int max_RPM = 100;

// Quarter turn per trial: enough to see movement, quick even at 2 RPM
const int benchSteps = stepsPerRevolution;

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
    Serial.println(F("rpm\texp_ms\tact_ms\tturned?"));

    // 1 RPM at a time, so the stall threshold lands between two rows
    for (int rpm = 2; rpm <= max_RPM; rpm += 1) {
        unsigned long start_time = millis();
        step(rpm, benchSteps);
        unsigned long end_time = millis();

        // Same formula the library uses, so we can compare like for like
        unsigned long stepDelay = 60000000UL / stepsPerRevolution / rpm;
        unsigned long expected = stepDelay * (unsigned long)benchSteps / 1000UL;

        Serial.print(rpm);
        Serial.print(F("\t"));
        Serial.print(expected);
        Serial.print(F("\t"));
        Serial.print(end_time - start_time);
        // Timing cannot tell us -- the pulse train is on schedule either way
        Serial.println(F("\t<- watch the shaft"));

        delay(600);
    }

    Serial.println(F("--- sweep complete ---\n"));
    delay(4000);
}