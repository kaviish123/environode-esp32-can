// 05b_SD_Diagnostic.ino
// Finds out WHY the SD card is "not found". Upload it, open the Serial Monitor
// at 115200 baud, press the EN (reset) button once, and send the full output.
//
// Wiring it expects: CS = GPIO5 (D5), SCK = GPIO18 (D18), MISO = GPIO19 (D19), MOSI = GPIO23 (D23)
//
// What it does:
//  Test 1: checks the MISO wire is not stuck to GND or 3.3V
//  Test 2: talks to the card at a very slow speed and asks "are you there?" (CMD0)
//          A working card answers 0x01. No answer (0xFF) or 0x00 points to a wiring or power problem.
//          It also repeats the question with a pull-up on MISO, to tell
//          "MISO wire not connected" apart from "module pulls MISO low".
//  Test 3: tries to mount the card at slow, medium and normal speed

#include <SPI.h>
#include <SD.h>
#include "driver/gpio.h"

#define PIN_CS   5
#define PIN_SCK  18
#define PIN_MISO 19
#define PIN_MOSI 23

// ---------- Test 1: MISO wire check ----------
void testMisoWire() {
  Serial.println("\n--- Test 1: MISO wire (D19) ---");
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);              // card not selected

  pinMode(PIN_MISO, INPUT_PULLUP);
  delay(5);
  int withUp = digitalRead(PIN_MISO);
  pinMode(PIN_MISO, INPUT_PULLDOWN);
  delay(5);
  int withDown = digitalRead(PIN_MISO);
  pinMode(PIN_MISO, INPUT);

  Serial.printf("MISO with pull-up: %s, with pull-down: %s\n",
                withUp ? "HIGH" : "LOW", withDown ? "HIGH" : "LOW");
  if (withUp == LOW && withDown == LOW)
    Serial.println("=> MISO is stuck LOW. It may be touching GND, or plugged in the wrong hole.");
  else if (withUp == HIGH && withDown == HIGH)
    Serial.println("=> MISO is held HIGH by the module. Normal for modules with a buffer chip.");
  else
    Serial.println("=> MISO is free (follows the pull). Normal when the card is not selected.");
}

// ---------- Test 2: send CMD0 and read the answer ----------
uint8_t sendCmd0() {
  SPI.beginTransaction(SPISettings(250000, MSBFIRST, SPI_MODE0));   // 250 kHz, very slow

  digitalWrite(PIN_CS, HIGH);
  for (int i = 0; i < 10; i++) SPI.transfer(0xFF);   // 80 clock pulses to wake the card

  digitalWrite(PIN_CS, LOW);
  SPI.transfer(0xFF);
  const uint8_t cmd0[6] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};    // CMD0 = "go to SPI mode"
  for (int i = 0; i < 6; i++) SPI.transfer(cmd0[i]);

  uint8_t r = 0xFF;
  for (int i = 0; i < 10 && r == 0xFF; i++) r = SPI.transfer(0xFF);   // wait for the answer

  digitalWrite(PIN_CS, HIGH);
  SPI.transfer(0xFF);
  SPI.endTransaction();
  return r;
}

bool testCmd0() {
  Serial.println("\n--- Test 2: ask the card 'are you there?' (CMD0) ---");
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);

  bool ok = false;
  for (int attempt = 1; attempt <= 3; attempt++) {
    uint8_t r = sendCmd0();
    Serial.printf("Attempt %d: card answered 0x%02X\n", attempt, r);
    if (r == 0x01) ok = true;
    delay(50);
  }

  // Same question again, with the ESP32's pull-up switched on for MISO.
  // If MISO is not connected, the pull-up makes the reply 0xFF.
  // If the module is really driving MISO low, it stays 0x00.
  gpio_pullup_en((gpio_num_t)PIN_MISO);
  uint8_t rUp = sendCmd0();
  gpio_pullup_dis((gpio_num_t)PIN_MISO);
  Serial.printf("With pull-up on MISO: card answered 0x%02X\n", rUp);
  if (rUp == 0x01) ok = true;

  if (ok) {
    Serial.println("=> The card answered correctly. Wiring and power are OK.");
  } else {
    Serial.println("=> No correct answer. Meaning of the reply:");
    Serial.println("   0xFF : card is silent. Check module VCC (power), GND, CS (D5), SCK (D18), MOSI (D23),");
    Serial.println("          and that MISO goes to D19. Also check the card is pushed fully in (click).");
    Serial.println("   0x00 : MISO is stuck LOW. Check the MISO wire and that it is not on a GND column.");
    Serial.println("   other: card is half working. Usually weak power: try 5V on the module VCC.");
    if (rUp == 0xFF)
      Serial.println("=> VERDICT: 0x00 turned into 0xFF with the pull-up, so nothing drives MISO. "
                     "Check the MISO wire (module MISO -> hole 10j, next to D19) and module power.");
    else if (rUp == 0x00)
      Serial.println("=> VERDICT: the module itself holds MISO LOW. On this module (AMS1117 + 74LVC125) "
                     "that means VCC is too low: power the module VCC from 5V (VIN).");
  }
  SPI.end();
  return ok;
}

// ---------- Test 3: mount at different speeds ----------
void testMount() {
  Serial.println("\n--- Test 3: mount the card ---");
  const uint32_t speeds[] = {400000, 1000000, 4000000};
  for (uint32_t hz : speeds) {
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    bool ok = SD.begin(PIN_CS, SPI, hz);
    Serial.printf("At %lu kHz: %s\n", (unsigned long)(hz / 1000), ok ? "MOUNTED" : "failed");
    if (ok) {
      uint8_t t = SD.cardType();
      Serial.printf("Card type: %s, size: %llu MB\n",
                    t == CARD_SDHC ? "SDHC" : t == CARD_SD ? "SD" : t == CARD_MMC ? "MMC" : "unknown",
                    SD.cardSize() / (1024ULL * 1024ULL));
      SD.end();
      Serial.println("=> SD card works. If it only mounts at a low speed, use that speed in the project.");
      return;
    }
    SD.end();
    SPI.end();
    delay(100);
  }
  Serial.println("=> Could not mount. If Test 2 said OK, the card is probably not FAT32: format it as FAT32.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===== SD card diagnostic =====");
  testMisoWire();
  bool answered = testCmd0();
  testMount();
  Serial.println("\n===== Done. Copy everything above and send it. =====");
  if (!answered) Serial.println("(Tip: send a photo of the SD module too, front and back.)");
}

void loop() {}
