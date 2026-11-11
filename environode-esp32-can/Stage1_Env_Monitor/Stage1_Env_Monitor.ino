// ESP32_Env_Monitor.ino
// Stage 1: single-ESP32 environment monitor and data logger.
//
//  - BME280 (I2C, own register-level driver): temperature, humidity, pressure
//  - SSD1306 OLED (I2C): live readings and status
//  - Micro SD (SPI): CSV log, keeps working when Wi-Fi is down
//  - Wi-Fi: uploads to ThingSpeak
//  - UART (USB Serial, 115200): debug messages
//  - FreeRTOS: separate tasks for sensor, display, logger and cloud upload
//
// Libraries to install (Arduino IDE > Tools > Manage Libraries):
//   "Adafruit SSD1306" and "Adafruit GFX Library"
// Board: Tools > Board > esp32 > "ESP32 Dev Module"

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "BME280Driver.h"

// ---------- Shared data ----------
struct Reading {
  float tempC = NAN;
  float humidity = NAN;
  float pressureHpa = NAN;
  bool  valid = false;
  uint32_t takenAtMs = 0;
};

Reading latest;                       // most recent sensor values
SemaphoreHandle_t dataMutex;          // protects "latest"
SemaphoreHandle_t i2cMutex;           // sensor and OLED share the I2C bus

volatile bool sensorOk = false;
volatile bool sdOk = false;
volatile bool alertOn = false;
volatile uint32_t uploadsOk = 0;
volatile uint32_t uploadsFailed = 0;
volatile uint32_t linesLogged = 0;

BME280Driver bme;
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
bool oledOk = false;

// ---------- Helpers ----------

Reading getLatest() {
  Reading copy;
  if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    copy = latest;
    xSemaphoreGive(dataMutex);
  }
  return copy;
}

// Returns "2026-10-05 14:30:00" once the clock is synced from the internet,
// otherwise "uptime_123s" so the log still has an order.
String timestamp() {
  struct tm t;
  if (getLocalTime(&t, 10)) {
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return String(buf);
  }
  return "uptime_" + String(millis() / 1000) + "s";
}

// ---------- Task 1: read the sensor ----------
void sensorTask(void *) {
  for (;;) {
    float t, h, p;
    bool ok = false;

    if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      ok = bme.read(t, h, p);
      xSemaphoreGive(i2cMutex);
    }

    if (ok) {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      latest.tempC = t;
      latest.humidity = h;
      latest.pressureHpa = p;
      latest.valid = true;
      latest.takenAtMs = millis();
      xSemaphoreGive(dataMutex);
      alertOn = (t > TEMP_ALERT_C);
      Serial.printf("[SENSOR] %.2f C  %.2f %%RH  %.2f hPa\n", t, h, p);
    } else {
      Serial.println("[SENSOR] Read failed");
    }
    sensorOk = ok;
    vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
  }
}

// ---------- Task 2: OLED screen and status outputs ----------
void displayTask(void *) {
  bool beep = false;
  for (;;) {
    Reading r = getLatest();
    bool wifiUp = (WiFi.status() == WL_CONNECTED);

    if (oledOk && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("ENV MONITOR");
      display.setCursor(80, 0);
      display.print(wifiUp ? "WiFi OK" : "OFFLINE");

      if (r.valid && sensorOk) {
        display.setTextSize(2);
        display.setCursor(0, 14);
        display.printf("%.1f C", r.tempC);
        display.setTextSize(1);
        display.setCursor(0, 34);
        if (isnan(r.humidity)) display.print("Hum:   n/a (BMP280)");
        else display.printf("Hum:   %.1f %%", r.humidity);
        display.setCursor(0, 44);
        display.printf("Press: %.1f hPa", r.pressureHpa);
      } else {
        display.setCursor(0, 20);
        display.print("SENSOR ERROR");
        display.setCursor(0, 32);
        display.print("Check BME280 wiring");
      }

      display.setCursor(0, 56);
      display.print(sdOk ? "SD:OK " : "SD:ERR ");
      display.printf("Log:%lu", (unsigned long)linesLogged);
      if (alertOn) { display.setCursor(104, 56); display.print("HOT"); }
      display.display();
      xSemaphoreGive(i2cMutex);
    }

    // Red LED = something is wrong or too hot. Buzzer beeps while too hot.
    digitalWrite(PIN_LED_RED, (!sensorOk || !sdOk || alertOn) ? HIGH : LOW);
    beep = alertOn ? !beep : false;
    digitalWrite(PIN_BUZZER, beep ? HIGH : LOW);

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_PERIOD_MS));
  }
}

