# Handoff — 2026-07-31

Session state for picking this up cold. `README.md` holds the measured results
and the full wiring; this file holds what is *in flight*, what is unresolved,
and the environment traps that will otherwise be rediscovered the hard way.

## Orientation

Anti-Rage Doorbell, prototype 1. **Three boards**, not two:

| | Board | Role |
|---|---|---|
| A | ESP32-C6-DevKitC-1, MAC `20:6E:F1:17:03:B8` | guest node — door panel. Buttons, mic, sender |
| B | ESP32-C6-DevKitC-1, MAC `20:6E:F1:17:07:40` | host node — indoor chime. MAX98357A amp, receiver |
| C | Arduino Nano ESP32 (ESP32-S3) | OLED + buzzer, deep sleeps |

MAC is also the USB serial number, so `pio device list` identifies boards
without flashing. On this machine A enumerates as COM9 and B as COM8, but that
swaps — check the MAC, never the port number.

`LINK_PEER_MAC_INIT` names **board B** because it must name the receiver.

## Status

| Item | State |
|---|---|
| Step 0 — hello | ✅ both boards |
| Step 1 — UART link | ✅ passed, then **retired** (BTN1 took IO1) |
| Step 2 — voice over UART | ✅ 73 pkt/s, 130 kbps, lost=0/2911, gap-max 13.7 ms |
| Step 3 — voice over ESP-NOW | ✅ lost=0/1558 rx; ack-ok=1018, ack-FAIL=0 |
| step_io — buttons | ✅ working, debounce verified against real bounce |
| step_io — buzzer | ✅ works on board C's D4; never worked on A's IO3 |
| Mic (A, IO6) | wired, **untested** |
| Step 5 — link side | ✅ lost=0, i2s=1393/rx=1405, costs the link nothing |
| Step 5 — audio | ⛔ **PARKED.** Amp receives data but far too quiet to judge. Never heard properly |
| A → C link | ✅ wired (A IO0→C D2, A IO10→C D3); `step_panel_tx`/`_rx` built, **not run** |
| OLED | not started |
| Zigbee fallback | not built |
| `metro_gpio` — Metro M4 bare metal | ✅ builds; image verified; **never flashed**, no board attached |

## Do these first

1. **Run `step_panel`.** Wired and built; never flashed, because only one USB
   cable reaches the bench and it is on board B. Flash A, then C, then leave the
   cable on **C** — C is the one that prints the layered report, and A runs off
   the shared 5 V rail.
   ```
   pio run -e step_panel_tx -t upload --upload-port <A>   # C6, native USB port
   pio run -e step_panel_rx -t upload --upload-port <C>   # Nano, DFU: double-tap RESET
   ```
   PASS: a self-test beep at boot, then one burst every 4 s with the beep count
   walking 1, 2, 3 and holding at 3 with the lower `locked` tone under it.

   The sequencing is the risky part and is what this actually tests: assert
   wake, wait `PANEL_WAKE_MS` (300 ms) for C to boot, *then* send. Get the order
   wrong and it still passes on a bench where C is already awake, then fails in
   the field every time C has gone to sleep.

   **Check the wake line has its 10 kΩ pulldown at C.** It was not mentioned
   when the link was wired. `INPUT_PULLDOWN` in the sketch does not cover the
   reset-to-`pinMode` window, which is the whole reason the resistor is there.

2. **Test the mic** on A's IO6. Sample with `analogContinuous()` (ADC DMA), not
   `analogRead()` — the design rests on a 13.75 ms deadline and `analogRead()`
   jitter lands straight in the `gap-max` that step 2 calibrated.

3. **Audio is parked, not broken — resume it with the two cheap levers.** Read
   README §Step 5 *What the bring-up established* first; it records which
   results from that session are real and which are void. Short version: the
   amp works and is simply too quiet, GAIN is still unconnected at 9 dB, and
   `voice_tx` sends quarter-scale samples. Try **GAIN → GND** before writing any
   code. Do **not** assume 8 kHz is broken — it was never fairly tested.

   Only one USB cable reaches the bench, and it currently sits on board B
   (COM8); board A runs off the shared 5 V rail. Moving it back is what makes
   board A visible again.

4. **`metro_gpio` has never touched hardware** — a sidetrack, not a doorbell
   step; see README §Sidetrack. It builds and the image checks out (vector
   table, every register address, UF2 container), but no Metro M4 has been
   plugged in, so nothing about it is confirmed. Flashing is a file copy:
   `pio run -e metro_gpio`, `python tools/bin2uf2.py
   .pio/build/metro_gpio/firmware.bin`, **double-tap** RESET, drop the `.uf2` on
   `METROM4BOOT`. The button on D7 is the only part that must be wired; D13 is
   soldered down and shows the same state, so the external LED on D8 is
   optional. Read D13 first: slow blink = running and released, solid = pressed
   (PASS), blinks but never solid = the button, dark = never left the bootloader.
   **Not the LED on pin 40** — that is PB22, the NeoPixel, and it cannot be lit
   by a level write.

