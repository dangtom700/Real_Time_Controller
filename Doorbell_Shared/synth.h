#pragma once
#include <Arduino.h>
#include <avr/pgmspace.h>
// A 64-entry quarter-volume-safe sine table plus a phase accumulator.
//
// There is no microphone on this bench, so "voice" is synthesised: the sender
// packs a real sine wave into VoiceMsg.pcm[] at the true 8 kHz rate, and the
// receiver renders one through the LM386 to prove the speaker chain. That makes
// the packet sizes, the sample rate and the audio path all genuinely exercised;
// only the acoustic source is fake. Swap in i2s_read() on the ESP32 and the rest
// of the pipeline is already proven.

static const uint8_t SINE64[64] PROGMEM = {
  128, 140, 153, 165, 177, 188, 199, 209, 218, 226, 234, 240, 245, 250, 253, 254,
  255, 254, 253, 250, 245, 240, 234, 226, 218, 209, 199, 188, 177, 165, 153, 140,
  128, 116, 103,  91,  79,  68,  57,  47,  38,  30,  22,  16,  11,   6,   3,   2,
    1,   2,   3,   6,  11,  16,  22,  30,  38,  47,  57,  68,  79,  91, 103, 116
};

// Phase step for `freq` Hz at `rate` Hz, in 16.16-ish fixed point: the table is
// indexed by the top 6 bits of a uint16_t phase, so a full turn is 65536.
inline uint16_t synthPhaseInc(uint16_t freq, uint16_t rate) {
  return (uint16_t)(((uint32_t)freq * 65536UL) / rate);
}

// Next unsigned 8-bit sample (128 = silence), advancing `phase`.
inline uint8_t synthNext8(uint16_t &phase, uint16_t inc) {
  const uint8_t s = pgm_read_byte(&SINE64[(phase >> 10) & 0x3F]);
  phase += inc;
  return s;
}

// Same, as the signed 16-bit sample the VoiceMsg wire format carries.
inline int16_t synthNext16(uint16_t &phase, uint16_t inc) {
  return (int16_t)(((int16_t)synthNext8(phase, inc) - 128) << 8);
}
