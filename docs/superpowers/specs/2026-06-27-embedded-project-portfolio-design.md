# Embedded Project Portfolio — Design

## Context

`Real_Time_Controller`'s root `README.md` was an empty stub until now; it has been filled in
with the actual on-hand parts inventory (Arduino Nano ESP32, ESP8266MOD, 3x stepper motors,
sensors, a 2.2" color LCD, etc. — see `README.md` for the full list).

A prior spec (`2026-06-23-openplc-heater-bridge-design.md`, and its accompanying
`docs/backlog_hardware_bom.csv`) scoped this repo narrowly around one job-market-driven
deliverable (OpenPLC + a heater + an ESP8266). Both have been deleted: that framing assumed
OpenPLC and a to-buy parts list; the actual plan is broader (a portfolio of small projects
against parts already owned) and the PLC tool of choice is **CodeSys V3**, not OpenPLC.

One sub-project already exists with a complete spec: **Anti-Rage Doorbell**
(`Anti-rage doobell/README.md` — goal, wiring table, state machine, milestones). No firmware
written yet. It is treated as project #1 in this portfolio, not re-designed here.

A separate brainstorming pass (pasted into `README.md`, then mined and discarded — see below)
explored what the ESP32/ESP8266 can do that a plain Arduino Uno can't: WiFi and networking. Most
of that pass proposed cloud-dashboard-style architectures (Grafana/Node-RED/React + a database),
which were rejected as redundant — uninteresting compared to the rest of this portfolio. One
idea from that pass survived: an **edge node that is useful entirely on its own, with no
backend** — project #7 below.

## Goal

A portfolio of small, mostly-independent embedded projects that collectively put every part in
the inventory to use, mixing three motivations:

- **Fun** — builds that are satisfying for their own sake (the doorbell's "rage-ringer lockout"
  is the model for this).
- **Skill-building** — builds that double as practice for job-market-relevant skills
  (electrical wiring/schematics, PLC programming via CodeSys V3, sensors/instrumentation,
  motion control), per the gap analysis that originally motivated the heater-bridge spec.
- **The ESP's actual differentiator** — at least one project (#7) specifically exercises
  networking/edge capability, since that's what distinguishes this MCU family from a basic
  Arduino Uno and was otherwise unused by the rest of the portfolio.

Target mix: roughly 80% well-rounded, single-theme projects and 20% intentionally
"frankenstein" — a mashup project that exists specifically to use parts that don't have a
natural single-purpose home.

## Hardware-sharing policy

The inventory has exactly one Arduino Nano ESP32 and one ESP8266MOD. Projects are built and
torn down **one at a time** — rewire/reflash the same pair of boards for the next project in
the queue. No additional MCU purchases are assumed. This means only one project is physically
assembled and "live" at any given time; the rest are backlog. Parts are reused sequentially
across projects (e.g. the photosensor and temp/humidity sensor serve project #2, then later
project #7 — see below); nothing is permanently dedicated to one project.

## Portfolio decomposition

Each entry below is a scope stub, not a full spec. Full specs are written one at a time,
immediately before that project is implemented, following this same brainstorming → write spec
→ plan → implement cycle. Order is roughly the build order but not committed — re-sequencing
later is fine.

### 1. Anti-Rage Doorbell — *spec already exists, build first*

Already fully specced in `Anti-rage doobell/README.md` (state machine, wiring, milestones).
Uses: Nano ESP32, OLED 0.96, a button, a buzzer. No firmware yet. Next step for this
sub-project is firmware implementation planning, not re-brainstorming the design.

### 2. Environment/Weather Station — *well-rounded, fun-leaning*

Temp/humidity sensor + photosensor + rain-drop sensor + water-level sensor, logged to the 2.2"
color LCD and microSD card. ESP8266 as the data-collection brain. Apply a moving-average filter
to the noisy analog rain/light channels, and handle a missing/failed SD card gracefully (don't
crash the display UI). General-purpose instrumentation practice; no PLC/CodeSys angle.

**Stretch variant:** instead of passive logging, run a defined environmental test profile
(e.g. ramp to 40°C, hold 30 min, cool down, humidity check) and generate a pass/fail report —
closer to environmental-qualification test equipment than a passive logger.

### 3. Gas/Flame Safety Interlock — *well-rounded, skill-building*

Splits into two tracks, since the safety layer and the heater's own control loop are separable
concerns. Both are kept (not a replacement of one by the other):

**3a — PLC track (CodeSys V3 + Modbus).** Gas sensor + IR flame sensor + photosensor (as a
third, corroborating witness) as inputs, the induction heater (Mini-ZVS) as the "hazard"
actuator, a relay as the cutoff, the JQ6500 MP3 module for a spoken alarm. ESP8266 acts as
Modbus remote I/O; CodeSys V3 runs the interlock as **2-of-3 voting logic** rather than a
single-sensor trip — if only one sensor disagrees with the other two, the system logs a
"sensor drift" fault instead of either ignoring it or shutting down outright. RC snubber
suppresses the relay's inductive kickback. This is the direct, better-scoped descendant of the
deleted OpenPLC heater-bridge spec, retargeted at the PLC tool actually being learned.

**3b — Firmware track (pure C++, no CodeSys).** Separately, characterize the induction heater's
own thermal response empirically: apply a step input, record the temperature rise, fit a
first-order-plus-dead-time (FOPDT) model, then tune a PI controller with back-calculation
anti-windup, run directly on the MCU. This is independent of 3a's safety layer — it's the
"white-box math, black-box hardware" complement, and gives a second, empirically-tuned data
point to compare against `Controller-Toolbox`'s `BasicPID` (which used simulated, not measured,
plant parameters).

### 4. Stepper/Servo Motion Rig — *well-rounded, skill-building*

Base build: a 2-axis plotter or turntable. The firmware-architecture goal is the motion
*controller* itself, not the mechanism — non-blocking step-pulse generation off hardware timers
(not `delay()`), trapezoidal accel/decel ramps, a homing routine, and an emergency-stop input.
The potentiometer provides manual jog/speed control.

**Stretch variants** once the base loop works: (a) closed-loop position control using the
accelerometer as feedback — estimate velocity by differentiating, position by
double-integrating — for a full-state-feedback/pole-placement controller; (b) active vibration
damping — since the stepper's own pulse rate is a precisely *known* disturbance frequency, a
digital notch filter on the accelerometer signal can drive the servo in counter-phase to
demonstrate damping.

### 5. Tank Level / Pump Control Loop — *well-rounded, skill-building*

Ultrasonic sensor + mini pump + PWM module as a closed-loop tank-fill demo. Candidate for
reusing/comparing PID math from `Controller-Toolbox`'s `BasicPID` — same reuse idea the
heater-bridge spec had, just on a water tank instead of a heater, which avoids that spec's
mains-voltage safety concerns entirely.

**Stretch variants:** (a) measure and compensate the pump's dead-zone/stiction with a
feed-forward inverse dead-zone term, verified by commanding a sine-wave setpoint and confirming
the output isn't clipped; (b) add a second, cheap corroborating sensor (e.g. the water-level
sensor alongside the ultrasonic) and fuse the two with a complementary filter, flagging a fault
if they disagree — the same redundancy idea as 3a's voting logic, applied to a non-safety loop.

