// 02c_Power_Check.ino
// Uses the ESP32 itself as a simple voltmeter (no multimeter needed).
// It measures the voltage on pin D4 (GPIO4) every half second.
//
// How to use:
//   Put one jumper wire into hole 5a (the free hole right above the D4 pin).
//   Touch / push its other end into the place you want to check:
//     red + rail            -> should show about 3.3 V
//     OLED VDD column (20b) -> should show about 3.3 V
//     blue - rail           -> should show 0.0 V
//     hole 4a (D2, driven HIGH by this sketch) -> 3.3 V: proves the probe itself works
//   NEVER connect it to VIN or 5V. Only 3.3 V or less is safe on this pin.

#define PROBE_PIN 4
#define REF_PIN   2     // D2 is driven HIGH = a known 3.3 V test point (hole 4a). Its blue LED lights up.

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(REF_PIN, OUTPUT);
  digitalWrite(REF_PIN, HIGH);
  analogSetPinAttenuation(PROBE_PIN, ADC_11db);   // measure up to about 3.3 V
  Serial.println("\n===== Power check (probe on D4, hole 5a) =====");
}

void loop() {
  uint32_t mv = analogReadMilliVolts(PROBE_PIN);
  float v = mv / 1000.0;
  Serial.printf("Probe: %.2f V   ", v);
  if (v > 2.9)      Serial.println("=> 3.3 V is here (power OK)");
  else if (v < 0.2) Serial.println("=> 0 V (this is GND, or NO power here)");
  else              Serial.println("=> in between: probe not touching well, or weak power");
  delay(500);
}
