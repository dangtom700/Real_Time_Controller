# Anti-Rage Doorbell — Plan v2

Written 2026-07-29. Supersedes the milestone list in `Anti-rage doobell/README.md`.

This plan treats the four diagrams in [`Anti-rage doobell/img/`](Anti-rage%20doobell/img/) as
**ground truth** and reconciles them against (a) the code that actually exists and (b) the six
MCU datasheets now sitting in the repo root. Where the code and the diagrams disagree, the
diagram wins unless flagged under *Open decisions*.

---

## 1. What exists today

Two parallel codebases, both real, neither matching the diagrams:

| Tree | Target | State |
|---|---|---|
| `Anti-rage doobell/` | Nano ESP32 + ESP8266, ESP-NOW | M1/M2 working, M3 intercom is a `TODO` scaffold, M4 GIFs not started. 12 diag envs, all compiling. |
| `Doorbell_{Sender,Receiver}_Side/` + `Doorbell_Shared/` | one Mega 2560, serial-as-radio | **More complete on logic than the ESP build.** Full state machine, real 8 kHz PCM engine (Timer1 PWM carrier + Timer3 CTC clock), ILI9341 TFT, LM386 chain, rage lockout end-to-end. |

The Mega bench is good work and is not wasted — see §6. But it proves neither the radio nor
sleep, and it cannot drive the screen at the rate the diagrams need.

---

## 2. What the diagrams say that the code does not

Read in order of how much they change the design.

### 2.1 ESP-NOW is brought up and torn down per intercom session

`system workflow.png` has two explicit boxes the firmware has no equivalent of:

- **"Set up ESP-NOW protocol"** — reached only from *Trigger timer 2*, i.e. the button-2 path.
- **"Kill ESP-NOW protocol"** — reached from *Button 2 is pressed? → No*.

