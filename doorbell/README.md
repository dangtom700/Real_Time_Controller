# Anti-Rage Doorbell — prototype 1 bring-up log

Two ESP32-C6-DevKitC-1 boards, brought up one step at a time. Every env in
`platformio.ini` is one step, and each step ends with something you can see,
hear, or measure. This file records what each step actually measured on this
bench, so later steps have a baseline to be judged against.

Status: **steps 0–3 pass.** The panel hardware is now wired (buttons, buzzer,
mic, amp) and untested — buttons and buzzer are the next thing to bring up.

## The two boards

| | STA MAC | Role | Port (this machine) |
|---|---|---|---|
| Board A | `20:6E:F1:17:03:B8` | sender | COM9 |
| Board B | `20:6E:F1:17:07:40` | receiver | COM8 |

`LINK_PEER_MAC_INIT` in `include/config_v1.h` names **board B**, because it must
name the RECEIVER. The MAC is also the USB serial number, so `pio device list`
tells you which board is on which port without flashing anything.

## Current wiring

Both boards share one 5 V rail and a common ground, fed from a single USB port.

### Board A — guest node (door panel)

| Signal | Pin | Notes |
|---|---|---|
| Button 1 (doorbell) | **IO1** | to GND; LP_GPIO → deep-sleep wake |
| Button 2 (PTT) | **IO2** | to GND; LP_GPIO → deep-sleep wake |
| ~~Buzzer~~ | ~~IO3~~ | **moved to the Nano — see below** |
| Mic OUT (GY-MAX4466) | **IO6** | A6 / ADC1_CH6 |
| Mic VCC | **3.3 V** | *not* 5 V — see below |
| LED1 / LED2 | IO7 / IO5 | not yet wired |

Buttons are wired active-low to ground, so they need `INPUT_PULLUP`.

### Board B — host node (indoor chime)

| MAX98357A | Pin | Notes |
|---|---|---|
| VIN | 5 V rail | 10 µF + 100 µF local decoupling |
| GND | GND | |
| DIN | **IO4** | JTAG pin, noisy at boot — harmless, SD holds the amp muted |
| LRC | **IO11** | |
| BCLK | **IO10** | moved off IO8 — see below |
| SD | **IO3** + 10 kΩ to GND | HIGH = on (left channel), LOW = mute |
| GAIN | *unconnected* | = 9 dB, correct for voice |

### Transports

| Path | Transport | Status |
|---|---|---|
| A ↔ B (voice + events) | ESP-NOW, unicast, channel 1 | ✅ measured, 100% delivery |
| A ↔ B fallback | Zigbee / 802.15.4 | **events only — see below** |
| A → C (display, buzzer, wake) | simplex UART, 2 wires | designed, not built |

**Zigbee cannot carry the intercom.** It is a fallback for the doorbell *event*
path only, and the reason is a hard frame limit rather than something tunable:

```
VoiceMsg                224 bytes, 73/s  = 130 kbps
802.15.4 max PHY frame  127 bytes        -> ~100 usable after MAC headers
```

Every chunk would need splitting across three frames, each paying its own
header, CSMA backoff and ack, against a 250 kbps raw PHY that yields perhaps
50–100 kbps in practice — roughly half what voice needs. Events are a few bytes
seconds apart and fit easily. Halving the audio (8-bit companded, ~65 kbps) is
the change that would make voice-over-Zigbee viable.

### A → C link (simplex, two wires)

```
board A  IO0   ──────>  board C  D0 (GPIO44)   message, 115200 8N1
board A  IO10  ──────>  board C  D2 (GPIO5)    wake  ── 10 kΩ ── GND
board A  GND   ───────  board C  GND
```

Simplex because nothing C knows needs to travel back. **Two wires rather than
one** because C deep sleeps: a byte on RX cannot wake an ESP32-S3 from deep
sleep at all — only GPIO, timer and touch can — and even from light sleep the
first characters are consumed waking the UART. A single wire carrying both
"wake" and "here is the message" loses the message every time. Sequence is:
assert WAKE, wait ~300 ms for C to boot, then send.

Pin choices are not arbitrary. IO10 carries no strapping or JTAG role, so it
does not glitch while A boots — IO4–IO7 would, and a glitch on the wake line
wakes C for nothing. The 10 kΩ pulldown at C covers the window between reset and
the first `pinMode`, when every GPIO floats; same defensive pattern as the amp's
SD pin.

### Board C — Arduino Nano ESP32 (OLED + buzzer)

