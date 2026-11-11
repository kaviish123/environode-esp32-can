// 01_Blink.ino
// First test: checks the board, the USB cable and the Arduino IDE setup.
// Expected: the small blue LED on the ESP32 blinks, and the Serial Monitor
// (115200 baud) prints "Hello from ESP32" once a second.

#define LED_PIN 2   // onboard LED on most ESP32 DevKit V1 boards

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Blink test started");
}

void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.println("Hello from ESP32");
  delay(500);
  digitalWrite(LED_PIN, LOW);
  delay(500);
}
