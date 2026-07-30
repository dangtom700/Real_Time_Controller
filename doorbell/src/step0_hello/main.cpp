// STEP 0 — "is this thing on?"
//
// Wiring:  one USB-C cable, into the port silkscreened UART.  Nothing else.
// Run:     pio run -e step0_hello -t upload -t monitor --upload-port COMx
// PASS:    the banner below, a MAC address, and a heartbeat every second.
//
// This proves four things at once, and it is worth doing on BOTH boards before
// you wire anything: the pioarduino toolchain resolved, the C6 takes a flash,
// Serial comes out of the port you plugged into, and the board survives a
// reset.  Every later step assumes all four.
//
// It also prints the STA MAC, which you need for step 3 — write both down.

#include <Arduino.h>
#include <WiFi.h>
#include "config_v1.h"
#include "protocol.h"   // VOICE_SAMPLES / _SAMPLE_RATE / _CHUNK_US for the line below

static uint32_t beats = 0;

static const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "power-on";
    case ESP_RST_SW:       return "software";
    case ESP_RST_PANIC:    return "PANIC (crash)";
    case ESP_RST_INT_WDT:  return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_DEEPSLEEP:return "deep-sleep wake";
    case ESP_RST_BROWNOUT: return "BROWNOUT (bad power)";
    default:               return "other";
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);                      // let the host reopen the port after reset

  Serial.println();
  Serial.println("=====================================================");
  Serial.println(" Anti-Rage Doorbell  -  prototype 1  -  STEP 0");
  Serial.println("=====================================================");
  Serial.printf("chip        : %s rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("cores       : %d @ %lu MHz\n", ESP.getChipCores(), (unsigned long)getCpuFrequencyMhz());
  Serial.printf("flash       : %lu MB\n", (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));
  Serial.printf("free heap   : %lu bytes\n", (unsigned long)ESP.getFreeHeap());
  Serial.printf("arduino core: %d.%d.%d\n", ESP_ARDUINO_VERSION_MAJOR,
                ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  Serial.printf("reset reason: %s\n", resetReasonName(esp_reset_reason()));

  // The MAC only exists once the WiFi stack has been started.
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.printf(">>> STA MAC : %s\n", WiFi.macAddress().c_str());
  Serial.println(">>> Write this down. The RECEIVER's MAC goes into");
  Serial.println(">>> include/config_v1.h as LINK_PEER_MAC_INIT before step 3.");
  Serial.println();

  // PLAN §7.2 decision 2: the module's flash variant was an open question.
  // It stopped being load-bearing once the art became static images, but a
  // mismatch here still bites at upload time, so say it out loud.
  if (ESP.getFlashChipSize() < 8UL * 1024 * 1024) {
    Serial.println("[note] under 8 MB of flash. platformio.ini's board assumes 8 MB.");
    Serial.println("[note] if uploads fail, add:  board_upload.flash_size = 4MB");
  }

  Serial.printf("[step0] chunk deadline is %lu us (%u samples @ %u Hz)\n",
                (unsigned long)VOICE_CHUNK_US, VOICE_SAMPLES, VOICE_SAMPLE_RATE);
  Serial.println("[step0] heartbeat starting - if this stops, the board reset.");
}

void loop() {
  delay(1000);
  beats++;
#if defined(RGB_BUILTIN)
  // Visible confirmation without a serial monitor. If your board has no RGB
  // LED, or this will not compile, delete this block - nothing depends on it.
  rgbLedWrite(RGB_BUILTIN, (beats & 1) ? 8 : 0, 0, (beats & 1) ? 0 : 8);
#endif
  Serial.printf("[step0] alive %lus  heap=%lu\n",
                (unsigned long)beats, (unsigned long)ESP.getFreeHeap());
}