The buzzer would not sound from the C6's IO3. The same element, same wiring
topology (pin → buzzer → GND) and the same code path — `gpio_set_drive_capability`
→ `ledcAttach` → `ledcWriteTone` — produced three clean, clearly distinct tones
from the Nano's **D4**. That also confirmed the element is genuinely passive:
an active buzzer would have sounded one pitch three times.

Since the Nano was already carrying the OLED, the buzzer joins it there.

**Pin numbering trap on this board.** PlatformIO's manifest sets
`BOARD_USES_HW_GPIO_NUMBERS`, which turns *off* the Arduino pin remap that the
Arduino IDE uses. With it off, the constant `D4` resolves to **GPIO7**, not
GPIO4 — so `digitalWrite(4, …)` drives a pin that is not the one silkscreened
D4. Always use the `D4` symbol. The Nano also uploads over **DFU**: double-tap
RESET first, and expect the COM port to change.

Two consequences worth carrying forward:

- The doorbell beep is now a **message between boards**, not a local write, so
  it inherits that link's latency and failure modes.
- The panel can no longer make a sound while the C6 is the only thing awake,
  which cuts across the deep-sleep design in §2.5.

Unresolved: whether `ledcAttach` on IO3 was reporting `OK` or `FAILED` was never
captured, so it is not known whether IO3 can drive anything at all. Treat IO3 as
suspect before reusing it.

### The UART bench link is retired

Steps 1 and 2 used `board A IO1 → board B IO0` plus common ground. **BTN1 now
occupies IO1**, so that link and the button cannot coexist. This is acceptable —
the shipped transport is the radio, and the UART baseline is recorded below —
but the wire can no longer be re-measured for comparison without lifting the
button. Move BTN1 to IO7 if that fallback is wanted back.

### Pins that must not be used

- **IO8** — the DevKitC-1's addressable RGB LED (`PIN_RGB_LED 8` in the variant
  header, what `step0_hello` blinks) *and* a C6 boot-mode strapping pin. BCLK
  was originally wired here; a continuous bit clock into a strapping pin invites
  intermittent boot failures. It now sits on IO10 and LRC on IO11 — which of the
  two clocks got which pin does not matter, since C6 I2S signals reach their
  pads through the GPIO matrix. Leaving IO8 was the whole point.
- **IO9, IO15** — strapping. **IO12/IO13** — USB D-/D+. **IO16/IO17** — UART0
  console.
- **IO4–IO7** double as JTAG and are noisy at boot. Tolerable for LEDs and for a
  mid-rail-biased audio input; do not trust a reading taken during reset.

### Power budget

One USB port now feeds two ESP32-C6s plus the amp. Audio peaks run 600–700 mA
against 500 mA (USB 2.0) or 900 mA (USB 3.0). The 110 µF fitted is light for
that transient — a 470 µF bulk cap at the amp's VIN would be better.

The tell is already built in: `step0_hello` decodes `ESP_RST_BROWNOUT` as
`"BROWNOUT (bad power)"`. A board resetting during loud playback and reporting
that is a rail problem, not a code problem.

## Results

### Step 0 — does anything work at all?

Passes on both boards. ESP32-C6 rev 2, 1 core @ 160 MHz, 8 MB flash, Arduino
core 3.3.11, heap flat across a 13-beat heartbeat. 8 MB confirms the board
assumption in `platformio.ini`, so no `board_upload.flash_size` override is
needed.

### Step 1 — the wire

51 consecutive receipts, `crc=0`, `lost` frozen. The `pressCount` cycle
(`1,2,3,3,3,3`) arrived in step with the sender, which is the real result: the
`DoorbellMsg` struct crossed the wire with its field layout intact. 921600 baud,
`sizeof(DoorbellMsg)=8`.

### Step 2 — voice-rate traffic over that wire

The baseline everything else is judged against.

```
73 pkt/s   130 kbps   lost=0 (0/2911)   crc=0   bad=0   gap-max=13.7 ms
sender:    failed=0   resync=0
```

### Step 3 — the same two sketches over the radio

Only the `-D` changed. Receiver:

```
73 pkt/s   130 kbps   lost=0 (0/1558)   bad=0   ovr=0   gap-max=23.6–56.7 ms
```

Sender:

```
73 pkt/s   sent=1018   failed=0   resync=0   ack-ok=1018   ack-FAIL=0
```

**100% unicast delivery, zero loss.** The radio matches the wire on throughput
and loss; the cost is jitter — gap-max runs 2–4× the wire's. Since `lost=0`,
packets are bunching rather than dropping. Worst observed gap of 56.7 ms is 4.1
chunk periods, so roughly 5 chunks (~69 ms) of jitter buffer absorbs it.

