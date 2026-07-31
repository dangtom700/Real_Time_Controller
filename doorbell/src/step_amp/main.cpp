// STEP 5a — the amp on its own.  Board B only, no radio, no second board.
//
//   pio run -e step_amp -t upload --upload-port COMy
//
// WHY THIS EXISTS SEPARATELY FROM voice_rx_chime
//   The end-to-end test has two things in series that had never run: the
//   ESP-NOW audio path and the MAX98357A.  If the speaker is silent, that test
//   cannot say which one failed — and the buzzer already cost this project a
//   day of exactly that question (README: was it the element or the C6?).
//
// WHAT THE BRING-UP ACTUALLY FOUND — read this before changing anything here.
//   The first run produced hiss, and hiss was misread as a broken I2S config.
//   Four sketches went looking for it: sample rate swept 8/16/32/48 kHz, slot
//   mode left then both, BCLK/LRC tried both ways round.  None of it changed
//   the sound, because none of it was ever wrong.
//
//   What finally moved the needle was AMPLITUDE.  A full-scale square where
//   there had been a quarter-scale sine was audible where the sine was not.
//   The amp had been working the whole time; a 1/4-scale sine at the 9 dB that
//   an unconnected GAIN pin selects simply sits under this speaker's noise
//   floor and is indistinguishable from hiss by ear.
//
//   The lesson worth keeping: "silent" and "too quiet to pick out" are the same
//   observation through a speaker, and they have completely different causes.
//   Establish that the path carries ANYTHING — loudest possible signal against
//   zeros, enable pin held steady so it cannot confound — before touching a
//   single I2S parameter.
//
// SETTINGS, AND WHY THEY ARE THESE
//   48 kHz    BCLK 1.536 MHz, inside the 1.5-5 MHz the part is quoted at.
//             8 kHz would clock BCLK at 256 kHz, far below it.  The wire format
//             stays 8 kHz; voice_rx_chime upsamples rather than reclocking.
//   STEREO    the same sample in both slots.  Mono picks one slot, and ESP-IDF's
//             choice of which is not worth depending on when duplicating costs
//             one loop and cannot be wrong.
//   kAmp      near full scale, NOT voice_tx's 1/4-scale 8000.  That headroom
//             was the whole bug.
//
// PASS: 1 s of clean, clearly audible 440 Hz A4, then 1 s of true silence,
//       repeating.  The gap must be SILENT, not merely quieter: a Class D amp
//       still hissing through it has SD stuck high.
//
// IF IT IS AUDIBLE BUT WEAK: tie GAIN to GND for 15 dB instead of the 9 dB an
// unconnected GAIN pin selects.  One wire, +6 dB, and it is what the reference
// design this bench was checked against does.

#include <Arduino.h>
#include <math.h>
#include <ESP_I2S.h>
#include "config_v1.h"
#include "protocol.h"

static I2SClass i2s;

static const float    kTwoPi  = 6.28318530718f;
static const float    kToneHz = 440.0f;          // voice_tx's A4
static const int16_t  kAmp    = 28000;           // ~85% FS — see above
static const uint8_t  kBlock  = VOICE_SAMPLES;   // 110 samples per write
static const uint32_t kRate   = 48000;
static const uint32_t kOnMs   = 1000, kOffMs = 1000;

static float phase = 0.0f;
static const float kStep = kTwoPi * kToneHz / (float)kRate;

// Mono sine, duplicated into both slots on the way out.
static void writeTone(uint32_t ms) {
  int16_t stereo[kBlock * 2];
  const uint32_t start = millis();
  // i2s.write() blocks once the DMA fills, and that is what paces this — no
  // delay() is involved in the timing.
  while (millis() - start < ms) {
    for (uint8_t i = 0; i < kBlock; i++) {
      const int16_t s = (int16_t)(sinf(phase) * (float)kAmp);
      phase += kStep;
      if (phase >= kTwoPi) phase -= kTwoPi;
      stereo[i * 2] = stereo[i * 2 + 1] = s;
    }
    i2s.write((const void *)stereo, sizeof stereo);
  }
}

static void writeSilence(uint32_t ms) {
  int16_t stereo[kBlock * 2] = {};
  const uint32_t start = millis();
  while (millis() - start < ms) i2s.write((const void *)stereo, sizeof stereo);
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[step-amp] MAX98357A bench test - no link involved");

  // SD low first.  The 10k pulldown holds the amp off from reset until this
  // line runs; driving it low explicitly keeps it off across i2s.begin(),
  // which is when the signal pins first move.  That is what kills the pop.
  pinMode(AMP_SD_GPIO, OUTPUT);
  digitalWrite(AMP_SD_GPIO, LOW);

  i2s.setPins(AMP_BCLK_GPIO, AMP_LRC_GPIO, AMP_DIN_GPIO);
  if (!i2s.begin(I2S_MODE_STD, kRate,
                 I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.printf("[step-amp] i2s.begin() FAILED (err %d) - stopping\n", i2s.lastError());
    while (true) delay(1000);
  }

  Serial.printf("[step-amp] BCLK=IO%d  LRC=IO%d  DIN=IO%d  SD=IO%d\n",
                AMP_BCLK_GPIO, AMP_LRC_GPIO, AMP_DIN_GPIO, AMP_SD_GPIO);
  Serial.printf("[step-amp] %lu Hz 16-bit stereo -> BCLK %lu kHz, tone %.0f Hz at %d\n",
                (unsigned long)kRate, (unsigned long)(kRate * 2 * 16 / 1000),
                kToneHz, kAmp);
  Serial.println("[step-amp] expect 1s TONE / 1s SILENCE, repeating");
}

void loop() {
  // Queue audio before unmuting so the amp comes up on real samples rather
  // than on whatever the DMA still held.
  writeTone(40);
  digitalWrite(AMP_SD_GPIO, HIGH);
  writeTone(kOnMs);

  // Mute first, then keep feeding zeros: muting alone would leave the tail
  // sitting in the DMA to play out at the start of the next cycle.
  digitalWrite(AMP_SD_GPIO, LOW);
  writeSilence(kOffMs);

  Serial.println("[step-amp] cycle");
}