## Unresolved

- **Is IO3 on board A usable at all?** The buzzer never sounded from it while
  the same element, wiring and code rang three clean tones from board C's D4.
  Whether `ledcAttach` on IO3 reported OK or FAILED was never captured. Treat
  IO3 as suspect before reusing it for anything.
- **Board C's idle current.** An S3 with PSRAM is far hungrier asleep than a
  sleeping C6. If the buzzer and screen live on C, either C stays awake or every
  beep pays ~300 ms of wake latency. A doorbell that beeps late feels broken.
  Not yet measured or decided.
- **Voice bitrate.** Zigbee cannot carry the intercom (see README): 224-byte
  `VoiceMsg` against a 127-byte 802.15.4 frame, 130 kbps needed against 50–100
  kbps realistic. Only halving the audio — 8-bit companded, ~65 kbps — would
  change that. Currently voice is Wi-Fi/ESP-NOW only.
- **`DEBOUNCE_MS` is 40 and was a guess.** Measured bounce is ~1 ms across ~6
  edges; real presses run 124–988 ms. There is room to drop it to 10–15 ms for a
  crisper feel. Collect a few more `bounce: N extra edges` readings first.

## Environment traps

These cost hours already. All are recorded in `platformio.ini` and `README.md`
comments too, but they are the ones that block work outright.

- **`core_dir = C:\pio`.** Not cosmetic. Under the default core dir, an
  ESP-Matter header inside `esp32-arduino-libs` lands at 265 chars and blows
  Windows' 260-char `MAX_PATH`; the install dies mid-unpack with a
  `FileNotFoundError` that reads like a PlatformIO bug.
- **Flash over the port silkscreened `USB`, not `UART`.** The UART port's
  CP2102N enumerates but sits at `ConfigManagerErrorCode 28` — no driver on this
  machine, so no COM port. The native port needs **both**
  `-DARDUINO_USB_CDC_ON_BOOT=1` and `-DARDUINO_USB_MODE=1`; with `MODE=0` the
  core resolves `Serial` to `USBSerial`, which does not exist on a C6.
- **An upload that fails with `UnicodeEncodeError: 'charmap' codec` is hiding
  the real error.** PlatformIO's error printer writes non-ASCII through the
  console's cp1252 codepage and dies mid-message, so `[upload] Error 4294967295`
  is all you get and the actual cause is lost. Set `PYTHONIOENCODING=utf-8` for
  the command and the real message comes back. (An upload can also just hang on
  the first connect — one did here for 13 minutes at 0% CPU, then succeeded on a
  straight retry with nothing else changed.)
- **`*** [upload] Error 2` usually means the monitor is holding the port.**
  Windows serial access is exclusive. Close the monitor, or upload from the IDE
  which releases it.
- **Never let the VSCode PlatformIO extension install while a CLI build runs.**
  Saving `platformio.ini` triggers a re-init; racing a build once stranded the
  2.4 GB toolchain in a `.tmp` dir and left `packages/toolchain-riscv32-esp`
  missing entirely.
- **A monitor opened right after upload misses `setup()`.** On native USB,
  `Serial` is `HWCDCSerial` and discards output with no host attached; the
  device re-enumerates after reset, which outlasts the `delay(400)`. Use
  `tools/readserial.ps1`, which pulses EN and then reads, or press RESET with
  the monitor already open.
- **Board C (`buzzer_nano`) deliberately does not inherit `${env.build_flags}`.**
  Those are C6-specific — `ARDUINO_USB_MODE=1` is right for USB-Serial-JTAG and
  wrong for the Nano's TinyUSB. It also uploads over **DFU**: double-tap RESET,
  and the COM port changes.
- **On board C, `D4` means GPIO7, not GPIO4.** PlatformIO sets
  `BOARD_USES_HW_GPIO_NUMBERS`, which turns off the Arduino pin remap. Use the
  `Dx` symbols, never the literal.

## Counters that look like faults and are not

- A large `LOST n` (step 1) or `reorder=1` (steps 2–3) right after the receiver
  starts: it joined a stream already in progress. What matters is that the
  counter then stops incrementing.
- `gap-max` excursions on UART to ~19 ms: they track the sender's `late-max`
  almost exactly. Both sketches `printf` once per second and over USB CDC that
  print lands inside the measured interval. The instrument is measuring itself.
- `ack-ok` one above `sent`: the `MSG_VOICE_BEGIN` packet is acked but is not a
  chunk.
- `ack-ok` at *exactly* zero with `failed` climbing ~69/s: that is the sender
  flashed onto board B, transmitting to its own MAC. A weak link still gets some
  acks; exactly zero means a self-addressed peer.