### Step IO — buttons and buzzer

Board A only, no link involved. Out of numeric order because it needed the panel
wired, which happened after steps 0–3 had already passed.

```
pio run -e step_io -t upload -t monitor --upload-port COM9
```

Built; **not yet run on hardware.** PASS is: a three-note rising sweep at boot,
exactly one line per press (never a burst), `pressCount` walking 1, 2, 3 and then
locking, and BTN2 held 2 s printing `TALK`.

It reproduces on purpose the semantics step 1 faked. The `pressCount` that walked
`1,2,3,3,3,3` over the wire was synthetic; here it comes from a real thumb,
clamped by the same `RAGE_LIMIT` and expired by the same `TIMER1_MS`. If the
numbers behave the same way, the input layer can be dropped under the link with
nothing else changing.

Each press also reports how long the contact took to settle, so if bounce ever
outlasts `DEBOUNCE_MS` the log tells you what to raise it to rather than leaving
you to guess.

### Step 5 — the speaker — **PARKED, no audio confirmed**

Status in one line: the link side passed cleanly and **nothing was ever heard
properly**. The amp receives data, but not at a level anyone could judge. Read
*What the bring-up established* below before resuming — several plausible-looking
results from this session are worthless and it matters which.

Two envs, **run them in this order**:

```
pio run -e step_amp        -t upload --upload-port COMy   # 5a, board B alone
pio run -e voice_rx_chime  -t upload --upload-port COMy   # 5b, board B
```

**5a `step_amp` — the amp with no link behind it.** A locally generated 440 Hz
tone, 1 s on, 1 s off, forever. PASS is a clean A4 and then *true silence* — not
quieter, and not hissing. A Class D amp hissing through the gap has SD stuck
high. This env exists only to tell a dead amp apart from a broken audio path,
which is the question the buzzer cost a day to answer the hard way.

**5b `voice_rx_chime` — step 3's receiver plus `-DCHIME_AUDIO`.** The sender is
`voice_tx_espnow` exactly as step 3 flashed it, so board A needs no reflash and
the counters stay comparable to the step 3 baseline above. PASS is a clean,
steady A4 out of the speaker with the packet numbers still reading like step 3,
plus `i2s` tracking `rx` and `short=0`.

The report line gains `i2s=<chunks played> short=<blocked writes> ON|muted`.
`short` counts writes that still could not fit after two chunk periods, which
means the radio is delivering faster than 8 kHz drains it — clock drift between
two boards, not a link fault.

`voice_tx`'s comment calls the shot: a buzzing or warbling tone means the
transport, not the amp. One caveat it could not have anticipated — that verdict
only holds while `gap-max` stays under the 82.5 ms prefill. Past that the jitter
buffer is dry and the warble is the receiver starving its own DMA.

Sizing note: the prefill is **6 chunks (82.5 ms)**, taken from step 3's measured
23.6–56.7 ms gap-max, not step 2's 13.7 ms. The wire's figure would have been
too small for the radio, and an underrun sounds exactly like the transport
fault the test is meant to rule out. It still fits inside the 180 ms the I2S DMA
holds at 8 kHz.

#### Measured, 5b

Board B on COM8, board A running `voice_tx_espnow` and powered from the shared
rail. Steady state, held flat over 19 consecutive report lines:

```
72-74 pkt/s   129-132 kbps   lost=0 (0.0%)   gap-max=26.0ms
rx=1405   reorder=1   bad=0   ovr=18   i2s=1393  short=12  ON
```

**The audio path costs the link nothing.** Rate, bitrate and loss are
indistinguishable from the step 3 line measured without a speaker attached, and
`gap-max` of 26 ms sits inside the 23.6–56.7 ms band step 3 recorded — so
feeding I2S from `loop()` is not stealing time from the receive path.

`i2s + short == rx` exactly, which is the invariant worth checking: every chunk
that arrived was either played or explicitly counted as not played.

**This says nothing about the sound, and the speaker produced only hiss.** A
perfect `i2s=` count means chunks were handed to the driver, not that anything
came out. Those are separate claims and only the first one is measured here.

The startup transient is the fourth entry in *Reading the instruments* wearing a
new hat. `lost=36`, `ovr=18` and `short=12` all accrue in the **first second**
and then never move again: board B reset into a stream board A was already
transmitting, inherited a full link ring, and played the backlog at wire speed
instead of at 8 kHz until the DMA saturated. Self-clearing, and the counters
freezing is the proof. A `short` that keeps climbing would be the real fault —
the sender's clock running fast against board B's 8 kHz.

#### What the bring-up established — and what it did not

