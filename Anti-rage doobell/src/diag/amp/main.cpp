// DIAG: MAX98357A amplifier (door panel, ESP32-S3, I2S_NUM_1 TX)
// Plays a 440 Hz sine test tone.
// Run: pio run -e diag_amp -t upload -t monitor
// PASS: a steady beep from the speaker. Silence -> SD pin pulled to GND (=shutdown),
//       speaker '-' grounded (it's bridge-tied), or DIN/BCLK/LRC swapped.
#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include "config.h"

#define SAMPLE_RATE 16000

void setup() {
  Serial.begin(115200);
  delay(300);
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = AMP_BCLK_GPIO,
    .ws_io_num = AMP_LRC_GPIO,
    .data_out_num = AMP_DIN_GPIO,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_1, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pins);
  Serial.println("[amp] playing 440 Hz tone on MAX98357A");
}

void loop() {
  const int N = 256;
  int16_t buf[N];
  static float phase = 0;
  for (int i = 0; i < N; i++) {
    buf[i] = (int16_t)(sinf(phase) * 8000);
    phase += 2.0f * PI * 440.0f / SAMPLE_RATE;
    if (phase > 2.0f * PI) phase -= 2.0f * PI;
  }
  size_t bytesWritten;
  i2s_write(I2S_NUM_1, buf, sizeof(buf), &bytesWritten, portMAX_DELAY);
}
