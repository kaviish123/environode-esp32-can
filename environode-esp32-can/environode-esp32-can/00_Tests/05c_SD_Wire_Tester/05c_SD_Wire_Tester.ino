// 05c_SD_Wire_Tester.ino
// Checks each SD wire one by one, using the ESP32 itself (no multimeter needed).
//
// How it works: every pin below has its internal pull-up switched on, so it reads HIGH.
// When you connect the far end of a wire to GND (the blue - rail), that pin reads LOW.
// If the pin goes LOW, the wire is good AND it is in the right hole.
//
// Pins watched:
//   D5  (GPIO5)  = CS wire
//   D18 (GPIO18) = SCK wire
//   D19 (GPIO19) = MISO wire
//   D23 (GPIO23) = MOSI wire
//   D4  (GPIO4)  = spare test pin, used to check the VCC and GND wires (hole 5a)
//
// Open the Serial Monitor at 115200. It prints a line every time something changes.

const int pins[]       = {5, 18, 19, 23, 4};
const char *names[]    = {"CS   (D5)", "SCK  (D18)", "MISO (D19)", "MOSI (D23)", "TEST (D4)"};
const int N = 5;
int last[N];

void printAll() {
  Serial.println("--------------------------------");
  for (int i = 0; i < N; i++) {
    int v = digitalRead(pins[i]);
    Serial.printf("%s : %s\n", names[i], v == LOW ? "LOW   <== connected to GND" : "HIGH");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===== SD wire tester =====");
  Serial.println("All pins should show HIGH now.");
  Serial.println("Touch the far end of ONE wire to the blue - rail: its pin must change to LOW.");
  for (int i = 0; i < N; i++) {
    pinMode(pins[i], INPUT_PULLUP);
    delay(2);
    last[i] = digitalRead(pins[i]);
  }
  printAll();
}

void loop() {
  bool changed = false;
  for (int i = 0; i < N; i++) {
    int v = digitalRead(pins[i]);
    if (v != last[i]) { last[i] = v; changed = true; }
  }
  if (changed) printAll();
  delay(100);
}