The speaker hissed, and hiss was read as a broken I2S configuration. Four
sketches went hunting for that: sample rate swept 8/16/32/48 kHz, slot mode left
then both, BCLK/LRC tried both ways round. **None of it changed the sound.**

What finally moved the needle was **amplitude**. A full-scale square wave, where
every earlier test had used a quarter-scale sine, was audible where the sine was
not — faintly, but unmistakably switching against silence with SD held steady.

So the digital path works. Established, and worth trusting:

- **DIN reaches the amp.** Data changes on IO4 change the sound.
- **The pin map as wired and as configured is correct**: `BCLK=IO10`,
  `LRC=IO11`, `DIN=IO4`. The verbal rewiring report was right.
- **SD on board B's IO3 works.** It enables and mutes the amp reliably — which
  is a real data point against the IO3-is-suspect note from the buzzer, though
  it says nothing about IO3 on *board A*.
- **A quarter-scale sine at 9 dB is inaudible on this speaker.** That is the
  entire original symptom.

**Now the trap.** Those four sweeps all ran at quarter amplitude, so every
negative result they produced is void. In particular, **8 kHz was never fairly
tested** and may be fine. The BCLK-floor theory that motivated 48 kHz rests on
third-party sources, not on a measurement made here and not on the datasheet,
which lists 8 kHz as supported. Do not treat "8 kHz does not work" as known.

That leaves the two sketches deliberately inconsistent:

| | `step_amp` | `voice_rx_chime` |
|---|---|---|
| rate | 48 kHz | 8 kHz |
| slots | stereo, duplicated | mono, left |
| amplitude | ~85% FS | whatever `voice_tx` sends (25% FS) |

`step_amp`'s settings are a **precaution, not a finding**. Re-test 8 kHz mono at
full amplitude before adding an upsampler to the chime path — if 8 kHz works,
`voice_rx_chime` needs no rate change at all and keeps its 180 ms of DMA
cushion, which at 48 kHz collapses to 30 ms and would no longer cover the
measured 26 ms `gap-max`.

Two untried levers, cheapest first:

1. **GAIN → GND**: 9 dB becomes 15 dB. One wire, and what the reference design
   this bench was checked against does. GAIN is currently unconnected.
2. **Amplitude**: `voice_tx` sends `kAmp = 8000`, a quarter of full scale. That
   headroom made sense for a link test and costs 12 dB in a listening test.

An **LM386 analog path was also tried** and produced audible but rough output.
That is not a reason to switch: the C6 has no DAC, so an analog amp must be fed
PWM through an RC reconstruction filter, and "raw and rough" is precisely the
outcome that argument predicts. It does confirm the speaker itself is fine.

**The methodological lesson, which is the expensive part:** through a speaker,
"silent" and "too quiet to pick out" are the same observation with completely
different causes. Establish that the path carries *anything* — loudest possible
signal against zeros, enable pin held steady so it cannot confound — before
changing a single I2S parameter. Doing it the other way round cost four sketches
and produced four meaningless results.

`ON` confirms SD is being driven high on **IO3 of board B**, which is worth
noting given IO3 on board A is still the suspect pin from the buzzer.

## Reading the instruments

Four numbers look like faults and are not:

- **A large `LOST n` (step 1) or `reorder=1` (step 2/3) right after the receiver
  starts.** The receiver joined a stream already in progress. `voice_rx`
  deliberately books a jump ≥ 1000 as a reorder rather than as thousands of
  phantom lost packets. What matters is that the counter then stops
  incrementing.
- **`gap-max` excursions on UART (13.7 → ~19 ms).** These track the sender's
  `late-max` almost exactly. Both sketches `Serial.printf` once per second, and
  over USB CDC that print lands inside the measured interval. The instrument is
  measuring itself.
- **`ack-ok` sitting one above `sent`.** The `MSG_VOICE_BEGIN` packet is acked
  but is not a chunk, so it is not counted in `sent`. Small lags the other way
  are the send callback being asynchronous.
- **The receiver printing its banner and then nothing.** See below.

## Gotchas that cost real time here

**The link is not on the pins silkscreened `TX`/`RX`.** Those are UART0, the
serial console (GPIO16/17), which `config_v1.h` says never to reuse. The link is
`IO1` → `IO0`. Wiring the silkscreened pins ties the two consoles together:
harmless, carries no traffic, and the receiver prints its banner then goes
silent — indistinguishable from a missing ground.

**Never flash the sender onto board B.** `LINK_PEER_MAC_INIT` names board B, so
`voice_tx_espnow` on board B transmits to its own MAC. The signature is
`ack-ok` at *exactly* zero (a weak link still gets some acks) with `failed`
climbing ~69/s while `sent` limps at 4–5/s.

