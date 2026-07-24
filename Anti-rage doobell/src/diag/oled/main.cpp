// DIAG: OLED SSD1306 (door panel, ESP32-S3)
// Run: pio run -e diag_oled -t upload -t monitor
// PASS: screen shows "OLED OK" + a sweeping progress bar. Blank -> wrong addr (try 0x3D) or wiring.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(128, 64, &Wire, -1);

void setup() {
  Serial.begin(115200);
  delay(300);
  Wire.begin();
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[oled] init FAILED - try 0x3D or run diag_i2c_scan");
    return;
  }
  Serial.println("[oled] init OK");
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2); oled.setCursor(0, 0);  oled.println("OLED OK");
  oled.setTextSize(1); oled.setCursor(0, 20); oled.println("SSD1306 128x64");
  oled.drawRect(0, 34, 128, 14, SSD1306_WHITE);
  oled.display();
}

void loop() {
  static uint8_t x = 0;
  oled.fillRect(1, 35, 126, 12, SSD1306_BLACK);
  oled.fillRect(1, 35, x, 12, SSD1306_WHITE);
  oled.display();
  x = (x + 4) % 126;
  delay(60);
}