### 6. "Doomsday Console" — *frankenstein, ~20% slot*

Accelerometer + photosensor + whatever else doesn't get a clean single-purpose home, dumped onto
one interactive dashboard. The console boots into a "stable" state, but random faults fire (a
detected shake, a button that must be held within N seconds, a potentiometer that must be dialed
to a value shown on the LCD/OLED) — the user resolves cascading failures before a countdown
expires, with MP3-module alerts escalating as faults stack. Exists to be fun and absorb parts
that don't have a natural home; no job-market skill target by design.

### 7. "Micro-Lavarand" Entropy/RNG Node — *well-rounded, skill-building (networking)*

The one project in this portfolio that exercises the ESP's actual differentiator over a plain
Arduino Uno: WiFi and hardware crypto acceleration. Harvests ambient physical chaos — the
photosensor (light flicker), the temp/humidity sensor (thermal drift), and one or two
deliberately unconnected ADC pins (picking up ambient EM noise) — accumulates raw samples into a
byte array, whitens them through a SHA-256 hash (fast on the ESP32's hardware acceleration) to
produce an unbiased seed, then feeds a CSPRNG (e.g. ChaCha20) for a continuous random-byte
stream. Exposes this over a small local WebServer endpoint (`GET /api/next-seed`) that other
devices on the LAN can query directly.

Deliberately **not** a dashboard: there's no database, no Grafana, no central server — the
node's value is that it answers a query by itself. Reuses the photosensor and temp/humidity
sensor already used by project #2 (sequential reuse per the hardware-sharing policy, not a
conflict). Stretch goal: use the generated randomness for local key rotation (e.g. rotate a
WPA2 password or a symmetric key on a timer) as a concrete consumer of the entropy, rather than
just serving random bytes nobody uses.

### Shared infrastructure (not standalone projects)

Power adapter (110V→12V), RC snubber circuit module, PCB breadboard power supply module,
breadboard wires, capacitors, LEDs, spare buttons. These get reused inside whichever project
needs mains-derived 12V power or inductive-load (relay/motor) spike suppression — most relevant
to #3 (relay switching the induction heater) and #4/#5 (motor/pump driver transients).

## Documentation practice (applies across all seven projects)

Regardless of which project is live, document the physical build the same way every time:

- A **hand-drawn, photographed wiring diagram** — no EDA tool required. Show power-plane
  separation (dirty 12V/motor power vs. clean 5V/3.3V logic) and a single star-ground point
  where all grounds meet, to make ground-loop reasoning visible.
- A **pin-audit table**: MCU pin → connected to → signal type → max current → protection →
  pull-up/pull-down state. Cheap to produce, and it's the kind of artifact that proves a pin's
  power-on state was actually considered (e.g. a floating relay-enable pin during MCU reset).

This costs nothing extra in hardware or tooling and directly targets the two highest-coverage
skills from the original job-market gap analysis that motivated the (now-deleted) heater-bridge
spec: electrical schematics (85% of postings) and documentation/commissioning (55%/47%).

## Non-goals for this spec

- Full design (wiring tables, state machines, milestones) for projects #2–#7 — those are written
  individually, immediately before each is built.
- A fixed build order beyond "doorbell first" — re-sequencing the rest based on what feels right
  at the time is expected and fine.
- Buying additional hardware (extra MCUs, sensors) — out of scope per the hardware-sharing
  policy above.
- Cloud-dashboard/fleet-management-style architectures (Grafana, Node-RED, MQTT brokers, React
  dashboards) — explicitly considered and rejected as redundant; the one networking idea worth
  keeping (project #7) was chosen specifically because it has no dashboard/backend component.