**Flash and monitor the receiver first, then the sender.** Opening a monitor
resets that board, so doing the sender last keeps its reboot out of the
receiver's measurement window.

**A monitor opened right after upload misses `setup()`.** On native USB,
`Serial` is `HWCDCSerial`, which discards output when no host is attached.
After a reset the USB device re-enumerates, which takes longer than the
`delay(400)` in `setup()`. Press the board's RESET button with the monitor
already open to see the banner. `voice_rx` and `step1_ping_rx` only print on
receipt otherwise, so a healthy board can look completely dead.

## Toolchain notes for this machine

**`core_dir = C:\pio`, not the default.** `esp32-arduino-libs` carries an
ESP-Matter header 203 chars deep inside the tarball. Under the default core dir
the staging path pushes it to 265 > 260 (`MAX_PATH`) and the install dies
mid-unpack with a `FileNotFoundError` that reads like a PlatformIO bug.
Enabling `LongPathsEnabled` would still leave ~6 chars of headroom for
`riscv32-esp-elf-gcc`, which is MinGW-built and not manifest-aware.

**Flash over the port silkscreened `USB`, not `UART`.** The UART port's CP2102N
enumerates but sits at `ConfigManagerErrorCode 28` — Windows ships no CP210x
driver, so no COM port is created. The native USB-Serial-JTAG port needs no
driver, but requires BOTH `-DARDUINO_USB_CDC_ON_BOOT=1` and
`-DARDUINO_USB_MODE=1`: the C6 has no USB-OTG, so `MODE=0` resolves `Serial` to
`USBSerial`, which does not exist on this chip and fails to compile.

**Do not let the VSCode PlatformIO extension install concurrently with a CLI
build.** Saving `platformio.ini` triggers a re-init; racing a build once
stranded the 2.4 GB toolchain in a `.tmp` dir and left
`packages/toolchain-riscv32-esp` missing entirely.

## Audio parts — before steps 4 and 5

| Path | Part | Interface | Supply |
|---|---|---|---|
| Mic (guest node) | GY-MAX4466 | **analog** → ADC on IO4 | **3.3 V** |
| Amp (host node) | MAX98357A | I2S TX on IO10/11/4 | 5 V rail is fine |

**Power the MAX4466 from 3.3 V, not 5 V.** It biases its output at VCC/2 and
swings around it, so at 5 V the signal centres on 2.5 V with peaks toward 5 V —
into an ADC pin whose absolute max is ~3.6 V. Its supply range is 2.4–5.5 V, so
3.3 V is well in spec and centres the output at 1.65 V, inside the ADC's
0–3.1 V window at 11 dB attenuation.

**The mic must sit on GPIO0–6.** The C6 has one ADC unit and only those seven
pins are analog-capable. IO6 is the one used, and it is a better choice than the
IO4 first written into the config: IO4 doubles as JTAG MTMS and is noisy at
boot, while IO6 carries no strapping or JTAG role. Dropping the I2S mic clocks
frees IO10/IO11 on the guest node, taking §10.4's budget from 14-of-16 to
12-of-16.

**SD must be wired; floating is not a setting.** SD_MODE is enable *and* channel
select — under 0.16 V is shutdown, 0.16–0.77 V right, 0.77–1.4 V (L+R)/2, above
1.4 V left. Left floating, behaviour depends on whatever resistor the particular
breakout carries. It is driven from IO3 with a 10 kΩ pulldown: 10 kΩ rather than
100 kΩ because many breakouts have a ~1 MΩ pullup to VIN, against which 100 kΩ
divides to ~0.45 V — inside the *right channel* band, so the amp would come up
enabled on the wrong channel instead of muted. The pulldown also holds the amp
off through boot, before `setup()` runs, which removes the power-on pop.

Because SD selects the **left** channel, the I2S config must place samples in
the left slot. Right-slot data into a left-configured amp is silent, and looks
identical to the floating-SD failure.

**Keep the amp digital.** The C6 has no DAC (verified: `soc_caps.h` defines no
DAC symbols; `SOC_ADC_PERIPH_NUM` is 1 with 7 channels). An analog amp like the
LM386 would need PWM through an RC reconstruction filter; I2S into the
MAX98357A avoids that entirely.

Neither change touches `protocol.h` or the link layer. `VoiceMsg` still carries
`int16` PCM at 8 kHz — the mic path becomes 12-bit ADC scaled to `int16`, and
the amp path stays I2S. Everything steps 1–3 proved still holds.
