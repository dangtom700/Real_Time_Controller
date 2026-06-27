# Complete Project Inventory

## PRIORITY PROJECTS (Aerospace/Industrial Focus)

These were the final recommended projects based on your control theory background and career goals.

### 1. Flight Control Actuator Test Bench
**Components:**
- ESP32
- Servo motor
- Accelerometer
- LCD/OLED
- Buttons

**Features:**
- Position commands
- Closed-loop position verification
- Motion profiles
- Fault injection
- Emergency stop
- Calibration mode
- PID tuning
- Velocity limiting
- Soft limits
- Watchdog timer
- Brownout detection

**Skills Demonstrated:**
- Control Systems
- Servo Control
- State Machine
- Embedded C++
- Safety Logic
- Real-Time Systems

---

### 2. Environmental Qualification Chamber Controller
**Components:**
- Temperature sensor
- Humidity sensor
- LCD
- SD card
- Relay
- Buttons

**Features:**
- Logs environmental conditions
- Maintains test sequences
- Raises alarms
- Records failures

**Example Test Profile:**
```
25°C → 40°C for 30 minutes → Cool down → Humidity test → Generate report
```

**Skills Demonstrated:**
- Test automation
- Data logging
- Finite state machines
- Environmental testing
- Report generation

---

### 3. Pump Control System (Industrial PLC Style)
**Components:**
- Water level sensor
- Ultrasonic sensor
- Mini pump
- Relay
- PWM module
- OLED

**States:**
```
Normal → Warning → High Alarm → Pump On → Critical → Emergency Shutdown
```

**Features:**
- Hysteresis
- Debounce
- Sensor plausibility checks
- Watchdog recovery

**Skills Demonstrated:**
- Industrial logic
- State machines
- Safety interlocks
- Robust firmware design

---

### 4. Sensor Fusion Navigation Platform
**Components:**
- Accelerometer
- Ultrasonic sensor
- Light sensor
- OLED

**Algorithms:**
- Moving averages
- Complementary filters
- Kalman filter (optional)

**Skills Demonstrated:**
- Sensor fusion
- Signal processing
- Navigation
- Estimation theory

---

### 5. Stepper Motion Controller (Pure Controller)
**Components:**
- 3x Stepper motors with drivers
- Servo motor
- Buttons
- LCD/OLED

**Features:**
- Acceleration ramps
- Jerk limiting
- Homing
- Command queue
- Trajectory generation
- Emergency stop
- Limit switch simulation

**Skills Demonstrated:**
- Motion control firmware
- Real-time timing
- Trajectory planning
- Multi-axis coordination

---

### 6. Adaptive Pole-Placement Motor Controller
**Components:**
- Stepper motor + driver
- Accelerometer (feedback)
- Potentiometer (command)

**Control Theory:**
- Model as second-order system: `J·θ̈ + B·θ̇ = τ`
- Full-state feedback controller `u = -Kx`
- Luenberger observer
- State-space matrices `A, B, C, K`

**Skills Demonstrated:**
- State-space control
- Observer design
- Load disturbance rejection
- System identification

---

### 7. Disturbance-Rejection Thermal Plant
**Components:**
- Induction heater (plant)
- Temperature sensor (feedback)
- Relay (actuator enable)
- PWM module (duty cycle)

**Control Theory:**
- Empirical system identification
- FOPDT model: `G(s) = K·e^(-θs)/(τs + 1)`
- Ziegler-Nichols or IMC tuning
- Back-calculation anti-windup

**Skills Demonstrated:**
- Plant characterization
- PI/PID with anti-windup
- Dead-time compensation
- Thermal dynamics

---

### 8. Vibration Notch-Filter Isolator
**Components:**
- Accelerometer
- Stepper motor (shaker)
- Servo motor (active mass damper)

**Control Theory:**
- Digital IIR notch filter
- Bilinear Transform
- Active vibration suppression
- Pole-zero map analysis

**Skills Demonstrated:**
- Frequency-domain analysis
- Digital filter design
- Active damping
- Vibration control

---

### 9. Nonlinear Pump Dead-Zone Compensator
**Components:**
- Mini pump
- Water level sensor (or ultrasonic)
- PWM module

**Control Theory:**
- Dead-zone characterization
- Gain-scheduled PID
- Feed-forward inverse dead-zone
- Nonlinear control compensation

