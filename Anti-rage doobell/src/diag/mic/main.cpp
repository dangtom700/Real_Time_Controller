// DIAG: INMP441 microphone (door panel, ESP32-S3, I2S_NUM_0 RX)
// Prints a serial VU meter from the mic's RMS level.
// Run: pio run -e diag_mic -t upload -t monitor
// PASS: the '#' bar grows when you talk/tap the mic; near-empty in silence.
//       Flat/zero -> check SD/WS/SCK wiring and L/R tied to GND.
#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include "config.h"

#define SAMPLE_RATE 16000

void setup() {
  Serial.begin(115200);
  delay(300);
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = MIC_SCK_GPIO,
    .ws_io_num = MIC_WS_GPIO,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_SD_GPIO
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  Serial.println("[mic] INMP441 VU meter - speak into the mic");
}

void loop() {
  int32_t buf[256];
  size_t bytesRead = 0;
  i2s_read(I2S_NUM_0, buf, sizeof(buf), &bytesRead, portMAX_DELAY);
  int n = bytesRead / sizeof(int32_t);
  double sumsq = 0;
  for (int i = 0; i < n; i++) { int32_t s = buf[i] >> 16; sumsq += (double)s * s; }
  int rms = n ? (int)sqrt(sumsq / n) : 0;
  int bars = map(constrain(rms, 0, 1200), 0, 1200, 0, 40);
  Serial.printf("[mic] rms=%5d |", rms);
  for (int i = 0; i < bars; i++) Serial.print('#');
  Serial.println();
}
