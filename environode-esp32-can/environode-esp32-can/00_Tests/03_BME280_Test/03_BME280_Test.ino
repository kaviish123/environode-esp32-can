// 03_BME280_Test.ino
// Tests your own BME280 driver (BME280Driver.cpp) on its own.
// Expected: temperature, humidity and pressure printed every 2 seconds.
// Room values are usually around 25 to 35 C, 40 to 80 %RH, 900 to 1015 hPa.
// Breathe on the sensor: humidity should jump up.
// Works with a BMP280 too: it has no humidity sensor, so humidity shows "n/a".

#include <Wire.h>
#include "BME280Driver.h"

BME280Driver bme;

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);
  if (!bme.begin(Wire)) {
    Serial.println("BME280 not found. Run the I2C scanner and check wiring.");
    while (true) delay(1000);
  }
}

void loop() {
  float t, h, p;
  if (bme.read(t, h, p)) {
    if (isnan(h)) Serial.printf("Temp: %.2f C   Humidity: n/a   Pressure: %.2f hPa\n", t, p);
    else          Serial.printf("Temp: %.2f C   Humidity: %.2f %%   Pressure: %.2f hPa\n", t, h, p);
  } else {
    Serial.println("Read failed");
  }
  delay(2000);
}