**Skills Demonstrated:**
- Nonlinear control
- Actuator characterization
- Compensation design
- Tracking performance

---

### 10. Sensor Fusion Redundancy Manager
**Components:**
- Ultrasonic sensor
- Photosensor (secondary proxy)
- Temperature sensor
- Relay
- LEDs

**Features:**
- Complementary filter
- Kalman filter (2x2 matrices)
- Residual monitoring
- Fault detection
- Safe state management

**Skills Demonstrated:**
- Redundancy management
- Fault-tolerant control
- Sensor validation
- Estimation theory

---

## SOFTWARE-FIRST PROJECTS

### 11. Edge Device Fleet Manager
**Components:**
- Nano ESP32 (master)
- ESP8288 (worker nodes)

**Software Stack:**
- MQTT Broker (Mosquitto)
- REST APIs
- OTA updates
- Device provisioning
- Health monitoring

**Features:**
- Device registration
- Firmware version tracking
- Heartbeat monitoring
- Remote configuration
- Telemetry streaming

**Skills Demonstrated:**
- Distributed systems
- MQTT
- Docker
- JSON serialization

---

### 12. Tiny Kubernetes for ESPs
**Components:**
- Multiple ESP devices

**Features:**
- Dynamic node deployment
- Service discovery
- Configuration management
- Watchdogs
- Health monitoring

**Skills Demonstrated:**
- Scheduling
- Service management
- Configuration distribution
- Fault tolerance

---

### 13. Embedded Digital Twin
**Components:**
- Any sensors/actuators

**Features:**
- Physical device
- Software model
- State synchronization
- Prediction capability

**Skills Demonstrated:**
- Model-based design
- State estimation
- Predictive maintenance
- System modeling

---

### 14. Modular RTOS Automation Controller
**Components:**
- ESP32
- Various sensors/actuators

**Architecture:**
```
Task Scheduler
    ├── Sensor Task
    ├── Control Task
    ├── Logging Task
    ├── Display Task
    ├── Network Task
    └── Diagnostics Task
```

**Skills Demonstrated:**
- RTOS concepts
- Task scheduling
- Cooperative multitasking
- Embedded architecture

---

### 15. CAN Bus Simulator (Virtual)
**Components:**
- ESP32 only

**Features:**
- Virtual CAN network
- Message routing
- Fault injection
- Automotive protocol practice

**Skills Demonstrated:**
- Automotive protocols
- Message handling
- Network simulation
- Fault injection testing

---

### 16. Embedded Event Bus
**Components:**
- ESP32
- Various sensors

**Events:**
```
GasDetected
ButtonPressed
PumpStarted
LowBattery
TemperatureAlarm
```

**Skills Demonstrated:**
- Event-driven architecture
- Publish-subscribe pattern
- Decoupled design
- Message handling

---

### 17. Hardware Entropy Server
**Components:**
- Photosensor
- Floating ADC pins
- Temperature sensor

**Features:**
- Entropy harvesting
- SHA256 mixing
- CSPRNG generation
- REST API for random bytes

**Skills Demonstrated:**
- Cryptography
- Entropy harvesting
- Hardware security
- API design

---

## 🔧 HARDWARE-HEAVY PROJECTS

### 18. 3-Axis Pen Plotter / Laser Engraver
**Components:**
- 3x Stepper motors + drivers
- Servo motor (pen lift)
- Buttons
- LCD

**Features:**
- G-code parser
- Trapezoidal acceleration
- Coordinate translation
- Motion planning

**Skills Demonstrated:**
- Kinematics
- Motion planning
- CNC control
- Coordinate systems

---

### 19. Closed-Loop Fluid Control (PID Sump)
**Components:**
- Water level sensor
- Mini pump
- PWM module
- OLED
- Relay (backup)

**Features:**
- PID control loop
- Setpoint tracking
- Backup pump switching
- Real-time graphing

**Skills Demonstrated:**
- PID tuning
- Actuator throttling
- Control visualization
- Fail-safe logic

---

### 20. Induction Heater Safety Station
**Components:**
- Induction heater
- Relay
- RC snubber
- IR flame sensor
- Temperature sensor
- Buzzer

**Features:**
- Temperature/timer control
- Emergency stops
- Flame detection
- Over-temperature protection

