#pragma once
#include <stdint.h>
// Shared ESP-NOW wire format between the door panel (ESP32-S3) and the indoor
// chime (ESP8266). This header is the contract — keep both nodes building against
// the same copy. ESP-NOW payloads are capped at 250 bytes; keep messages small.

static const uint8_t PROTO_VERSION = 1;

enum MsgType : uint8_t {
  MSG_DOORBELL    = 1,  // a doorbell press happened
  MSG_VOICE_BEGIN = 2,  // intercom session opening              (milestone 3)
  MSG_VOICE_CHUNK = 3,  // a chunk of PCM audio                  (milestone 3)
  MSG_VOICE_END   = 4,  // intercom session closing              (milestone 3)
};

#pragma pack(push, 1)

// Doorbell notification: door panel -> indoor chime.
struct DoorbellMsg {
  uint8_t  version;     // = PROTO_VERSION
  uint8_t  type;        // = MSG_DOORBELL
  uint8_t  pressCount;  // 1..RAGE_LIMIT
  uint8_t  locked;      // 1 once the rage-lockout has engaged
  uint32_t seq;         // increments per press since boot
};

// Voice payload for the half-duplex intercom (milestone 3).
// 110 * int16 = 220 bytes, under the 250-byte ESP-NOW cap. 8 kHz / 16-bit mono.
static const uint8_t VOICE_SAMPLES = 110;
struct VoiceMsg {
  uint8_t  version;                 // = PROTO_VERSION
  uint8_t  type;                    // MSG_VOICE_BEGIN / _CHUNK / _END
  uint16_t seq;                     // chunk order (loss/reorder detection)
  int16_t  pcm[VOICE_SAMPLES];      // 16-bit mono samples
};

#pragma pack(pop)
