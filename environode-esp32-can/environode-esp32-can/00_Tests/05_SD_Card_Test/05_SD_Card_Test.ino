// 05_SD_Card_Test.ino
// Tests the micro SD module on SPI: CS = GPIO5, SCK = 18, MISO = 19, MOSI = 23.
// The card must be formatted as FAT32.
// Expected: card size printed, a line written to /test.txt, then read back.

#include <SPI.h>
#include <SD.h>

#define SD_CS 5

void setup() {
  Serial.begin(115200);
  delay(500);

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card not found. Check wiring, 5V/3.3V power and FAT32 format.");
    return;
  }
  Serial.printf("Card size: %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));

  File f = SD.open("/test.txt", FILE_APPEND);
  if (f) {
    f.printf("Test line written at %lu ms\n", millis());
    f.close();
    Serial.println("Wrote a line to /test.txt");
  } else {
    Serial.println("Could not open /test.txt for writing");
    return;
  }

  f = SD.open("/test.txt");
  Serial.println("Contents of /test.txt:");
  while (f && f.available()) Serial.write(f.read());
  if (f) f.close();
}

void loop() {}
