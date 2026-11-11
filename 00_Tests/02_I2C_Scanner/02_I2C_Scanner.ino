// 02_I2C_Scanner.ino
// Finds every device on the I2C bus (SDA = GPIO21, SCL = GPIO22).
// Expected with BME280 and OLED connected:
//   Found device at 0x3C   <- OLED
//   Found device at 0x76   <- BME280 (some boards use 0x77)
// If nothing is found, check VCC, GND, and that SDA/SCL are not swapped.

#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);
  Serial.println("I2C scanner");
}

void loop() {
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Found device at 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) Serial.println("No I2C devices found");
  Serial.println("---- scanning again in 3 s ----");
  delay(3000);
}