**Skills Demonstrated:**
- High-current isolation
- Interrupt handling
- Safety interlocks
- Transient suppression

---

### 21. Electro-Mechanical Damping System
**Components:**
- Accelerometer
- Stepper motor + driver
- OLED
- Potentiometer

**Features:**
- Vibration detection
- Active counter-force
- PID control loop
- Phase lag analysis

**Skills Demonstrated:**
- Active vibration control
- Real-time sensor fusion
- Phase margin tuning
- Mechanical resonance

---

## BLENDED FULL-STACK PROJECTS

### 22. Industrial SCADA Mini-Plant
**Components:**
- Gas sensor
- Raindrop sensor
- Temp/humidity sensor
- Water level sensor
- Pump
- Relay
- LCD
- SD card

**Features:**
- Modbus slave
- Data logging
- Alarm rules
- Historical playback

**Skills Demonstrated:**
- Modbus/TCP
- Historian databases
- Alarm management
- Industrial protocols

---

### 23. Predictive Maintenance Edge Node
**Components:**
- Accelerometer
- Temperature sensor
- Humidity sensor
- SD card
- OLED
- WiFi

**Features:**
- Vibration analysis
- FFT / RMS computation
- TinyML classification
- Alert generation

**Skills Demonstrated:**
- Sensor fusion
- Feature extraction
- Edge ML
- Predictive maintenance

---

### 24. Interactive Audio-Visual Console
**Components:**
- OLED
- Buttons
- LEDs
- Potentiometer
- JQ6500 MP3 module
- Servo motor

**Features:**
- Random fault events
- Audio feedback
- State machine game
- Interactive UI

**Skills Demonstrated:**
- UX design
- Non-blocking state machines
- Audio integration
- Human-machine interface

---

### 25. Research Automation Workflow
**Components:**
- ESP32
- SD card
- Various sensors
- WiFi

**Features:**
- YAML/JSON workflow engine
- Experiment sequences
- Data logging
- API integration

**Skills Demonstrated:**
- Workflow engines
- Data provenance
- Research automation
- Integration with existing tools

---

## SECURITY-FOCUSED PROJECTS

### 26. Secure OTA Infrastructure
**Components:**
- ESP32
- ESP8288
- SD card

**Features:**
- Signed firmware
- Signature verification
- Update rollback
- Dual-partition flash

**Skills Demonstrated:**
- Secure boot
- Flash partitioning
- Update management
- Cryptographic verification

---

### 27. Secure Door Access System
**Components:**
- OLED
- Buttons
- Relay
- JQ6500 MP3 module

**Features:**
- PIN authentication
- Hashing
- Audit logging
- Encrypted storage

**Skills Demonstrated:**
- Authentication
- Access control
- Secure storage
- Audit trails

---

## ROBOTICS / MECHATRONICS

### 28. Autonomous Inspection Rover (Static)
**Components:**
- Stepper motors
- Ultrasonic sensor
- Servo motor
- Display

**Features:**
- Occupancy grid generation
- Obstacle mapping
- Visualization

**Skills Demonstrated:**
- SLAM concepts
- Occupancy mapping
- Sensor integration
- Robotics algorithms

---

### 29. Robotic Camera Motion Platform
**Components:**
- 2x Stepper motors
- Servo motor

**Features:**
- Pan/Tilt/Focus control
- Object tracking
- Face following
- Bezier motion planning

**Skills Demonstrated:**
- Kinematics
- Motion planning
- Object tracking
- Smooth trajectory generation

---

## RESEARCH-ORIENTED PROJECTS

### 30. Smart Laboratory Logger
**Components:**
- ESP32
- Environmental sensors
- SD card
- WiFi

**Features:**
- Continuous data logging
- Cloud upload
- Anomaly detection
- LLM-generated summaries

**Skills Demonstrated:**
- Research automation
- Data pipeline
- Integration with LLMs
- Experiment documentation

---

### 31. Automated Experiment Controller
**Components:**
- ESP32
- Sensors/actuators
- SD card

**Features:**
- YAML experiment definitions
- Workflow execution
- Data collection
- Result logging

**Skills Demonstrated:**
- Workflow engine design
- Flexible configuration
- Experiment automation
- Reusable code architecture

---