Today, [`door_panel/main.cpp:112-123`](Anti-rage%20doobell/src/door_panel/main.cpp#L112-L123)
calls `espnowSetup()` once in `setup()` and never deinitialises. The radio is the single largest
power consumer on the board; the diagram wants it off unless someone is talking. This is a
lifecycle change, not a feature.

The intended lifecycle, confirmed: **the link is alive for the duration of a live session, and
is re-initialised after every wake.** Deep sleep destroys the radio state anyway — RAM is lost
and the MAC is torn down — so `init` after wake is not a design choice, it is the only thing
that can happen. The design choice is that it does *not* stay up between sessions.

### 2.2 The doorbell *does* cross the wall — settled by the peripheral list

Follow the workflow diagram's doorbell branch: *Timer is on → Play "Ding Dong" once* and
*Set press counter +1 → if >3 → GIF*. It terminates at the screen, and no ESP-NOW box appears
on that path in any of the three logic diagrams. I had this open as the top design question.

**It is closed by the hardware allocation (§2.7): the host has a buzzer.** The only thing that
would ring it is a guest button press, so `MSG_DOORBELL` stays and the radio must come up for a
press as well as for a voice session. The diagrams simply did not draw the notification.

### 2.3 The rage lockout does not mute the chime

`button logic.png`: *Timer is on → Yes* forks unconditionally into **both** *Play "Ding Dong"
once* **and** *Set press counter +1 (limit of 3)*. There is no gate on the sound. Escalation is
expressed entirely through the screen: **`>3` → Play GIF#3**, otherwise **Play a GIF from a
presetted map**.

The firmware does the opposite — [`main.cpp:63-68`](Anti-rage%20doobell/src/door_panel/main.cpp#L63-L68)
early-returns from `playDingDong()` once `locked` is set, and the Mega bench mirrors it. Under
the diagrams the punishment is *visual*, and the guest still gets their ding-dong.

### 2.3.1 Three presses, three ding-dongs, three GIFs — one clamped index

Settled: **the press limit, the ding-dong count and the GIF count are all 3, by design.** Press
*n* inside timer 1 plays ding-dong *n* and GIF *n*. That collapses the workflow diagram's
two-way branch — *if >3 → Play GIF#3* versus *Play a GIF from a presetted map* — into a single
clamped lookup:

```c
playDingDong();                       // every press, never muted (2.3)
showGif(gifMap[min(pressCount, GIF_COUNT) - 1]);
```

with `RAGE_LIMIT == GIF_COUNT == 3`. They are not two constants that happen to match; they are
one constant. GIF#3 is not a special "rage" case in the code — it is simply the last entry, and
the clamp is what makes a 4th, 5th and 6th press keep landing on it. This also answers open
decision 3: the diagram's `>3` is `>= 3` after clamping, and it needs no separate branch.

### 2.4 GIF playback is the main path, not a stretch goal

In all three logic diagrams the terminal action of the button branch is *Play GIF#3* or *Play a
GIF from a presetted map*. The OLED showing `!!!` and a bouncing 8×8 block
([`main.cpp:71-90`](Anti-rage%20doobell/src/door_panel/main.cpp#L71-L90)) is a placeholder that
does not satisfy this. The current README files it as **M4 — polish (stretch)**. It should be
M2.

This is also the requirement that decides the hardware (§3).

### 2.5 Two timers, deliberately different durations

There are exactly two timers, and they are **not** meant to have the same value. They exist to
give the two features independent activation windows:

| | Role | Default | Current constant |
|---|---|---|---|
| **Timer 1** | Doorbell activation window. Max **3** presses inside it. On expiry → sleep. | **30 s** | `RAGE_WINDOW_MS` **and** `IDLE_SLEEP_MS`, both 30000 |
| **Timer 2** | Voice session window. Bounds one intercom conversation. | **3 min** | `INTERCOM_MAX_MS` = **15000** ← wrong |

Two corrections follow:

- **`INTERCOM_MAX_MS` must go 15 s → 180000.** A 15-second cap cannot hold a conversation; it
  is a safety timeout masquerading as a feature window. Timer 2 ends a session when it expires
  *or* when button 2 is released, whichever comes first — 3 min is the cap, not the expectation.
- **Timer 1 is one timer with one value.** `RAGE_WINDOW_MS` and `IDLE_SLEEP_MS` are the same
  diagram box (*Timer is on → No → … → System asleep*). Collapse them to a single constant so
  the press window and the sleep deadline cannot silently drift apart.

`PTT_HOLD_MS` (2 s) is **not** a third timer — it is the gesture threshold that *triggers*
timer 2, which is exactly how `system workflow.png` draws it: *Button 2 is hold 2 seconds? →
Trigger timer 2*.

### 2.6 ESP-NOW liveness gates sleep

*Timer is on → No* does not go straight to sleep; it goes to **"ESP-NOW alive?"** first. The
current code gets the same behaviour by accident (the sleep check is inside
`if (state == ST_ACTIVE)`). Make the condition explicit — it will stop being accidental as soon
as the radio has a lifecycle (§2.1).

### 2.7 The peripheral allocation — settled

| | **Guest** (door panel) | **Host** (indoor) |
|---|---|---|
| Screen | ✅ ILI9341 | — |
| Buzzer | ✅ | ✅ |
| Buttons | ✅ ×2 | **none** |
| Microphone | ✅ INMP441 | — |
| Speaker | — | ✅ MAX98357A |
| Status LEDs | nice-to-have | nice-to-have |

Four consequences, none of them cosmetic:

1. **Voice is simplex, not half-duplex.** The guest has no speaker and the host has no mic, so
   audio flows guest → host only. `voice logic.png` agrees — it draws exactly one direction,
   *mic → protocol → amp → "Voice 2 other end"*. The README's "half-duplex PTT" framing is an
   over-design: there is no direction to arbitrate. Drop the talk-back scaffold and the
   `MSG_VOICE_*` receive path on the guest.
2. **The link is unidirectional.** Guest transmits `MSG_DOORBELL` and `MSG_VOICE_*`; the host
   only ever receives. No reverse channel except transport-level acks.
3. **The host has no wake source.** With no buttons, nothing can wake it, and a doorbell press
   is unannounced — so **the host cannot deep-sleep.** It must stay awake (or in modem-sleep)
   listening for the radio, which means **the host is mains/USB-powered**. Only the guest
   sleeps. The diagrams' *System asleep* box belongs to the guest alone.
4. **The two nodes are not symmetric** in hardware or firmware. Guest = state machine +
   timers + mic + screen; host = a sink that rings a buzzer and plays a stream. This weakens
   the symmetry case in §9.1 — see the correction there.

Both buzzers are LEDC/`tone()` on one GPIO each. The guest's is local feedback (*you rang*);
the host's is the actual chime. They are not the speaker path — the host's MAX98357A carries
voice only.

---

## 3. Hardware: what each board is actually good for

Read from the datasheets in the repo root.

| Board | MCU | Clock | RAM | Flash | Radio | The one fact that matters |
|---|---|---|---|---|---|---|
| **Arduino Nano ESP32** | ESP32-S3 (NORA-W106-10B) | 2×240 MHz LX7 | 512 KB + **8 MB PSRAM** (octal) | 16 MB | WiFi/BLE | Only board here with the RAM *and* the bandwidth for GIF playback. **2 I2S controllers.** |
| **ESP32-C6-WROOM-1** | ESP32-C6 RISC-V | 160 MHz | 512 KB HP + 16 KB LP | 4/8/16 MB, **no PSRAM** | WiFi 6, BLE 5.3, 802.15.4 | 7 µA deep sleep with wake on **LP_GPIO0–7 only**. I2S on any GPIO. |
| **ESP8266MOD** | ESP8266EX | 80/160 MHz | ~50 KB usable heap | 4 MB typ. | WiFi | **I2S-out data pin is GPIO3 = U0RXD.** Turning on audio output costs you the serial console. |
| **Adafruit Metro M4 Express** | ATSAMD51J19A | 120 MHz M4F | 192 KB | 512 KB + **2 MB QSPI** | **none** | No radio ⇒ no deadlines to miss. 192 KB RAM ≈ one 240×320 framebuffer. **The display MCU** (§3.1). |
| **Seeeduino V4.2** | ATmega328P | 16 MHz | **2 KB** | 32 KB | none | 2 KB of RAM. A single `VoiceMsg` is 224 B. Cannot be a node. |
| **Arduino Mega 2560** | ATmega2560 | 16 MHz | 8 KB | 256 KB | none | The existing bench. 8 MHz SPI ceiling ⇒ ~1 MB/s to the TFT. |

### 3.1 The screen wants its own MCU — pins first, then timing

The ILI9341 + microSD breakout needs **10 wires**: `VIN · GND · SCK · MOSI · MISO · CS · D/C ·
RST · SDCS · BL` — 8 of them signals. Budget the door panel with the screen attached:

| Function | Signals |
|---|---|
| Button 1, Button 2 | 2 |
| ILI9341 + microSD | **8** |
| INMP441 mic (I²S RX) | 3 |
| Buzzer | 1 |
| Status LEDs (nice-to-have) | 2 |
| **Total** | **16** |

A C6 module has 16 usable GPIOs after reserving strapping (IO8/9/15), USB (IO12/13) and the
console (IO16/17). **16 of 16** — it fits with nothing left, and only if every I²S and SPI pin
lands somewhere legal. The Nano ESP32 is roomier at 20, but not by much.

The pin argument alone would be survivable. The timing argument is not.

**The timing argument — weaker than I first claimed.** The audio path has a **13.75 ms** deadline
(one `VoiceMsg` = 110 samples at 8 kHz), and a 150 KB full-frame repaint is ~15 ms of SPI even at
80 MHz, so on one MCU every screen update is a candidate to blow a chunk. *But* GIF playback
hangs off **button 1** and voice streaming off **button 2**, and the diagrams never run them
together — during a voice session the screen shows a static banner, which is negligible SPI
traffic. The two workloads are very nearly mutually exclusive, so this is a robustness argument,
not a correctness one. Stated honestly so it is not doing more work than it can bear.

**The deciding argument was GIF storage — and §10 resolves it in favour of *not* splitting.**
Raw frame storage is brutal (13.5 MB for three full-screen animations), but real LZW-compressed
`.gif` files are ~20× smaller and a C6 holds a full framebuffer in SRAM. See §10 for the spec
and the consequence: **one C6 drives the panel directly.**

**So: give the screen its own MCU.** §9 argues that MCU should be the Nano ESP32.

```
   guest node (C6)                      display MCU (S3)
   buttons, mic, buzzer, radio  --TX-->   ILI9341
   9 signals of 16                        7 signals, GIFs in 16 MB flash
```

The guest node drops to **9 signals** (2 buttons + 3 mic + 1 buzzer + 1 UART TX + 2 LEDs) out of
16, and the display workload becomes physically incapable of stealing time from the audio
deadline. Both boards are 3.3 V logic, so no level shifting.

The command channel is **one byte — "play animation *n*"** — and since the display never needs
to answer, it is **TX-only: a single wire plus ground.** It reuses the framing seam of §4.3.

### 3.1.1 ~~The S3 still earns the door panel~~ — retracted

I argued here that the door panel needed the S3 because the C6 has only one I²S controller
against the S3's two, and a panel needs both a mic and an amp. **That is wrong.** The C6
datasheet's I²S feature list reads:

> Full-duplex and half-duplex communications · **Separate TX and RX units that can work
> independently or simultaneously**

INMP441 and MAX98357A both slave to the same BCLK and WS, so one C6 I²S controller runs both at
once on 4 pins (BCLK, WS, DIN, DOUT). With the display already moved off (§3.1), **nothing
requires the S3 on a node.** See §9 for what that opens up.

### 3.2 Why the ESP8266 should be retired from the voice path

Three independent reasons, all from the datasheet:

1. **The I2S-out pins are fixed and hostile.** Table 4-5: `I2SO_BCK = GPIO15`,
   `I2SO_WS = GPIO2`, `I2SO_DATA = GPIO3`. GPIO3 is `U0RXD` — the serial console's receive
   line. GPIO15 and GPIO2 are both boot-strapping pins. You would be debugging the hardest
   milestone in the project (live audio) with a crippled console.
2. **~50 KB usable heap** with the WiFi stack up, versus 512 KB on the C6.
3. **Deep sleep needs the GPIO16→EXT_RSTB wire hack** and comes back through a full reset with
   all RAM lost. The C6 does 7 µA with LP memory retained.

Swapping in the **ESP32-C6-WROOM-1** fixes all three and keeps ESP-NOW. Keep the ESP8266 as a
spare v1 chime — `diag_espnow_rx_led` and `diag_chime_buzzer` still have value as a
known-good link target.

> **Cost of the swap:** the C6 needs IDF 5.1+, i.e. **arduino-esp32 3.x**. The project is
> currently pinned to 2.0.17 (IDF 4.4). See §4.
>
> **Do not expect WiFi 6 to help the audio link.** ESP-NOW runs on legacy 802.11b/g rates
> regardless; the C6's 802.11ax support is irrelevant here. The bitrate risk in §5 is unchanged.

### 3.3 The board that is not in the plan

**Seeeduino V4.2** — 2 KB RAM rules it out as a node. Useful only as a button/LED jig, and the
Mega already does that. Its 3.3 V/5 V switch makes it a convenient level-safe test harness for
3.3 V peripherals if you need one.

---

## 4. Proposed architecture — Plan A

> With **two C6 boards** on hand this is no longer the recommended allocation. Plan A is kept
> because its pin maps, timings and link design are unchanged by the choice. **See §9 for
> Plans B and C, and the recommendation.**

```
┌─ GUEST / door panel ──────────── ESP32-C6  (battery, deep-sleeps) ─┐
│  Button 1 doorbell   Button 2 PTT   Buzzer   INMP441 mic (I2S RX)  │
│  2 status LEDs                                                     │
│  Owns: press counter, timer 1 (30 s), timer 2 (3 min), the link    │
│                          9 of 16 signals                           │
└──────────────┬─────────────────────────────────┬───────────────────┘
               │ 1 wire, TX-only                 │  the wall link
               │ "play animation n"              │  SIMPLEX, guest -> host
               v                                 v
┌─ DISPLAY ─ Nano ESP32 / S3 ──┐   ┌─ HOST / indoor ─ ESP32-C6 ─────┐
│  2.2" 240x320 ILI9341        │   │  Buzzer      (the chime)       │
│  GIF library in 16 MB flash  │   │  MAX98357A + speaker (I2S TX)  │
│  no microSD, no radio        │   │  2 status LEDs                 │
│  no deadlines to miss        │   │  no buttons -> never sleeps,   │
│  7 signals                   │   │  mains powered.  6 of 16       │
└──────────────────────────────┘   └────────────────────────────────┘
```

### 4.1 Pin constraints that are not negotiable

**ESP32-C6-WROOM-1** (29 pins; available GPIOs 0–13, 15–23):

| Reserve | Pins | Why |
|---|---|---|
| Deep-sleep wake | **IO0–IO7 only** | These are the only pins carrying `LP_GPIOn`. Buttons must land here. |
| Strapping — avoid | IO8, IO9, IO15 | Boot mode / ROM-log control. |
| USB — avoid | IO12, IO13 | `USB_D-` / `USB_D+`. |
| Console — avoid | IO16 (TXD0), IO17 (RXD0) | |

Recommended: **buttons on IO2 and IO3** (`LP_GPIO2/3`, `ADC1_CH2/3`, no strapping or JTAG
role — IO4–IO7 are the JTAG pins MTMS/MTDI/MTCK/MTDO and are noisier at boot). I2S is free to
sit on IO18–IO23 via the GPIO matrix.

**Nano ESP32 (S3)**: the existing `config.h` pin choices stand. Keep
`-DBOARD_USES_HW_GPIO_NUMBERS` so `pinMode`, the wake mask and the I2S config all speak raw
GPIO.

### 4.2 The toolchain migration is the first task, not an afterthought

Moving to arduino-esp32 3.x is forced by the C6, and it pays for itself:

- The LEDC calls in `door_panel/main.cpp` are already guarded with
  `#if ESP_ARDUINO_VERSION_MAJOR >= 3` — that path finally gets exercised.
- **The "active-HIGH buttons + external pull-downs" constraint may go away.** The README's ⚠️
  block is a consequence of IDF 4.4 having no deep-sleep GPIO wake on the S3, forcing `ext1` in
  `ANY_HIGH`. On IDF 5.x, `esp_deep_sleep_enable_gpio_wakeup()` exists on the S3. If it holds
  up on hardware, the buttons revert to plain active-low with internal pull-ups and the external
  resistors come off both nodes. **Verify this on hardware before rewiring** — it is the single
  highest-value thing to test early.

### 4.3 The link is a requirement; ESP-NOW is one implementation of it

Confirmed: **communication is required, ESP-NOW is not, and an antenna is optional.** That
reframes the biggest risk in the project.

ESP-NOW is a **soft** real-time transport — delivery is best-effort with unbounded retry
latency, so a late `VoiceMsg` becomes a click rather than a failure. That is acceptable for
voice, and it is *why* voice is the only thing crossing the wall. But it means the 13.75 ms
chunk deadline is met statistically, never guaranteed.

| Transport | Meets 136 kbps | Deadline behaviour | Antenna | Notes |
|---|---|---|---|---|
| **Wired UART** @ 921600 | 92 kB/s vs 17 kB/s needed | **deterministic** | no | 3 wires incl. GND. Cannot drop packets. |
| **ESP-NOW** | marginal, needs measuring | soft, unbounded retries | yes | works S3 ↔ C6 on a fixed channel |
| Wired SPI | Mbps | deterministic | no | 4 wires + a master/slave role decision |
| BLE GATT | ~100–300 kbps practical | worse — connection interval quantises to ≥7.5 ms | yes | no advantage here |
| 802.15.4 (Thread/Zigbee) | 250 kbps raw | soft, plus stack overhead | yes | **C6 only — the S3 has no 802.15.4.** Would force a C6 pair. |
| WiFi UDP | ample | soft, router-dependent | yes | needs an AP; the original spec rejected this on purpose |

**The shipped link must be wireless.** Guest and host are separately-installed modules, so a
permanent wire through the wall is not on the table — this closes open decision 6. What remains
true is that a wire is the right *bench* configuration.

**Recommendation: build behind the seam, bring up on the wire, ship on the radio.** The Mega
bench already invented the right abstraction by accident —
[`sim_link.h`](Doorbell_Shared/sim_link.h) exposes exactly two calls:

```c
linkSend(&msg, sizeof msg);            // -> esp_now_send() / Serial1.write()
linkPoll(buf, sizeof buf, onCommand);  // <- recv callback / Serial1.read()
```

That was written as a *simulation hack*. With "antenna optional" it stops being a hack and
becomes **the transport interface**. Promote `sim_link.h` to `link.h` with selectable backends
(`LINK_SERIAL`, `LINK_ESPNOW`), and the same header also carries the S3 ↔ Metro M4 display
channel (§3.1). One framing module, three uses.

Doing voice over a wire first means M4 (voice) is proven with a link that **cannot** drop a
packet. If it then breaks over ESP-NOW, you know with certainty the fault is the radio and not
the audio code. That inverts the current risk order, where the two hardest unknowns — live
audio and a marginal radio — would otherwise be debugged simultaneously.

Because the radio is now mandatory rather than optional, **M5 is a real milestone with a real
chance of failing**, and §9.4's BLE fallback stops being academic. Budget for it.

---

## 5. Milestones v2

| # | Milestone | Board(s) | Done when |
|---|---|---|---|
| **M0** | **Toolchain.** arduino-esp32 2.0.17 → 3.x. Add `[env:indoor_c6]` and a SAMD51 env. Re-verify all 12 diag envs build. Test `esp_deep_sleep_enable_gpio_wakeup()` on the S3. | S3, C6, M4 | 12 diags + firmwares build on 3.x; wake-mode question answered. |
| **M1** | **Door panel core, corrected to the diagrams.** Timer 1 = 30 s (one constant), timer 2 = 3 min. Ding-dong on every valid press. Counter saturates at 3. Explicit "link alive?" sleep gate. | S3 | `diag_logic` shows counter 1,2,3,3,3… with sound on every press. |
| **M2** | **`link.h`.** Promote `sim_link.h` to a real transport interface with `LINK_SERIAL` / `LINK_ESPNOW` backends. | S3 | Same `DoorbellMsg`/`VoiceMsg` bytes move over a wire between two boards. |
| **M3** | **The brain-rot screen on its own MCU.** ILI9341 + microSD on the Metro M4; UART command channel from the S3; count→animation map with GIF#3 on the rage branch. Retire the OLED. | S3 + M4 | 3 presses inside 30 s ⇒ three animations, the third GIF#3, with no effect on S3 timing. |
| **M4** | **Voice over the wire.** INMP441 → `link.h` → MAX98357A, half-duplex, both directions. | S3 + C6 | Intelligible speech both ways over UART. Audio proven independent of the radio. |
| **M5** | **Cut the wire.** Swap the backend to `LINK_ESPNOW`. Session-scoped `init`/`deinit`; re-init after wake. | S3 + C6 | Same speech quality wirelessly, or a measured reason why not. |
| **M6** | **Power.** Deep sleep on both nodes, wake on button. | S3 + C6 | Measured idle current; wake press counts as press #1. |

Two orderings are deliberate. **M3 before the voice work**: the screen is the project's identity,
it is entirely local, and it now lives on a board with no deadlines — it can be finished while
the radio question is still open. **M4 before M5**: prove the audio over a link that cannot drop
a packet, *then* introduce one that can.

### 5.1 The one number to watch

The Mega bench already computes it: one intercom session is **~76 pkt/s × 224 B ≈ 136 kbps
sustained**.

- Over **UART at 921600** that is 17 kB/s against a 92 kB/s ceiling — a 5× margin, deterministic.
  M4 is not at risk.
- Over **ESP-NOW** it is the whole risk. Before attempting M5, edit `diag_espnow_tx` to send a
  220-byte payload every ~14 ms and watch the ack rate hold. If it collapses, drop the sample
  rate or the packet size — do not start debugging audio code that M4 already proved.
- **WiFi 6 on the C6 does not help.** ESP-NOW runs legacy 802.11 b/g rates regardless (§3.2).

---

## 6. What carries over from the Mega bench

Keep it. It is the cheapest place to iterate on logic, and these parts port directly:

- **`protocol.h`** — byte-identical to the ESP build's copy. The wire format is settled.
- **The state machine shape** in `Doorbell_Sender_Side/src/main.cpp` — closer to the diagrams
  than the ESP firmware is, once §2.3 and §2.5 are applied.
- **The audio-chain reasoning** — the 8 kHz sample clock, the ring buffer, the int16→8-bit
  conversion. On the ESP32, `audioPlayChunk()` collapses to one `i2s_write()` of `m.pcm` and
  everything upstream is unchanged.
- **`sim_link.h`** — was written to be *deleted* on real hardware. It should instead be
  **promoted** (§4.3). `linkSend`/`linkPoll` map 1:1 onto `esp_now_send` and the recv callback,
  and equally well onto `Serial1` — which, now that the antenna is optional, makes it the
  project's transport interface rather than a simulation hack. This is the single most valuable
  thing the bench produced.

What it will never prove: the radio, deep sleep, a real microphone, and — now — GIF playback,
which is out of the Mega's SPI budget and has moved to the Metro M4 (§3.1).

---

## 7. Open decisions

### 7.1 Settled

- **Timers.** Two, with deliberately different defaults: timer 1 = 30 s (doorbell window, ≤3
  presses), timer 2 = 3 min (voice session). `INTERCOM_MAX_MS` changes 15 s → 180 s;
  `RAGE_WINDOW_MS`/`IDLE_SLEEP_MS` collapse into one timer-1 constant.
- **Doorbell crosses the wall.** The host has a buzzer, so `MSG_DOORBELL` stays (§2.2).
- **The chime is never muted.** Three presses, three ding-dongs, three GIFs; escalation is
  visual (§2.3, §2.3.1).
- **`RAGE_LIMIT == GIF_COUNT == 3`,** one clamped index, no special rage branch (§2.3.1).
- **Voice is simplex,** guest → host. No talk-back; drop the half-duplex framing (§2.7).
- **Guest sleeps, host does not.** The host has no buttons, so no wake source; it is
  mains-powered and always listening (§2.7).
- **Roles.** Guest = screen, buzzer, 2 buttons, mic. Host = buzzer, speaker. LEDs on both if
  pins allow — and they do, on both sides (§2.7).
- **The link is session-scoped** and re-initialised after every wake (§2.1).
- **The transport is pluggable, but the shipped link is wireless** — the modules install
  separately, so the wire is a bench crutch only (§4.3).

### 7.2 Still open

1. **Does the C6 hit 10 fps decoding a 240×240 GIF?** (§10.3) The only claim in this plan not
   derivable from a datasheet, and the display architecture rests on it. One `diag_gif` env
   answers it in an hour. **Do this before wiring anything.**

2. **Which C6 flash variant?** N4 (4 MB) makes the 1.5 MB GIF budget tight; N8/N16 make it a
   non-issue. Check the module marking.

3. **Is the guest battery-powered at all?** The whole deep-sleep design assumes it is. If the
   door module can be mains-fed, timer-1-to-sleep becomes display blanking rather than a power
   feature, and the C6-versus-S3 sleep advantage stops mattering.

4. **ESP8266: retired or kept?** §3.2 argues for the C6. The ESP8266 stays useful as a spare
   v1 chime, and `diag_espnow_rx_led` remains the fastest way to prove a new radio.

---

## 8. Immediate next steps

1. Answer decisions 1, 2 and 4 — they change M1 and M5.
2. Apply the constant changes now, they are free: `INTERCOM_MAX_MS` → 180000, and collapse
   `RAGE_WINDOW_MS`/`IDLE_SLEEP_MS` into one timer-1 constant. Both files
   ([`config.h`](Anti-rage%20doobell/include/config.h),
   [`config_mega.h`](Doorbell_Shared/config_mega.h)) must stay in step.
3. Migrate to arduino-esp32 3.x and confirm the 12 diags still build (M0).
4. Test S3 deep-sleep GPIO wake on 3.x. If it works, drop the external pull-downs from the
   wiring docs and flip `BTN_ACTIVE_LEVEL` back.
5. Bring up the display MCU with the ILI9341 and get one hard-coded frame sequence on screen,
   driven by a byte over UART. It is the riskiest unproven thing left that a wire cannot rescue.

---

## 9. Alternative allocations — with 2× ESP32-C6

§3.1.1 retracted the only reason a *node* had to be the S3. Once the display is on its own MCU
and one C6 I²S controller can run mic and amp simultaneously, the boards can be re-dealt.

| | Guest node | Host node | Display | Spare |
|---|---|---|---|---|
| **Plan A** (§4) | Nano ESP32 (S3) | C6 | Metro M4 | C6, 8266, Seeeduino |
| **Plan B** | C6 | C6 | Metro M4 | S3, 8266, Seeeduino |
| **Plan C** | C6 | C6 | Nano ESP32 (S3) | Metro M4, 8266, Seeeduino |
| **Plan D** ⭐ | **C6 + panel** | **C6** | *(none)* | S3, Metro M4, 8266, Seeeduino |

> **Plan D is the recommendation**, on the strength of §10: with real compressed `.gif` files the
> storage argument that forced a separate display MCU evaporates, and the C6's 512 KB SRAM holds
> a full framebuffer. Two boards, no microSD, no power-gating problem. Plans A–C survive as
> documented fallbacks if §10.3's decode measurement disappoints.

### 9.1 Same chip, not the same node — corrected

I originally argued Plans B and C make the two nodes **identical**. The settled peripheral list
(§2.7) says otherwise, and two of my five points were wrong:

- ~~Talk-back becomes inherent.~~ **There is no talk-back.** The guest has no speaker and the
  host has no mic; voice is simplex. Nothing to gain.
- ~~Identical sleep code.~~ **Only the guest sleeps.** The host has no buttons, so it has no
  wake source and must stay listening (§2.7, consequence 3).

The nodes differ in hardware *and* firmware: guest = state machine + timers + mic + screen +
deep sleep; host = a sink that rings a buzzer and plays a stream, always on.

What genuinely survives, and is still worth having:

1. **The ESP-NOW interop unknown disappears.** §3.2 flagged S3 ↔ C6 as needing verification —
   different silicon, different WiFi generations, fixed-channel interop. C6 ↔ C6 is the same
   chip at both ends. One fewer thing that can quietly not work, on the project's riskiest path.
2. **One toolchain, one platform, one `platformio.ini` pattern**, one set of chip-specific
   gotchas to learn instead of two.
3. **The guest gets the better sleep story.** LP_GPIO0–7 wake at 7 µA on the C6, with none of
   the S3's `ext1`-versus-`gpio_wakeup` awkwardness (§4.2). The guest is the node that sleeps,
   so this lands exactly where it matters.

That is a weaker case than I first made, but it still points the same way — and §9.2's argument
for the S3 as the display engine is untouched by any of this.

### 9.2 Why Plan C — the S3 is a much better display engine than the M4

If the S3 is not needed on a node, it should go where it is strongest:

| | Metro M4 Express | Nano ESP32 (S3) |
|---|---|---|
| RAM | 192 KB — one 150 KB framebuffer, ~40 KB left | 512 KB + **8 MB PSRAM** |
| Frame storage | 2 MB QSPI ≈ **13** full frames | **16 MB flash ≈ 106** full frames |
| microSD needed? | **yes** | **no** |
| SPI to panel | ~24 MHz | **80 MHz** |
| Cores | 1 | 2 |

Dropping the microSD is worth more than it looks: it removes a card slot, a filesystem, a
card-detect failure mode, and the floating-`SDCS`-corrupts-MISO trap that
[`Doorbell_Shared/README.md`](Doorbell_Shared/README.md) already documents as a real hazard.
The GIF library ships inside the firmware image.

The screen is the project's identity, and Plan C is the only allocation that gives it the best
silicon in the box.

### 9.2.1 The cost of splitting: the guest becomes two boards

Guest and host install as separate modules, so guest-side bulk is a real constraint. Plan C puts
**two MCUs plus a TFT** in the door enclosure, on the side that is battery-powered and expected
to deep-sleep. Two consequences to design for:

- **Power.** The S3 and the TFT backlight will dominate the guest's budget and neither is needed
  between events. The C6 must either power-gate the display board (a MOSFET on its supply) or
  put it in deep sleep and wake it over the UART line. Do not leave it idling at 40 mA next to a
  node whose whole point is 7 µA.
- **Packaging.** Two boards and a panel is a chunky door unit. If that is unacceptable, the
  single-C6 layout in §3.1 is the fallback — and the GIF-storage table there is what decides
  whether it is viable.

### 9.3 The cost of Plan C, stated plainly

**The C6 is single-core at 160 MHz;** the S3 is dual-core at 240 MHz. On the S3 you can pin the
audio task to core 1 and leave WiFi on core 0. On a C6 node the radio stack and the audio task
share one core, so a WiFi burst can delay an audio chunk against the 13.75 ms deadline.

Three things make this acceptable rather than alarming:

- I²S runs on **DMA** — the CPU only refills buffers, it does not clock samples.
- §4.3's wire-first ordering takes the radio out of the audio path entirely for M4.
- If it does bite, the fix is a **swap, not a redesign**: move the door node to the S3 and the
  display back to the M4, i.e. fall back to Plan A. The firmware is portable between C6 and S3
  within arduino-esp32 3.x.

That fallback is why Plan C is safe to try first.

### 9.4 Two radios a C6 pair unlocks — one trap, one real option

**802.15.4 (Thread/Zigbee) — do not use it for voice.** A C6 pair can do it and the S3 cannot,
so it looks like a reason to go symmetric. It is not. IEEE 802.15.4 caps the PHY frame at
**127 bytes**; a `VoiceMsg` is **224**. Every chunk would fragment into two frames, and 136 kbps
against a 250 kbps PHY with CSMA overhead will not hold. Fine for a doorbell notification,
useless for the stream.

**BLE 5.3 — better than I gave it credit for in §4.3.** Both C6s have it. The 2M PHY with data
length extension carries well past 136 kbps, and — the part that matters for a real-time
controller — a **connection interval gives bounded delivery latency**, where ESP-NOW's retries
are unbounded. Heavier stack and slower session setup, so not the default. But it is the right
first fallback if ESP-NOW's ack rate disappoints, ahead of dropping the sample rate.

### 9.5 What changes in the milestones

Only the board column. §5's M0–M6 stand as written, with:

- **M0** adds a second C6 env instead of a SAMD51 env; the S3 env becomes the display target.
- **M3** targets the S3 for the display and gains a task: the GIF library moves into the 16 MB
  flash image, so there is no microSD bring-up at all.
- **M4/M5** are unaffected by the display choice.

---

## 10. Recommended GIF specification

The storage table in §3.1 assumed **raw** frames. That was the wrong unit: a real `.gif` is
8-bit palettised **and LZW-compressed with inter-frame differencing**, which is roughly 20×
smaller than raw for cartoon and meme content. Sizing against raw frames overstates the problem
by more than an order of magnitude, and it was pushing the architecture toward a board it does
not need.

### 10.1 The spec

| Parameter | Recommended | Why |
|---|---|---|
| **Format** | animated `.gif`, native 8-bit palette | LZW + frame differencing for free; authoring tools everywhere |
| **Resolution** | **240 × 240** | Square is the native aspect of meme content; leaves a 240×80 strip on the 240×320 panel for the press count and `WAIT.` |
| **Frame rate** | **10 fps** (GIF delay = 10 cs) | Native GIF cadence; 12 fps is fine, above 15 buys nothing |
| **Duration** | **2–3 s** ⇒ 20–30 frames | Longer than the 460 ms ding-dong, short enough not to straddle the next press |
| **Palette** | ≤256, per-GIF optimised | GIF native; dither photographic sources |
| **File size** | **≤ 500 KB each, ≤ 1.5 MB total** | The budget that makes everything below fit |
| **Loop** | play once per press | Escalation is per-press, not continuous |

### 10.2 Why that fits a single C6 — three checks

- **Storage.** 3 × 500 KB = **1.5 MB** in a LittleFS partition (`pio run -t uploadfs`).
  Comfortable on a C6-N8 (8 MB); workable on an N4 (4 MB) with ~1.5 MB of app.
  **No microSD, no 16 MB flash, no S3.** *(Check which N-variant your modules are.)*
- **RAM.** The C6 has **512 KB SRAM**. The `AnimatedGIF` decoder (bitbank2) needs ~32 KB in
  per-line-callback mode; a *full* 240×240 RGB565 framebuffer is only 112.5 KB if you want
  tear-free double buffering. Either fits with room to spare — **the S3's 8 MB PSRAM buys
  nothing here.**
- **Bandwidth.** 240×240×2 B × 10 fps = **1.15 MB/s**. The C6's GP-SPI runs to 80 MHz ⇒
  5–10 MB/s. A 4–8× margin.

### 10.3 The one number to measure

Everything above is derivable from datasheets except **LZW decode throughput on a 160 MHz
single-core RISC-V**. An ESP32-S3 does 240×240 GIF at 30+ fps; the C6 should manage 15–25, and
the target is 10 — but that is an extrapolation, not a spec.

**Measure it first.** A `diag_gif` env that decodes one bundled GIF to the panel and prints
achieved fps is an hour's work and it de-risks the entire display architecture. If the C6 falls
short, the fallbacks in order are: drop to 8 fps → shrink to 160×160 → move the panel to the S3
(Plan C).

### 10.4 What this collapses

With the panel on the guest C6, the guest is **14 of 16 signals** — it fits, with two spare:

| Signal | Pin | Note |
|---|---|---|
| Button 1, Button 2 | **IO2, IO3** | `LP_GPIO2/3` — deep-sleep wake; no strapping or JTAG role |
| TFT MOSI · SCK · CS · DC · RST · BL | IO18 · IO19 · IO20 · IO21 · IO22 · IO23 | **no MISO** (panel is write-only) and **no SDCS** (no card) |
| INMP441 SCK · WS · SD | IO10 · IO11 · IO4 | I²S RX |
| Buzzer | IO5 | LEDC tone |
| Status LEDs ×2 | IO6, IO7 | the nice-to-have, and it fits |
| *spare* | IO0, IO1 | |

Host node is 6 of 16 (I²S TX ×3, buzzer, 2 LEDs) — trivial.

**So the recommended build is two boards, not three:** guest C6 with the panel attached, host C6,
and the S3 and Metro M4 both return to the spares bin. Plan C's two-MCU guest, its power-gating
problem (§9.2.1) and its packaging bulk all disappear.

### 10.5 One state-machine consequence

A 2–3 s GIF is longer than the gap between angry presses. **Policy: a new press interrupts the
running animation and restarts with the next GIF** — immediate feedback is the whole point, and
queueing would make the screen lag the button. The clamp in §2.3.1 means press 4+ simply
restarts GIF#3, which reads correctly as "still waiting."