// ---------- Task 3: SD card logger ----------
// Writes every LOG_PERIOD_MS whether or not Wi-Fi is working,
// so no data is lost when the internet goes down.
void loggerTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(LOG_PERIOD_MS));
    Reading r = getLatest();
    if (!r.valid) continue;

    if (!sdOk) {                      // try to recover if the card was removed and put back
      SD.end();
      sdOk = SD.begin(PIN_SD_CS);
      if (!sdOk) { Serial.println("[SD] Card not available"); continue; }
    }

    File f = SD.open(LOG_FILE, FILE_APPEND);
    if (!f) {
      Serial.println("[SD] Could not open log file");
      sdOk = false;
      continue;
    }
    String hum = isnan(r.humidity) ? String("") : String(r.humidity, 2);   // empty cell on a BMP280
    f.printf("%s,%.2f,%s,%.2f,%s\n", timestamp().c_str(), r.tempC, hum.c_str(), r.pressureHpa,
             WiFi.status() == WL_CONNECTED ? "online" : "offline");
    f.close();
    linesLogged++;
    Serial.printf("[SD] Logged line %lu\n", (unsigned long)linesLogged);
  }
}

// ---------- Task 4: Wi-Fi and ThingSpeak upload ----------
void cloudTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(UPLOAD_PERIOD_MS));

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WIFI] Offline, trying to reconnect. Data is still saved to SD.");
      WiFi.reconnect();
      uploadsFailed++;
      continue;
    }

    Reading r = getLatest();
    if (!r.valid) continue;

    String url = "http://api.thingspeak.com/update?api_key=" + String(THINGSPEAK_WRITE_KEY) +
                 "&field1=" + String(r.tempC, 2) +
                 "&field3=" + String(r.pressureHpa, 2);
    if (!isnan(r.humidity)) url += "&field2=" + String(r.humidity, 2);   // skipped on a BMP280

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(url);
    int code = http.GET();
    String body = (code > 0) ? http.getString() : "";
    http.end();

    // ThingSpeak replies with the new entry number, or "0" if it rejected the update.
    if (code == 200 && body.toInt() > 0) {
      uploadsOk++;
      Serial.printf("[CLOUD] Uploaded, entry %s\n", body.c_str());
      digitalWrite(PIN_LED_GREEN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(200));
      digitalWrite(PIN_LED_GREEN, LOW);
    } else {
      uploadsFailed++;
      Serial.printf("[CLOUD] Upload failed, HTTP %d, reply '%s'\n", code, body.c_str());
    }
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 Environment Monitor (Stage 1) ===");

  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  dataMutex = xSemaphoreCreateMutex();
  i2cMutex = xSemaphoreCreateMutex();

  // I2C bus shared by sensor and screen
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);

  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Starting...");
    display.display();
  } else {
    Serial.println("[OLED] Not found at 0x3C. Check wiring.");
  }

  sensorOk = bme.begin(Wire);

  // SD card on the default SPI pins (SCK 18, MISO 19, MOSI 23) with CS on GPIO5
  sdOk = SD.begin(PIN_SD_CS);
  if (sdOk) {
    if (!SD.exists(LOG_FILE)) {
      File f = SD.open(LOG_FILE, FILE_WRITE);
      if (f) { f.println("timestamp,temperature_c,humidity_pct,pressure_hpa,wifi"); f.close(); }
    }
    Serial.println("[SD] Card ready");
  } else {
    Serial.println("[SD] Card not found. Check wiring and that the card is FAT32.");
  }

  // Wi-Fi: start connecting, but do not wait forever. The other tasks run either way.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting");
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " not yet, will keep trying");

  configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);   // real clock for the log timestamps

  // FreeRTOS tasks: (function, name, stack size, parameter, priority, handle, CPU core)
  xTaskCreatePinnedToCore(sensorTask,  "sensor",  4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(displayTask, "display", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(loggerTask,  "logger",  6144, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(cloudTask,   "cloud",   8192, NULL, 1, NULL, 0);
}

void loop() {
  // Everything runs in the FreeRTOS tasks above.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
