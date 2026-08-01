#pragma once
#include <Arduino.h>
#include <string.h>
#include "protocol.h"
#include "config_v1.h"

// ===========================================================================
//  THE PANEL LINK — board A (guest panel) to board C (OLED + buzzer).
//
//  A THIRD transport, deliberately not routed through link.h.  That header
//  picks between UART1 and ESP-NOW at compile time and refuses to build unless
//  exactly one is chosen, because it answers one question: how does A talk to
//  B.  This link is not part of that question.  It runs on different pins, at a
//  different baud, in one direction only, and it must coexist with whichever
//  A<->B backend is selected — so it cannot be a third case inside that switch.
//
//  Simplex: A talks, C listens, C never replies.  Nothing C knows needs to
//  travel back, and one direction costs one wire instead of two.
//
//  Framing is the same as link.h's UART backend, and for the same reason:
//        A5 5A | len | payload | crc16-le
//  A raw struct dump is unrecoverable, because one lost byte misaligns every
//  message after it forever.  The sync word lets the receiver resync, the
//  length bounds the copy, and the CRC means a resync landing mid-frame is
//  rejected rather than handed up as a doorbell press.
//
//  The CRC below is byte-for-byte link.h's linkCrc16 and is duplicated rather
//  than shared, which is worth one sentence of justification: including link.h
//  to borrow it would force this file to also pick an A<->B backend it has no
//  business having an opinion about.  Twelve lines is cheaper than that
//  coupling.  If one changes, change both.
// ===========================================================================

static const uint8_t PANEL_SYNC0 = 0xA5;
static const uint8_t PANEL_SYNC1 = 0x5A;
static const uint8_t PANEL_MAX_PAYLOAD = 32;   // DoorbellMsg is 8

[[maybe_unused]] static uint32_t panelErrCrc = 0;   // frame arrived corrupt
[[maybe_unused]] static uint32_t panelRawBytes = 0; // anything at all on the wire

// CRC-16/CCITT-FALSE.
static inline uint16_t panelCrc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xFFFF;
  while (n--) {
    c ^= (uint16_t)(*p++) << 8;
    for (uint8_t i = 0; i < 8; i++)
      c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}

// ---- sender side (board A) ------------------------------------------------
// TX only: passing -1 for RX leaves that pad alone rather than quietly claiming
// a pin this board has other plans for.
inline void panelBeginTx() {
  pinMode(PANEL_WAKE_GPIO, OUTPUT);
  digitalWrite(PANEL_WAKE_GPIO, LOW);
  Serial1.begin(PANEL_BAUD, SERIAL_8N1, -1, PANEL_TX_GPIO);
}

inline bool panelSend(const void *data, uint8_t len) {
  if (len == 0 || len > PANEL_MAX_PAYLOAD) return false;
  const uint16_t c = panelCrc16((const uint8_t *)data, len);
  const uint8_t head[3] = { PANEL_SYNC0, PANEL_SYNC1, len };
  const uint8_t tail[2] = { (uint8_t)(c & 0xFF), (uint8_t)(c >> 8) };
  Serial1.write(head, sizeof head);
  Serial1.write((const uint8_t *)data, len);
  Serial1.write(tail, sizeof tail);
  Serial1.flush();          // do not drop the wake line on a half-sent frame
  return true;
}

// Assert wake, give C time to boot, send, then release.  THE ORDER IS THE WHOLE
// POINT (PLAN §4.4): a byte arriving on RX cannot wake an ESP32-S3 from deep
// sleep at all, and even from light sleep the first characters are eaten waking
// the UART.  Send first and the message is simply lost — but only when C
// happens to be asleep, which a bench with C already awake never reproduces.
inline bool panelSendWaking(const void *data, uint8_t len) {
  digitalWrite(PANEL_WAKE_GPIO, HIGH);
  delay(PANEL_WAKE_MS);
  const bool ok = panelSend(data, len);
  digitalWrite(PANEL_WAKE_GPIO, LOW);
  return ok;
}

// ---- receiver side (board C) ----------------------------------------------
#if defined(ARDUINO_NANO_ESP32)
inline void panelBeginRx() {
  // INPUT_PULLDOWN backs up the external 10k; it cannot replace it, because it
  // only takes effect once this line runs and the resistor's job is the window
  // between reset and here.
  pinMode(PANEL_C_WAKE_PIN, INPUT_PULLDOWN);
  Serial1.begin(PANEL_BAUD, SERIAL_8N1, PANEL_C_RX_PIN, -1);
}

// Non-blocking.  Returns payload length once a whole valid frame is in, else 0.
inline uint8_t panelPoll(uint8_t *out, uint8_t maxLen) {
  enum RxState : uint8_t { WANT_S0, WANT_S1, WANT_LEN, WANT_BODY, WANT_CRC0, WANT_CRC1 };
  static RxState st = WANT_S0;
  static uint8_t buf[PANEL_MAX_PAYLOAD];
  static uint8_t need = 0, got = 0;
  static uint16_t crcRx = 0;

  while (Serial1.available()) {
    const uint8_t b = (uint8_t)Serial1.read();
    panelRawBytes++;
    switch (st) {
      case WANT_S0:
        if (b == PANEL_SYNC0) st = WANT_S1;
        break;
      case WANT_S1:
        // A5 A5 5A is a legal prefix — do not throw away the second A5.
        if (b == PANEL_SYNC1)      st = WANT_LEN;
        else if (b == PANEL_SYNC0) st = WANT_S1;
        else                       st = WANT_S0;
        break;
      case WANT_LEN:
        if (b == 0 || b > PANEL_MAX_PAYLOAD) { st = WANT_S0; break; }
        need = b; got = 0; st = WANT_BODY;
        break;
      case WANT_BODY:
        buf[got++] = b;
        if (got == need) st = WANT_CRC0;
        break;
      case WANT_CRC0:
        crcRx = b; st = WANT_CRC1;
        break;
      case WANT_CRC1:
        crcRx |= (uint16_t)b << 8;
        st = WANT_S0;
        if (panelCrc16(buf, need) != crcRx) { panelErrCrc++; break; }
        if (need > maxLen) break;
        memcpy(out, buf, need);
        return need;
    }
  }
  return 0;
}
#endif  // ARDUINO_NANO_ESP32
