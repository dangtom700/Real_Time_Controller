#pragma once
#include <Arduino.h>
#include "protocol.h"
// The simulated ESP-NOW link for the single-board bench.
//
// WHY THIS EXISTS
//   The Mega 2560 has no radio, and you have one board, so the two nodes cannot
//   actually talk. Instead the link is bridged over the USB serial line: the
//   sender prints every packet it would have transmitted, the receiver reads
//   those same lines back in. The BYTES are real - only the transport is faked.
//
// WHY IT LOOKS LIKE THIS
//   The two calls below are shaped like their ESP-NOW counterparts so that the
//   port to the ESP32 pair touches this file and nothing else:
//
//       linkSend(&msg, sizeof msg)   ->   esp_now_send(BROADCAST, ..., len)
//       linkPoll(buf, sizeof buf)    ->   the esp_now recv callback filling buf
//
//   Every call site in main.cpp stays exactly as written.
//
// SERIAL CONVENTION
//   Lines beginning with '#' are packets. Lines beginning with anything else
//   (we use '[') are human logs and are ignored by the decoder, so you can read
//   the monitor and still copy-paste the traffic.
//
// Define LINK_ENABLE_RX before including this to compile the receive path (it
// costs a ~450 byte line buffer, so the TX-only sender leaves it out).

// ---------------------------------------------------------------- transmit --
// Frame a packet and emit it. Returns false only if the packet is too large to
// frame, which mirrors esp_now_send() rejecting an oversized payload.
inline bool linkSend(const void *data, uint8_t len) {
  char frame[FRAME_MAX_CHARS];
  if (frameEncode(data, len, frame) == 0) return false;
  Serial.println(frame);
  return true;
}

#ifdef LINK_ENABLE_RX
// A line that is not a '#' packet is handed to this, if the sketch supplies one.
// That is what lets you drive the bench by typing single-letter commands into
// the same monitor you paste frames into. It has no ESP-NOW counterpart and
// simply disappears on the real hardware.
typedef void (*LinkCommandFn)(const char *line);

// ----------------------------------------------------------------- receive --
// Non-blocking. Accumulates one serial line per call-batch; when a complete
// '#' frame arrives it is decoded into `out` and its length returned. Returns 0
// while the line is still in flight, and on malformed or non-packet lines.
inline uint8_t linkPoll(uint8_t *out, uint8_t maxLen, LinkCommandFn onCommand = 0) {
  static char     line[FRAME_MAX_CHARS];
  static uint16_t n = 0;

  while (Serial.available()) {
    const char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (n == 0) continue;               // swallow blank lines / CRLF pairs
      line[n] = '\0';
      n = 0;
      if (line[0] == '#') {
        const uint8_t got = frameDecode(line, out, maxLen);
        if (got) return got;              // a real packet: hand it up
        continue;                         // malformed frame: drop it
      }
      if (onCommand) onCommand(line);     // a typed command / stray log line
      continue;
    }

    if (n < FRAME_MAX_CHARS - 1) {
      line[n++] = c;
    } else {
      n = 0;                              // overlong line: drop it, resync
    }
  }
  return 0;
}
#endif  // LINK_ENABLE_RX