### 32. Research Knowledge Graph Integration
**Components:**
- ESP32
- Sensors/actuators
- WiFi

**Integration with:**
- PDF extraction pipeline
- Knowledge graph
- Experiment planner

**Features:**
- Research paper analysis
- Experiment planning
- Result logging back to graph

**Skills Demonstrated:**
- Cross-domain integration
- Knowledge management
- Experiment planning
- Data provenance tracking

---

## PRODUCTION LINE / INDUSTRIAL

### 33. Production Line Simulator
**Components:**
- LEDs
- Buttons
- Sensors
- Actuators (simulated)

**Stations:**
```
Sensor → Conveyor → Inspection → Reject → Packaging
```

**Features:**
- Queuing
- Timing
- Scheduling
- Fault handling

**Skills Demonstrated:**
- Manufacturing systems
- Process simulation
- Scheduling algorithms
- Fault management

---

## TIER 2 & STRETCH PROJECTS

### 34. Fault Tolerant Embedded Controller
**Components:**
- All sensors

**States:**
```
Healthy
Fault
Disconnected
Out of Range
Timeout
```

**Skills Demonstrated:**
- Robust firmware
- Fault handling
- Graceful degradation
- Error recovery

---

### 35. Hardware Abstraction Layer (HAL)
**Components:**
- All peripherals

**Layers:**
```
GPIO Driver
PWM Driver
SPI Driver
UART Driver
ADC Driver
I2C Driver
```

**Skills Demonstrated:**
- Driver development
- Hardware abstraction
- Portability
- Clean architecture

---

### 36. Embedded Data Recorder
**Components:**
- SD card
- Sensors
- ESP32

**Features:**
- Binary logging
- CRC validation
- Timestamping
- Diagnostics

**Skills Demonstrated:**
- File systems
- Data integrity
- Binary protocols
- Diagnostic logging

---

### 37. Built-In Self-Test (BIST)
**Components:**
- All components

**Tests:**
```
RAM test
Sensor test
Display test
ADC calibration
Flash CRC
PWM verification
```

**Skills Demonstrated:**
- Self-test design
- System verification
- Power-on testing
- Fault detection

---

### 38. Mini FADEC Simulator
**Components:**
- Potentiometer (throttle)
- Servo (output)
- Display

**States:**
```
Startup → Idle → Acceleration → Overspeed → Shutdown
```

**Skills Demonstrated:**
- Engine control
- State machines
- Safety logic
- Real-time control

---

### 39. Redundant Flight Computer
**Components:**
- ESP32 (primary)
- ESP8288 (secondary)

**Features:**
- Dual computation
- Cross-checking
- Voting
- Controller switching

**Skills Demonstrated:**
- Redundancy
- Voting algorithms
- Fault tolerance
- Fail-over design

---

### 40. RTOS-based Mission Computer
**Components:**
- ESP32
- FreeRTOS

**Tasks:**
```
Sensor Task (1kHz)
Control Task (500Hz)
Communications (10Hz)
Logging (1Hz)
Health Monitor (2Hz)
Display (20Hz)
```

**Skills Demonstrated:**
- Real-time scheduling
- Task priorities
- Resource management
- Deterministic execution

---

## ANTI-RAGE DOORBELL
**Components:**
- ESP32
- JQ6500 MP3 module
- Buttons
- LEDs
- Potentiometer

**Features:**
- Rage detection
- State machine
- Non-blocking timers
- Audio feedback
- Adjustable sensitivity

**Skills Demonstrated:**
- State machines
- Debouncing
- Interrupt handling
- Audio integration

---

## ENVIRONMENT / WEATHER STATION
**Components:**
- ESP8266 or Nano ESP32
- Temperature/Humidity sensor
- Raindrop sensor
- Photosensor
- 2.2" LCD with SD slot

**Features:**
- Real-time telemetry
- Data logging to CSV
- Graceful error handling
- Sensor filtering

**Skills Demonstrated:**
- Sensor calibration
- File system management
- Moving average filters
- SPI/I2C bus sharing

---

## GAS/FLAME SAFETY INTERLOCK
**Components:**
- Nano ESP32
- Gas sensor
- IR flame sensor
- 1-Channel relay
- RC snubber

**Features:**
- Modbus slave
- CODESYS V3 integration
- PLC ladder logic
- Safety interlocks
- Transient suppression

