# Bring-Up & Test Plan

Test **one component at a time**, bottom-up: prove each part alone, then prove pairs, then
integrate. Each test below is its own PlatformIO env under [`src/diag/`](src/diag/) — flashing one
never touches the others.

```
pio run -e diag_<name> -t upload -t monitor
```

The ESP8266 platform is installed; all envs build as-is.

## The ladder (run top to bottom)

| # | Env | Board | Proves | PASS looks like |
|--:|-----|-------|--------|-----------------|
| 1 | `diag_button` | Nano ESP32 | button input + active-HIGH + debounce | `[BTN1] PRESS` on press, `release` on release; idle is silent |
| 2 | `diag_i2c_scan` | Nano ESP32 | the OLED is alive on the bus | prints `found device at 0x3C` |
| 3 | `diag_oled` | Nano ESP32 | **screen** | shows `OLED OK` + sweeping bar |
| 4 | `diag_buzzer` | Nano ESP32 | **buzzer** | ding-dong then a rising sweep, looping |
| 5 | `diag_sleep_wake` | Nano ESP32 | deep sleep + ext1 wake | sleeps; a press reboots with `wake-cause=… EXT1` and boot# climbs |
| 6 | `diag_logic` | Nano ESP32 | anti-rage counting/lockout/window | type `p`×3 in 30 s → `locked=YES sound=MUTED`; idle 30 s → resets |
| 7 | `diag_espnow_tx` | Nano ESP32 | **ESP-NOW send** + MAC/channel + reliability | prints MAC, `sent=N acked=N (100%)` |
| 8 | `diag_espnow_rx_led` | ESP8266 | **ESP-NOW → LED** (your link test) | LED blinks + serial prints each packet as #7 sends |
| 9 | `diag_chime_buzzer` | ESP8266 | chime buzzer | ding-dong loop on the indoor node |
| 10 | `diag_mic` | Nano ESP32 | **voice capture** (INMP441) | serial VU `#` bar grows when you talk |
| 11 | `diag_amp` | Nano ESP32 | **audio out** (MAX98357A) | steady 440 Hz beep from the speaker |
| 12 | `diag_audio_loopback` | Nano ESP32 | the whole audio chain, **no radio** | speaker echoes the mic (keep them apart to avoid feedback) |

Steps 7+8 are run **together** on two boards — that's the point of the pair.

## Then integrate in stages (don't jump to the full firmware)

1. **Input → output pairs** on the door panel: button → buzzer, button → screen. (This is what the
   real [`door_panel`](src/door_panel/main.cpp) firmware already does — flash it and re-run bring-up
   steps 3–6 from [WIRING.md](WIRING.md).)
2. **Doorbell over the wall:** door panel `door_panel` + chime `indoor_chime` — a press beeps the chime.
3. **Local audio** (step 12) working *before* you attempt audio over the radio.
4. **Audio over ESP-NOW** (milestone 3) — the last and hardest step.

## What you were missing (now covered)

Your original list was screen / buzzer / voice / audio / ESP-NOW-LED. The ladder adds the parts
that bite if skipped: **button input** (#1), **I²C address discovery** (#2), **deep-sleep wake**
(#5, the S3's most error-prone feature), **state logic with no hardware** (#6), **send reliability
not just on/off** (#7), and **local audio loopback before the radio** (#12).

## The one risk with no script yet — audio over ESP-NOW

Live voice is the hard 20%. Before wiring real audio end-to-end, sanity-check the *link's* capacity:
8 kHz × 16-bit mono ≈ 128 kbps ≈ ~72 packets/s at the 220-byte `VoiceMsg` size. You can approximate
this by editing `diag_espnow_tx` to send a 220-byte payload every ~14 ms and watching the `acked %`
hold up. If the ack rate collapses, drop the sample rate or packet size before blaming the audio code.

## Verified status
All **12 diagnostics compile clean** — 10 ESP32 (arduino-esp32 2.0.17) and 2 ESP8266
(espressif8266 4.2.1) — as do both production firmwares (`door_panel`, `indoor_chime`).
