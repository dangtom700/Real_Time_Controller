# Anti-Rage Doorbell — Mega 2560 bench rig

One Arduino Mega 2560 stands in for both nodes so every component is proven on
known-good 5 V hardware before the design is compacted onto an ESP32.

```
Doorbell_Sender_Side/     "door panel"    buttons + OLED + buzzer + state machine
Doorbell_Receiver_Side/   "indoor chime"  buzzer + LM386/speaker + 2 status LEDs
Doorbell_Shared/          protocol.h · config_mega.h · sim_link.h · synth.h
```

`Doorbell_Shared/` is single-sourced — both `platformio.ini` files pull it in with
`build_flags = -I../Doorbell_Shared`. `protocol.h` is byte-identical to
`Anti-rage doobell/include/protocol.h`; keep it that way or the port breaks.

## Wire it ONCE, then just swap sketches

The two sketches share exactly one pin — **D8, the buzzer** — and use it the same
way. Nothing else overlaps, so you can build the whole bench on one breadboard and
flash whichever side you want to test without rewiring.

| Pin | Sender uses it as | Receiver uses it as |
|----|----|----|
| D2  | button 1 (doorbell) | — |
| D3  | button 2 (intercom PTT) | — |
| D4 · D6 · D7 · D9 · D10 | TFT SDCS · BL · RST · D/C · CS | — |
| **D8** | **passive buzzer** | **passive buzzer** |
| D11 | — | LM386 audio out (Timer1 PWM) |
| D50 · D51 · D52 · D53 | hardware SPI for the TFT | — |
| D22 | — | LED 1, doorbell status |
| D24 | — | LED 2, voice status |

---

## Sender side

### Buttons — D2, D3

No resistors needed. The firmware uses `INPUT_PULLUP`, so a button just shorts the
pin to ground.

```
D2 ----[ button 1 ]---- GND
D3 ----[ button 2 ]---- GND
```

> **Porting note.** The ESP32-S3 node needs the *opposite* — active-HIGH with
> **external pull-down resistors** — because `ext1` deep-sleep wake only offers
> `ANY_HIGH`. That is a wiring change, not a logic change: flip `BTN_PIN_MODE`
> and `BTN_ACTIVE_LEVEL` in `config_mega.h` and the firmware is already correct.

### Passive buzzer — D8

```
buzzer (+) ---- D8
buzzer (-) ---- GND
```

Must be a **passive** buzzer (the bare piezo disc kind). An *active* buzzer has its
own oscillator and will just squeal at one pitch — `tone()` cannot play the
ding-dong through it. If yours makes a sound the instant you apply DC, it's active.

### 2.2" 240×320 TFT, ILI9341 — hardware SPI

The back of this breakout reads **`VIN: 3.3-5V · LOGIC: 3.3-5V · Backlight: PWM safe`**.
That marking is what makes the wiring easy: the board carries its own regulator
*and* level shifters, so every line goes **straight to the Mega with no dividers**.

> A bare ILI9341 panel *without* that `LOGIC: 3.3-5V` marking is 3.3 V logic only,
> and 5 V on its signal pins will damage it — often not immediately, which makes it
> a miserable fault to trace. Read the back of the board rather than assuming from
> the controller name.

```
   TFT          Mega
   VIN   <----  5V
   GND   <----  common ground rail
   SCK   <----  D52    hardware SPI, fixed
   MOSI  <----  D51    hardware SPI, fixed
   MISO  ---->  D50    hardware SPI, fixed
   CS    <----  D10
   D/C   <----  D9
   RST   <----  D7
   SDCS  <----  D4     microSD on the same board - wire it, see below
   BL    <----  D6     PWM-safe backlight
```

- **`SDCS`** is the microSD's chip select. The firmware does not read the card yet
  (that is milestone 4, real GIF playback) but it **does** park SDCS high at boot,
  because an unselected card with a floating CS will drive MISO and corrupt the
  display's traffic. Wire it even though it is otherwise idle.
