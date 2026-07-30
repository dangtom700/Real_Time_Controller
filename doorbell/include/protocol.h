#pragma once
#include <stdint.h>
// The wire format.  BYTE-IDENTICAL to:
//     ../Anti-rage doobell/include/protocol.h
//     ../Doorbell_Shared/protocol.h
//
// That is deliberate and it is the most valuable thing the Mega bench produced
// (PLAN_v2.md §6).  Whatever this prototype proves about framing, ordering and
// packet sizes carries straight over to the other two trees, and vice versa.
// Do not "improve" these structs here without changing them there.
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

// Doorbell notification: guest (door panel) -> host (indoor chime).
struct DoorbellMsg {
  uint8_t  version;     // = PROTO_VERSION
  uint8_t  type;        // = MSG_DOORBELL
  uint8_t  pressCount;  // 1..RAGE_LIMIT
  uint8_t  locked;      // 1 once the rage-lockout has engaged
  uint32_t seq;         // increments per press since boot
};

// Voice payload.  110 * int16 = 220 B of PCM, 224 B on the wire, under the
// 250-byte ESP-NOW cap.  8 kHz / 16-bit mono.
static const uint8_t VOICE_SAMPLES = 110;
struct VoiceMsg {
  uint8_t  version;                 // = PROTO_VERSION
  uint8_t  type;                    // MSG_VOICE_BEGIN / _CHUNK / _END
  uint16_t seq;                     // chunk order (loss/reorder detection)
  int16_t  pcm[VOICE_SAMPLES];      // 16-bit mono samples
};

#pragma pack(pop)

// The audio rate the VoiceMsg math assumes.  Both nodes must agree.
static const uint16_t VOICE_SAMPLE_RATE = 8000;

// ---------------------------------------------------------------------------
//  The numbers that fall out of the two constants above.  These are the whole
//  reason step 2 exists, so they are written down rather than left implied:
//
//    chunk period   110 / 8000 Hz          = 13.75 ms   <- the deadline
//    chunk rate     8000 / 110             = 72.7 packets/s
//    payload rate   72.7 * 224 B           = 16.3 kB/s = 130 kbps
//
//  PLAN_v2.md §5.1 quotes "~76 pkt/s ... 136 kbps".  That rounded the chunk
//  rate up; the exact figures are the ones above.  The conclusion is unchanged
//  (UART at 921600 = 92 kB/s, a 5.6x margin) but measure against 72.7, not 76,
//  or step 2 will look like it is losing 4% of its packets when it is not.
// ---------------------------------------------------------------------------
static const uint32_t VOICE_CHUNK_US = 1000000UL * VOICE_SAMPLES / VOICE_SAMPLE_RATE;  // 13750
