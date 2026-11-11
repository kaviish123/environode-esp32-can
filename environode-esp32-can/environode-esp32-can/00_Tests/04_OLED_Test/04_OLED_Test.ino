// 04_OLED_Test.ino
// Tests the 0.96" SSD1306 OLED on I2C (address 0x3C).
// Needs libraries: "Adafruit SSD1306" and "Adafruit GFX Library".
// Expected: "OLED OK" and a counter that goes up every second.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
int counter = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found at 0x3C. Run the I2C scanner.");
    while (true) delay(1000);
  }
  Serial.println("OLED ready");
}

void loop() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("OLED OK");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.printf("Counter: %d", counter++);
  display.display();
  delay(1000);
}