- **`BL`** is documented PWM-safe, so the firmware `analogWrite()`s it and ramps the
  backlight up and down across the `SIM_SLEEP` transition instead of snapping it
  off. D6 is Timer4, which nothing else in this sketch touches. Set brightness with
  `TFT_BL_ON` in `config_mega.h`.
- **D53** is the Mega's hardware SS. We drive CS ourselves, but if D53 is left as an
  input the SPI peripheral silently drops out of master mode, so the firmware pins
  it high. Nothing to wire.

SPI runs at `TFT_SPI_HZ` = 8 MHz, the Mega's ceiling (F_CPU/2). Torn or speckled
pixels would mean long jumpers or poor breadboard contacts — drop to 4 MHz before
suspecting anything else.

At boot the sketch prints `ILI9341 self-diag = 0x…`. `0xC0` is a healthy panel;
`0x00` means MISO is unwired or the display is not responding. It is information,
not a gate — the display is effectively write-only here, so the firmware never
refuses to run on it.

---

## Receiver side

### Status LEDs — D22, D24

Standard current-limiting resistor, **220 Ω–330 Ω**.

```
D22 ----[ 220R ]----|>|---- GND     LED 1  doorbell  (pulse per ring, SOLID = rage-locked)
D24 ----[ 220R ]----|>|---- GND     LED 2  voice     (on for the intercom session)
```

Long leg (anode) toward the resistor/pin, short leg (cathode) to GND.

### LM386 + 8 Ω speaker — D11  ← the part worth reading

D11 is **not** an analog output. Timer1 drives it as a 62.5 kHz PWM square wave
whose duty cycle is the audio sample. Two things must happen before it reaches the
LM386: the carrier has to be filtered off, and the 5 V swing has to be knocked
down, because the LM386 has a gain of 20 and 5 Vpp on its input is a wall of
clipping.

The common LM386 mini-amp board has a **4-pin input header** (`VCC · IN · GND · GND`)
and a **separate 2-pin terminal** on the opposite edge for the speaker. It also
carries a 10 k pot on the input — that pot is your attenuator — and usually an
input coupling cap of its own.

```
                  R1 1k              C2 10uF
   D11 -----------/\/\/\------+------| |------> LM386 module  IN
                              |      +    -
                          C1 ===  33nF
                              |
   GND ------------------------+-------------> LM386 module  GND
                                                    (common ground - required)

   LM386 module  VCC  <---- +5 V from a SEPARATE supply   (see power note)
   LM386 module  GND  <---- the 2nd GND pin is tied to the first on the board;
                            wiring one is enough
   LM386 module  SPK terminal ----> 8 ohm 1 W speaker (either polarity)
```

- **R1 + C1 form the reconstruction low-pass.** 1 kΩ + 33 nF (marked `333`) puts the
  corner at ~4.8 kHz: it passes the whole 4 kHz audio band (the Nyquist limit of the
  8 kHz sample rate) while pushing the 62.5 kHz carrier ~22 dB down. If all you have
  is **0.1 µF**, use it — the corner drops to ~1.6 kHz so it sounds telephone-muffled,
  but it is perfectly safe and the carrier rejection is even better.
- **C2 blocks DC.** Idle output sits at half duty (2.5 V average); without the
  coupling cap that offset rides straight into the amp. Keep C2 even though the
  module likely has its own — two in series just halves the capacitance, which still
  leaves the high-pass corner near 3 Hz. Polarity: **+ toward R1**, since the junction
  sits at ~2.5 V DC and the module side does not.
- **Set the module's volume pot to MINIMUM before powering on**, then bring it up
  during the boot self-test. D11 swings a full 5 Vpp and the LM386's gain is 20.
  If your board has no pot, add a divider after C2: 10 kΩ to the input, 470 Ω to ground.

Breadboard order: D11 → a row; **R1** from that row to a second row (the *junction*);
**C1** from the junction to the ground rail; **C2** from the junction to a third row,
stripe away from the junction; jumper that row to `IN`. Then tie Mega GND, module GND
and supply − all onto the same rail.

