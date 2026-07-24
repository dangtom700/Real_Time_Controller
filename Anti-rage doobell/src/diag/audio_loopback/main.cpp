// DIAG: local audio loopback (door panel, ESP32-S3)
// Mic (I2S_NUM_0 RX) -> Amp (I2S_NUM_1 TX) on the SAME board. This proves the whole
// audio chain BEFORE you add the ESP-NOW radio. If this works but wireless doesn't,
// the problem is the link, not the audio.
// Run: pio run -e diag_audio_loopback -t upload -t monitor
// PASS: speaker echoes what the mic hears (watch for feedback - keep them apart).
#include <Arduino.h>
#include <driver/i2s.h>
#include "config.h"

#define SAMPLE_RATE 16000

void setup() {
  Serial.begin(115200);
  delay(300);

  // ---- MIC: I2S_NUM_0, RX, 32-bit ----
  i2s_config_t rx = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4, .dma_buf_len = 256,
    .use_apll = false, .tx_desc_auto_clear = false, .fixed_mclk = 0
  };
  i2s_pin_config_t rxp = {
    .mck_io_num = I2S_PIN_NO_CHANGE, .bck_io_num = MIC_SCK_GPIO,
    .ws_io_num = MIC_WS_GPIO, .data_out_num = I2S_PIN_NO_CHANGE, .data_in_num = MIC_SD_GPIO
  };
  i2s_driver_install(I2S_NUM_0, &rx, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &rxp);

  // ---- AMP: I2S_NUM_1, TX, 16-bit ----
  i2s_config_t tx = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8, .dma_buf_len = 256,
    .use_apll = false, .tx_desc_auto_clear = true, .fixed_mclk = 0
  };
  i2s_pin_config_t txp = {
    .mck_io_num = I2S_PIN_NO_CHANGE, .bck_io_num = AMP_BCLK_GPIO,
    .ws_io_num = AMP_LRC_GPIO, .data_out_num = AMP_DIN_GPIO, .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_1, &tx, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &txp);

  Serial.println("[loopback] mic -> amp passthrough. Speak into the INMP441.");
}

void loop() {
  int32_t in[256];
  size_t br = 0;
  i2s_read(I2S_NUM_0, in, sizeof(in), &br, portMAX_DELAY);
  int n = br / sizeof(int32_t);
  int16_t out[256];
  for (int i = 0; i < n; i++) out[i] = (int16_t)(in[i] >> 14);   // 32-bit -> 16-bit with gain
  size_t bw = 0;
  i2s_write(I2S_NUM_1, out, n * sizeof(int16_t), &bw, portMAX_DELAY);
}
