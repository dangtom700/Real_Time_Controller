

## Hardware components

### MCU
- Arduino Nano ESP32
- ESP8288MOD

### Components
- 3x stepper motor with driver (5V)
- 2.2" 18 bit color LCD display with microSD
- OLED screen 0.96
- Ultra-sound sensor
- Gas sensor (Flying Fish)
- Rain drop sensor with decoder
- Water level sensor with decoder
- Photosensor
- Induction heater (Mini-ZVS 5-12V)
- Temperature and humidity sensor
- 1-channel 5V relay module
- Power adapter (110V -> 12V)
- PWM module
- Potentiometer
- Accelerometer
- Mini pump
- IR flame sensor module
- RC snubber circuit module
- JQ6500-16P miniature serial port MP3 voice sound module
- PCB breadboard power supply module
- Servo motor

### Mics
- Breadboard wires
- Capacitors
- LEDs
- Buttons

## Combinatory game

A portfolio of small projects against the inventory above, mixing fun builds, job-market-skill
practice, and (project 7) the ESP's actual differentiator over a plain Arduino — networking.
Full decomposition and rationale:
`docs/superpowers/specs/2026-06-27-embedded-project-portfolio-design.md`.

### 1. Anti-Rage Doorbell

Spec done (`Anti-rage doobell/README.md`), build first. After 3 button presses within 30s it
locks the buzzer, the OLED escalates through hardcoded "brainrot" bitmap animations, and 30s of
inactivity puts it to deep sleep. No firmware yet.

### 2. Environment/Weather Station

Temp/humidity + photosensor + rain-drop + water-level sensors, logged to the 2.2" color LCD and
microSD, with moving-average filtering on the noisy analog channels and a graceful fallback if
the SD card is missing. Stretch: run a defined ramp/hold/cool test profile and produce a
pass/fail report instead of passively logging.

### 3. Gas/Flame Safety Interlock

Two tracks, both kept: **(a)** CodeSys V3 soft-PLC over Modbus, ESP8266 as remote I/O exposing
gas + flame + photosensor readings, 2-of-3 voting before tripping the relay (a lone disagreement
logs a fault instead of shutting down); **(b)** a separate pure-C++ track that empirically
characterizes the induction heater's thermal response (step response → FOPDT model → PI with
anti-windup) directly on the MCU, independent of the PLC layer.

### 4. Stepper/Servo Motion Rig

Base build: a 2-axis plotter/turntable driven by non-blocking, timer-based step pulses with
trapezoidal accel/decel, homing, and an e-stop — the firmware goal is the motion controller, not
the mechanism. Stretch: closed-loop position control using the accelerometer as feedback, or
active vibration damping (notch-filtered counter-phase servo drive against the stepper's own,
precisely known pulse-rate disturbance).

### 5. Tank Level / Pump Control Loop

Ultrasonic + mini pump + PWM as a closed-loop tank-fill demo, reusing/comparing PID math from
`Controller-Toolbox`'s `BasicPID`. Stretch: feed-forward dead-zone compensation for the pump's
stiction (verified against a sine-wave setpoint), or sensor redundancy via a complementary filter
fusing the ultrasonic and water-level readings.

### 6. "Doomsday Console"

Frankenstein dashboard for leftover parts (accelerometer, photosensor, spares): boots "stable,"
then random faults fire (a shake, a timed button hold, a dial-to-a-shown-value) that must be
resolved before a countdown expires, with escalating MP3 alerts. No job-market skill target by
design — this slot exists to be fun.

### 7. "Micro-Lavarand" Entropy/RNG Node

The one project that exercises the ESP's actual differentiator over a plain Arduino Uno —
WiFi and hardware crypto acceleration. Harvests ambient chaos (photosensor flicker,
temp/humidity drift, a couple of deliberately unconnected ADC pins) into a byte array, whitens
it via SHA-256, and feeds a CSPRNG to serve random bytes from a local endpoint
(`GET /api/next-seed`) that other devices on the LAN can query directly. Deliberately not a
dashboard — no database, no cloud, the node is just useful by itself. Reuses #2's photosensor
and temp/humidity sensor. Stretch: use the entropy for local key rotation as a real consumer.