> **Power.** An 8 Ω speaker at any real volume pulls current in the hundreds of mA.
> The Mega's onboard regulator, especially on USB power (500 mA total for the whole
> board), will brown out and reset the board mid-chime. Feed the LM386's VCC from a
> **separate 5 V supply** — the breadboard power-supply module in your BOM is
> exactly right — and **tie the grounds together**. Common ground is not optional;
> without it the audio has no return path and you get noise or nothing.

> **Do not `analogWrite()` on D11 or D12** in this sketch. Both are Timer1's, and
> Timer1 is the audio carrier.

---

## Timer budget (receiver)

All four in use, none fighting — worth knowing before you add anything:

| Timer | Owner | Why it matters |
|---|---|---|
| Timer0 | `millis()` / `delay()` | untouched |
| Timer1 | 8-bit fast PWM on OC1A/D11 | the 62.5 kHz audio carrier |
| Timer2 | `tone()` | the buzzer |
| Timer3 | CTC @ 8 kHz | the PCM sample clock ISR |

---

## Bring-up order

```
pio run -t upload -t monitor      # from inside either project folder
```

**1. Receiver first** — it self-tests on boot with no input needed.

| Step | Do | PASS |
|--:|----|------|
| 1 | power on | both LEDs alternate 3× |
| 2 | — | buzzer plays ding-dong |
| 3 | — | a clean 440 Hz tone from the speaker, no buzz or hiss |
| 4 | type `d` | ding-dong + LED 1 pulses |
| 5 | type `l` | **silence** + LED 1 goes solid (rage lockout) |
| 6 | type `v` | 1 s tone through the LM386, LED 2 on |

If step 3 buzzes harshly, the carrier is leaking — your C1 is too small or missing.
If it is distorted, turn the module's pot down.

**2. Sender second.**

| Step | Do | PASS |
|--:|----|------|
| 0 | power on | `READY` on a black screen; self-diag prints `0xC0` |
| 1 | press button 1 | ding-dong, a green `1` + one filled pip, one `#…` line printed |
| 2 | press again | the count goes orange |
| 3 | press a 3rd time within 30 s | **silent**, the screen goes fully red with `WAIT.`, frame carries `locked=1` |
| 4 | wait 30 s | window resets, sound returns, screen back to `READY` |
| 5 | hold button 2 for 2 s | navy `INTERCOM` screen, VOICE frames, then `CLOSED` on release |
| 6 | idle 30 s | `SIM_SLEEP`; backlight off; any press wakes and counts as press #1 |

**3. Bridge the two.** Copy a `#…` line from the sender's monitor, flash the
receiver, paste it in. Same bytes, same struct, same reaction — that is the whole
point of the exercise.

---

## What this bench does *not* prove

Stated plainly so nothing is assumed twice:

- **The radio.** There isn't one on a Mega. `sim_link.h` moves the identical bytes
  over serial instead. ESP-NOW range, channel and ack-rate testing all wait for the
  ESP32 pair — `diag_espnow_tx` / `diag_espnow_rx_led` in the other project.
- **Deep sleep.** AVR power-down and the ESP32-S3's `ext1` wake share no semantics,
  so proving one proves nothing about the other. Idle here is a logged `SIM_SLEEP`
  state that keeps the state machine's shape without pretending to test power.
- **A microphone.** There is none on this bench, so "voice" is a synthesised 440 Hz
  sine packed into real `VoiceMsg` payloads at the true 8 kHz rate. The packet
  sizes, the sample clock and the whole speaker chain are genuinely exercised —
  only the acoustic source is fake. On the ESP32, `i2s_read()` replaces the
  synthesiser and nothing downstream changes.

The sender prints the sustained rate the real link will have to carry when you
close an intercom session (~76 pkt/s × 224 B ≈ 136 kbps). That is the number to
check `diag_espnow_tx`'s ack rate against before trusting audio over the radio.
