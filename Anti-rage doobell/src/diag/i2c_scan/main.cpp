// DIAG: I2C bus scan (door panel, ESP32-S3)
// Finds the OLED's address before you trust the screen driver.
// Run: pio run -e diag_i2c_scan -t upload -t monitor
// PASS: prints "found 0x3C" (or 0x3D). Nothing found -> check SDA/SCL/power/pull-ups.
#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin();                 // default SDA=GPIO11(A4), SCL=GPIO12(A5)
  Serial.println("\n[i2c] scanner ready");
}

void loop() {
  uint8_t n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf("  found device at 0x%02X\n", a); n++; }
  }
  Serial.printf("[i2c] %u device(s). (SSD1306 OLED is usually 0x3C)\n", n);
  delay(3000);
}
