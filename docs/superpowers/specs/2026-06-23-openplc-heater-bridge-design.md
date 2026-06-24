# OpenPLC Heater Bridge — Design

## Context

`Controller-Toolbox` has ~90 controller/estimator implementations (PID, LQR, MPC, ADRC, SMC,
H-infinity, Kalman family, ...) and 18 physics case studies, but `job_post_analysis/Career_Intelligence_Report.md`
(167 scraped Canadian mechatronics/automation postings, N=166 valid) shows the market doesn't
ask for any of that theory: LQR, MPC, SMC, ADRC, state-space, and pole-placement all sit at
**0/166**. What the market actually wants, by coverage:

| Skill | Coverage |
|---|---|
| Electrical systems / wiring / schematics | 85% |
| Troubleshooting / maintenance | 67% |
| Documentation / technical writing | 55% |
| Commissioning / startup (FAT/SAT) | 47% |
| PLC programming (any brand) | 37% |
| Sensors / instrumentation | 37% |
| Embedded / firmware / microcontrollers | 28% |
| HMI development | 20% |
| Motion control / servo / VFD | 20% |

This project exists to close that gap with a real artifact: take a controller that already
exists in `Controller-Toolbox/lib/embedded` (the header-only, no-Eigen subset built for exactly
this kind of deployment) and run it for real, on a PLC, against real I/O, with the
documentation a hiring manager would expect to see from a commissioning engineer.

`Real_Time_Controller/` was an empty, unscoped placeholder (per `TODO.md` section 3) before this
spec. This is its scope.

## Goal

A Raspberry Pi running OpenPLC controls a real 12V heater to hold a temperature setpoint, with:
an ESP8266 as a Modbus remote-I/O node reading the sensor and driving the actuator, a
Structured Text PID block ported from `BasicPID`, a Ladder Diagram safety/sequencing layer, the
built-in OpenPLC web HMI, and a small commissioning document set (wiring schematic, I/O list,
FAT-style test record).

## Non-goals (explicit backlog, not lost — revisit after v1)

- Motor + encoder motion-control rig (speed/position PID/LQR).
- Tank/liquid-level control (pump + level sensor).
- Pure sequencing/interlock demo (conveyor/traffic-light style ladder logic, no continuous
  control math).
- Upgrading the HMI to Node-RED or Ignition-style SCADA.
- Porting a second, fancier algorithm (cascade, feedforward) into Structured Text.
- Mains-voltage heating elements — deliberately out of scope for safety; v1 stays at 12V.

## Architecture

```
[Thermistor] --analog--> [ESP8266: ADC read, deg-C convert] --Modbus TCP--> [Raspberry Pi: OpenPLC runtime]
                                                                                  |  ST: PID block (ported from BasicPID)
                                                                                  |  LD: start/stop, sensor-fault watchdog,
                                                                                  |      over-temp hard cutoff
                                                                                  v
[Heater + MOSFET] <--PWM duty-- [ESP8266: Modbus slave, sets PWM] <--Modbus TCP-- [OpenPLC]
                                                                                  |
                                                                          [Built-in web HMI]
```

The ESP8266 is a **Modbus remote I/O module**, not the controller — the control law lives on
the Pi/OpenPLC side. This mirrors how real PLC systems are structured (CPU + distributed I/O),
which is itself a point worth being able to talk about in an interview.

## Hardware (bill of materials)

| Component | Notes |
|---|---|
| Raspberry Pi (3/4/Zero 2 W) | Runs OpenPLC runtime. Buy if not already owned. |
| ESP8266 | Already owned. Runs as Modbus TCP slave (OpenPLC ESP8266 firmware). |
| 10k NTC thermistor + divider resistor | Into ESP8266 ADC (A0). |
| 12V resistive heater pad or PTC element | Low-voltage by design — no mains wiring. |
| Logic-level MOSFET driver module | PWM'd from an ESP8266 GPIO, switches heater. |
| 12V power supply, protoboard, jumper wires | |

Deliberately avoiding mains voltage removes an entire class of electrical-safety risk and
wiring complexity that isn't needed to demonstrate the skill.

## Software design

**Modbus mapping** (ESP8266 as slave, OpenPLC as master):
- Holding register: temperature reading (°C, scaled integer or float per OpenPLC's ESP8266
  firmware convention).
- Holding register or coil: heater PWM duty / on-off command from OpenPLC to ESP8266.

**Structured Text — PID function block**
- Math ported directly from `Controller-Toolbox/lib/embedded/BasicPID` (same gains/structure,
  re-expressed in ST). Inputs: setpoint, measured temperature. Output: heater duty (0–100%,
  clamped).
- Anti-windup / output clamping behavior should match `BasicPID`'s contract so the real-vs-
  simulated comparison (see Validation) is meaningful.

**Ladder Diagram — sequencing & safety**
- Start/stop control of the loop.
- Sensor-fault watchdog: ADC reading outside a physically plausible range (e.g. thermistor
  disconnected or shorted) forces a fault state.
- Over-temperature hard cutoff: independent of what the PID output says, forces heater off
  above a fixed limit. This interlock must win regardless of PID/ST state — it is the
  "machine safety" claim, and it has to be demonstrably independent of the control loop logic,
  not just another input to it.

**HMI**
- OpenPLC's built-in web dashboard: setpoint (writable), live PV, live output duty, fault
  status indicator. No custom HMI build for v1.

## Documentation deliverables

These map directly to the two highest-coverage skills in the report (electrical schematics
85%, documentation/commissioning 55%/47%) and are cheap to produce once the rig works:

1. **Wiring schematic** — Pi ↔ ESP8266 ↔ thermistor divider ↔ MOSFET ↔ heater, with supply
   rails labeled.
2. **I/O list** — every signal, its type (AI/AO/DI/DO), range, and Modbus address.
3. **FAT-style commissioning record** — step-response test (setpoint step, captured PV trend)
   and fail-safe test (disconnect sensor mid-run, confirm heater forces off and fault flag
   raises).

## Validation

- Step the setpoint and capture the PV trend (via OpenPLC's dashboard or logged values).
- Compare the shape of the real response against the equivalent simulated `BasicPID` step
  response already produced by an existing Controller-Toolbox PID example, as a sanity check
  that the ST port behaves like the source implementation under real-world noise and actuator
  lag.
- Run the fail-safe test from the FAT record above and confirm the ladder interlock wins.

## Risks / open questions to resolve during implementation

- Confirm which OpenPLC ESP8266 firmware/build is current and what register layout it expects
  — pin this down before wiring, not after.
- PWM heater control via a thermal actuator has real lag/noise that the toolbox's simulated
  case studies don't model; expect to retune gains rather than reuse simulated values directly.
- Pi + OpenPLC + ESP8266 Modbus TCP round-trip timing should be checked against the thermal
  process's time constant (heaters are slow, so this is unlikely to be a problem, but verify
  rather than assume).
