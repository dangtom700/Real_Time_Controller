#pragma once
#include <Arduino.h>
#include <string.h>
#include "protocol.h"
#include "config_v1.h"

// ===========================================================================
//  THE TRANSPORT SEAM — PLAN_v2.md §4.3.
//
//  `Doorbell_Shared/sim_link.h` was written as a simulation hack: the Mega had
//  no radio, so packets were printed as hex and pasted back in.  The plan's
//  argument is that it accidentally invented the right abstraction, because
//  these three calls sit equally well on a UART and on ESP-NOW:
//
//      linkBegin()
//      linkSend(&msg, sizeof msg)
//      linkPoll(buf, sizeof buf)
//
//  So it is promoted here from hack to interface.  Pick a backend at build
//  time; NOTHING above this line changes when you switch:
//
//      -DLINK_SERIAL   UART1, 2 jumpers.  Deterministic, cannot drop a packet.
//      -DLINK_ESPNOW   the radio.  Soft real-time, unbounded retries.
//
//  That is the whole point of §4.3's ordering — bring the audio up on a link
//  that CANNOT lose data, then swap one -D and find out what the radio costs.
//  If it breaks after the swap, the fault is the radio.  You do not have to
//  wonder whether it was your audio code.
// ===========================================================================

#if defined(LINK_SERIAL) == defined(LINK_ESPNOW)
#error "Define exactly one of -DLINK_SERIAL or -DLINK_ESPNOW (see platformio.ini)"
#endif

// The largest thing we ever put on the wire.  sizeof(VoiceMsg) == 224.
static const uint8_t LINK_MAX_PAYLOAD = 224;

// Health counters.  Declared for both backends so the sketches can print them
// unconditionally; [[maybe_unused]] because each build only ever touches half.
[[maybe_unused]] static uint32_t linkErrCrc     = 0;  // SERIAL: frame arrived corrupt
[[maybe_unused]] static uint32_t linkErrOverrun = 0;  // ESPNOW: RX ring full, loop() too slow
[[maybe_unused]] static uint32_t linkAckOk      = 0;  // ESPNOW: peer acked
[[maybe_unused]] static uint32_t linkAckFail    = 0;  // ESPNOW: no ack — THE number §5.1 wants

// CRC-16/CCITT-FALSE.  Cheap, and it catches the failure the UART actually
// has: a dropped byte resyncing mid-frame and handing up plausible garbage.
static inline uint16_t linkCrc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xFFFF;
  while (n--) {
    c ^= (uint16_t)(*p++) << 8;
    for (uint8_t i = 0; i < 8; i++)
      c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
  }
  return c;
}

// ===========================================================================
#ifdef LINK_SERIAL
// ---------------------------------------------------------------------------
//  UART1 backend.  Framing: A5 5A | len | payload | crc16-le
//
//  A raw struct dump would be unrecoverable — one lost byte and every packet
//  after it is misaligned forever.  The sync word lets the receiver resync,
//  the length bounds the copy, and the CRC means a resync that lands mid-frame
//  is rejected instead of being handed up as audio.
// ---------------------------------------------------------------------------
static const uint8_t LINK_SYNC0 = 0xA5;
static const uint8_t LINK_SYNC1 = 0x5A;

inline bool linkBegin() {
  // 230 wire-bytes per chunk at 72.7 chunks/s.  The default 256-byte RX buffer
  // is smaller than a single frame, so a receiver that blinks will lose one.
  Serial1.setRxBufferSize(4096);
  Serial1.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_GPIO, LINK_TX_GPIO);
  return true;
}

inline bool linkSend(const void *data, uint8_t len) {
  if (len == 0 || len > LINK_MAX_PAYLOAD) return false;
  const uint16_t c = linkCrc16((const uint8_t *)data, len);
  const uint8_t head[3] = { LINK_SYNC0, LINK_SYNC1, len };
  const uint8_t tail[2] = { (uint8_t)(c & 0xFF), (uint8_t)(c >> 8) };
  Serial1.write(head, sizeof head);
  Serial1.write((const uint8_t *)data, len);
  Serial1.write(tail, sizeof tail);
  return true;
}

