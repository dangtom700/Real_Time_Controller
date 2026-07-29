#pragma once
#include <stdint.h>
// Shared ESP-NOW wire format, bench edition.
//
// The structs below are BYTE-IDENTICAL to "Anti-rage doobell/include/protocol.h".
// That is the whole point: whatever the Mega bench proves about framing, ordering
// and payload sizes carries over unchanged when the nodes become an ESP32-S3 +
// ESP8266 pair. Do not "improve" these structs here without changing them there.
//
// ESP-NOW caps a payload at 250 bytes; keep messages small.

static const uint8_t PROTO_VERSION = 1;

enum MsgType : uint8_t {
  MSG_DOORBELL    = 1,  // a doorbell press happened
  MSG_VOICE_BEGIN = 2,  // intercom session opening
  MSG_VOICE_CHUNK = 3,  // a chunk of PCM audio
  MSG_VOICE_END   = 4,  // intercom session closing
};

#pragma pack(push, 1)

// Doorbell notification: sender (door panel) -> receiver (indoor chime).
struct DoorbellMsg {
  uint8_t  version;     // = PROTO_VERSION
  uint8_t  type;        // = MSG_DOORBELL
  uint8_t  pressCount;  // 1..RAGE_LIMIT
  uint8_t  locked;      // 1 once the rage-lockout has engaged
  uint32_t seq;         // increments per press since boot
};

// Voice payload for the half-duplex intercom.
// 110 * int16 = 220 bytes, under the 250-byte ESP-NOW cap. 8 kHz / 16-bit mono.
static const uint8_t VOICE_SAMPLES = 110;
struct VoiceMsg {
  uint8_t  version;                 // = PROTO_VERSION
  uint8_t  type;                    // MSG_VOICE_BEGIN / _CHUNK / _END
  uint16_t seq;                     // chunk order (loss/reorder detection)
  int16_t  pcm[VOICE_SAMPLES];      // 16-bit mono samples
};

#pragma pack(pop)

// The audio rate the VoiceMsg math above assumes. Both nodes must agree.
static const uint16_t VOICE_SAMPLE_RATE = 8000;

// ============================================================================
//  Text framing — the "digital simulation" of the radio on a single board.
//
//  With one Mega you cannot run both nodes at once, so the link is bridged by
//  the USB serial line instead: the sender PRINTS each packet it would have
//  transmitted, as
//
//      #0101020007000000
//       ^^ version
//         ^^ type
//           ^^^^^^^^^^^^ the rest of the struct, little-endian, as hex
//
//  ...and the receiver ACCEPTS exactly that line pasted into its monitor. Same
//  bytes, same order, same struct - only the transport is faked. On the ESP32
//  pair these two helpers are deleted and esp_now_send()/the recv callback move
//  the identical buffer.
// ============================================================================

// Longest frame we ever encode is a VoiceMsg (224 B) -> '#' + 448 hex + NUL.
static const uint16_t FRAME_MAX_BYTES = 224;
static const uint16_t FRAME_MAX_CHARS = 1 + (FRAME_MAX_BYTES * 2) + 1;

inline char frameNibble(uint8_t v) {
  return (char)(v < 10 ? ('0' + v) : ('A' + (v - 10)));
}

inline int8_t frameUnnibble(char c) {
  if (c >= '0' && c <= '9') return (int8_t)(c - '0');
  if (c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
  return -1;
}

// Encode `len` bytes as "#<hex>" into `out` (must hold FRAME_MAX_CHARS).
// Returns the string length, or 0 if the packet is too big to frame.
inline uint16_t frameEncode(const void *data, uint8_t len, char *out) {
  if (len > FRAME_MAX_BYTES) return 0;
  const uint8_t *p = (const uint8_t *)data;
  uint16_t n = 0;
  out[n++] = '#';
  for (uint8_t i = 0; i < len; i++) {
    out[n++] = frameNibble((uint8_t)(p[i] >> 4));
    out[n++] = frameNibble((uint8_t)(p[i] & 0x0F));
  }
  out[n] = '\0';
  return n;
}

// Decode a "#<hex>" line back into `out`. Returns the byte count, or 0 on a
// malformed line (bad prefix, odd digit count, non-hex char, or overflow).
inline uint8_t frameDecode(const char *line, uint8_t *out, uint8_t maxLen) {
  if (!line || line[0] != '#') return 0;
  const char *h = line + 1;
  uint8_t n = 0;
  while (h[0] && h[0] != '\r' && h[0] != '\n') {
    const int8_t hi = frameUnnibble(h[0]);
    const int8_t lo = frameUnnibble(h[1]);
    if (hi < 0 || lo < 0) return 0;          // odd length or junk
    if (n >= maxLen) return 0;               // would overflow the caller
    out[n++] = (uint8_t)((hi << 4) | lo);
    h += 2;
  }
  return n;
}