**Skills Demonstrated:**
- Modbus RTU/TCP
- PLC integration
- Safety-critical logic
- Electrical protection

---

## STACKED CORE PROJECTS (6-Project Portfolio)

| Project | Demonstrates |
|---------|--------------|
| **Motion Control Unit** | Stepper control, timers, trajectory generation, PWM |
| **Environmental Test Controller** | Instrumentation, logging, finite state machines |
| **Industrial Pump Controller** | PID, alarms, safety interlocks, actuator control |
| **Flight Actuator Test Bench** | Servo control, sensor feedback, fault handling |
| **Embedded Data Recorder** | SPI, SD card, CRCs, binary logging, diagnostics |
| **System Health Monitor** | Watchdog timers, self-tests, fault codes, recovery |

---

## PROJECT ARCHITECTURE PATTERNS REFERENCED

### Control Theory Patterns
- State-space control
- PID with anti-windup
- Kalman filtering
- Notch filtering
- Dead-zone compensation
- Complementary filtering
- Luenberger observers
- Pole placement

### Software Architecture Patterns
- Event-driven architecture
- Publish-subscribe
- State machines
- Task scheduling
- Redundancy voting
- Fault tolerance
- Watchdog recovery

### Hardware Patterns
- Star grounding
- Decoupling capacitors
- RC snubbers
- Power plane separation
- Transient suppression
- Opto-isolation
- EMI mitigation

---

## SKILLS MATRIX

### Aerospace/Industrial Focus Skills
| Category | Skills |
|----------|--------|
| **Control Theory** | PID, state-space, observers, Kalman filters, notch filters, dead-zone compensation |
| **Real-Time Systems** | Hardware timers, interrupts, deterministic execution, watchdog timers |
| **Sensor Integration** | I2C, SPI, ADC, signal conditioning, sensor fusion, calibration |
| **Actuator Control** | Stepper profiles, servo control, PWM, motion planning, homing |
| **Safety Systems** | Interlocks, redundancy, fail-safe, self-test, emergency stops |
| **Communication** | UART, SPI, I2C, Modbus, CAN concepts |
| **Data Management** | Binary logging, CRC, file systems, timestamping |
| **Diagnostics** | Self-test, health monitoring, fault codes, error recovery |

### Avoided/De-emphasized Skills
- Web dashboards
- Cloud APIs
- REST APIs
- MQTT dashboards
- Home automation
- Smart things
- IoT watering
- Voice assistants

---

## NOTABLE THEMES FROM CONVERSATION

1. **Hardware documentation via hand-drawn schematics** - Pen-and-paper diagrams with annotations showing decoupling caps, star-grounding, wire gauge/color coding

2. **System identification reports** - Step response, FOPDT modeling, linearization around operating points, validation against measured data

3. **Pin-level audit tables** - Excel/CSV showing every connection with protection, pull-up/down, max current considerations

4. **Physical wiring walkthrough videos** - Unscripted video showing actual breadboard, explaining power routing, grounding, EMI mitigation

5. **Model-based design** - Plant characterization → transfer function derivation → controller design → validation

6. **Fault injection** - Simulate sensor failures, stalls, power sags to demonstrate robustness

---

## RECOMMENDED ADDITIONS (Under $100 CAD)

- Rotary encoders
- Limit switches
- Load cell with amplifier
- Small DC motor with encoder
- CAN transceiver (MCP2515)
- Oscilloscope (cheap logic analyzer)

These additions enable:
- Closed-loop motor control
- Homing routines
- Force/torque sensing
- Industrial communications
- Waveform analysis

---

## DOCUMENTATION TEMPLATES REFERENCED

### Wiring Diagram Standard
```
[Dirty Power: 12V] → [Relay] → [Induction Heater]
                           ↓
                    [RC Snubber]
                           ↓
[CLEAN GND] ←←←←←←←←←←←←←┘
      ↓
[MCU GND]
```

### Pin-Level Audit
| MCU Pin | Connected To | Signal Type | Max Current | Protection | Pull-up/down |
|---------|--------------|-------------|-------------|------------|--------------|
| GPIO32 | Stepper STEP | 3.3V PWM | 5mA | 100Ω series | Internal pulldown |

### System Identification Report
- Data collection description
- FOPDT model derivation
- Linearization points
- Validation plots