// Non-blocking.  Returns the payload length once a whole valid frame is in,
// 0 otherwise.  Call it often — it consumes at most what the UART has buffered.
inline uint8_t linkPoll(uint8_t *out, uint8_t maxLen) {
  enum RxState : uint8_t { WANT_S0, WANT_S1, WANT_LEN, WANT_BODY, WANT_CRC0, WANT_CRC1 };
  static RxState st  = WANT_S0;
  static uint8_t buf[LINK_MAX_PAYLOAD];
  static uint8_t need = 0, got = 0;
  static uint16_t crcRx = 0;

  while (Serial1.available()) {
    const uint8_t b = (uint8_t)Serial1.read();
    switch (st) {
      case WANT_S0:
        if (b == LINK_SYNC0) st = WANT_S1;
        break;
      case WANT_S1:
        // A5 A5 5A is a legal prefix — do not throw away the second A5.
        if (b == LINK_SYNC1)      st = WANT_LEN;
        else if (b == LINK_SYNC0) st = WANT_S1;
        else                      st = WANT_S0;
        break;
      case WANT_LEN:
        if (b == 0 || b > LINK_MAX_PAYLOAD) { st = WANT_S0; break; }
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
        if (linkCrc16(buf, need) != crcRx) { linkErrCrc++; break; }
        if (need > maxLen) break;              // caller's buffer too small
        memcpy(out, buf, need);
        return need;
    }
  }
  return 0;
}
#endif  // LINK_SERIAL

// ===========================================================================
#ifdef LINK_ESPNOW
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

static const uint8_t LINK_PEER_MAC[6] = LINK_PEER_MAC_INIT;

// The callback runs in the WiFi task, not loop().  It must not block, so it
// copies into a ring and gets out.  8 slots is ~110 ms of audio at 72.7 pkt/s —
// enough to ride out a slow loop() without pretending we can buffer forever.
static const uint8_t LINK_RING = 8;
static uint8_t  linkRing[LINK_RING][LINK_MAX_PAYLOAD];
static uint8_t  linkRingLen[LINK_RING];
static volatile uint8_t linkHead = 0, linkTail = 0;

inline void linkOnRecv(const esp_now_recv_info_t *, const uint8_t *data, int len) {
  if (len <= 0 || len > LINK_MAX_PAYLOAD) return;
  const uint8_t next = (uint8_t)((linkHead + 1) % LINK_RING);
  if (next == linkTail) { linkErrOverrun++; return; }   // full: drop newest
  memcpy(linkRing[linkHead], data, (size_t)len);
  linkRingLen[linkHead] = (uint8_t)len;
  linkHead = next;
}

// ESP-IDF changed this callback's first argument in 5.4 (const uint8_t *mac ->
// const wifi_tx_info_t *).  pioarduino 55.03.311 is IDF 5.5.5, so the new form
// is what compiles — but the guard keeps this file building if you pin an
// older release, and tells you where to look when it does not.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
inline void linkOnSend(const wifi_tx_info_t *, esp_now_send_status_t status) {
#else
inline void linkOnSend(const uint8_t *, esp_now_send_status_t status) {
#endif
  if (status == ESP_NOW_SEND_SUCCESS) linkAckOk++; else linkAckFail++;
}

inline bool linkBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  // Power save parks the radio between beacons and adds tens of ms to RX.
  // Fine for a doorbell, fatal for a 13.75 ms chunk deadline.
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_recv_cb(linkOnRecv);
  esp_now_register_send_cb(linkOnSend);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, LINK_PEER_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

// True while LINK_PEER_MAC_INIT is still the all-zero placeholder.
inline bool linkPeerUnset() {
  for (uint8_t i = 0; i < 6; i++) if (LINK_PEER_MAC[i]) return false;
  return true;
}

inline bool linkSend(const void *data, uint8_t len) {
  if (len == 0 || len > LINK_MAX_PAYLOAD) return false;
  // Returns ESP_ERR_ESPNOW_NO_MEM when the driver queue backs up. That is a
  // real signal, not noise: it means we are offering faster than the radio
  // drains, which is precisely the §5.1 failure mode.
  return esp_now_send(LINK_PEER_MAC, (const uint8_t *)data, len) == ESP_OK;
}

inline uint8_t linkPoll(uint8_t *out, uint8_t maxLen) {
  if (linkHead == linkTail) return 0;
  const uint8_t n = linkRingLen[linkTail];
  if (n > maxLen) { linkTail = (uint8_t)((linkTail + 1) % LINK_RING); return 0; }
  memcpy(out, linkRing[linkTail], n);
  linkTail = (uint8_t)((linkTail + 1) % LINK_RING);
  return n;
}
#endif  // LINK_ESPNOW
