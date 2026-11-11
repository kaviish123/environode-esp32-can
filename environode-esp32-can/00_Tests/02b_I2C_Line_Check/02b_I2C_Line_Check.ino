// 02b_I2C_Line_Check.ino
// Finds out WHY the I2C scanner finds nothing. No extra parts needed.
// SDA = GPIO21 (D21), SCL = GPIO22 (D22).
//
// The OLED and BMP280 boards each have small pull-up resistors from SDA/SCL to their VCC.
// So when the modules are powered and connected, both lines sit HIGH by themselves.
// This sketch checks that, line by line, and tells you what is wrong.
// Open the Serial Monitor at 115200. It repeats every 3 seconds.

#define SDA_PIN 21
#define SCL_PIN 22

int readWith(int pin, int mode) {
  pinMode(pin, mode);
  delay(5);
  return digitalRead(pin);
}

void checkLine(const char *name, int pin) {
  int up   = readWith(pin, INPUT_PULLUP);
  int down = readWith(pin, INPUT_PULLDOWN);
  pinMode(pin, INPUT);

  Serial.printf("%s (D%d): with pull-up %s, with pull-down %s  =>  ",
                name, pin, up ? "HIGH" : "LOW", down ? "HIGH" : "LOW");
  if (up && down)
    Serial.println("GOOD: module pull-ups found, modules are powered and connected.");
  else if (up && !down)
    Serial.println("FLOATING: nothing is connected to this pin. Wire from the ESP32 pin "
                   "to the junction column is missing, in the wrong hole, or the modules are not on it.");
  else
    Serial.println("HELD LOW: the line is pulled to GND. Usually a module has GND but NO VCC "
                   "(check the red + rail wires), or this line touches GND.");
}

void checkShort() {
  pinMode(SCL_PIN, INPUT_PULLUP);
  pinMode(SDA_PIN, OUTPUT);
  digitalWrite(SDA_PIN, LOW);
  delay(5);
  int scl = digitalRead(SCL_PIN);
  pinMode(SDA_PIN, INPUT);
  if (scl == LOW)
    Serial.println("SHORT: SDA and SCL are connected together. Check columns 20 and 24 "
                   "(a wire may be in the wrong column).");
  else
    Serial.println("No short between SDA and SCL.");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n===== I2C line check =====");
}

void loop() {
  Serial.println("--------------------------------");
  checkLine("SDA", SDA_PIN);
  checkLine("SCL", SCL_PIN);
  checkShort();
  delay(3000);
}
