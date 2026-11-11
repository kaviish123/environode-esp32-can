// Stage2_Gateway_Node.ino  (upload to ESP32 #2)
// EnviroNode gateway: receives readings from the sensor node over CAN, shows them on the
// OLED, logs them to the SD card, uploads them to ThingSpeak, and raises an alert if the
// sensor node goes silent.
//
// FreeRTOS tasks:
//   canRxTask     receives ENV_DATA and HEARTBEAT frames
//   displayTask   OLED, LEDs and buzzer
//   loggerTask    SD card CSV log (keeps working without Wi-Fi)
//   cloudTask     ThingSpeak upload and Wi-Fi reconnect
//   canHealthTask CAN error monitoring and bus-off recovery
//
// Libraries: "Adafruit SSD1306" and "Adafruit GFX Library"
// Board: Tools > Board > esp32 > "ESP32 Dev Module"

#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/twai.h"
#include "config.h"
#include "can_protocol.h"

// ---------- Shared state ----------
EnvData latest = {NAN, NAN, NAN, 0, 0};
bool haveData = false;
uint32_t lastDataMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t nodeUptimeS = 0;
uint8_t nodeStatus = 0;
SemaphoreHandle_t dataMutex;
SemaphoreHandle_t sdMutex;            // logger task and CAN task both write to the SD card

volatile uint32_t framesReceived = 0;
volatile uint32_t framesLost = 0;       // gaps in the sequence number
volatile uint32_t linesLogged = 0;
volatile bool sdOk = false;
volatile bool nodeOnline = false;
volatile bool wasOnline = false;

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
bool oledOk = false;

// ---------- Helpers ----------
struct Snapshot {
  EnvData d;
  bool haveData;
  bool online;
  uint8_t status;
  uint32_t uptime;
};

Snapshot snapshot() {
  Snapshot s;
  xSemaphoreTake(dataMutex, portMAX_DELAY);
  s.d = latest;
  s.haveData = haveData;
  s.status = nodeStatus;
  s.uptime = nodeUptimeS;
  xSemaphoreGive(dataMutex);
  s.online = nodeOnline;
  return s;
}

String timestamp() {
  struct tm t;
  if (getLocalTime(&t, 10)) {
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return String(buf);
  }
  return "uptime_" + String(millis() / 1000) + "s";
}

void logLine(const String &line) {
  if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(500)) != pdTRUE) return;
  if (!sdOk) {
    SD.end();
    sdOk = SD.begin(PIN_SD_CS);
  }
  if (sdOk) {
    File f = SD.open(LOG_FILE, FILE_APPEND);
    if (f) {
      f.println(line);
      f.close();
      linesLogged++;
    } else {
      sdOk = false;
    }
  }
  xSemaphoreGive(sdMutex);
}

bool canBegin() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)PIN_CAN_TX, (gpio_num_t)PIN_CAN_RX, TWAI_MODE_NORMAL);
  g.rx_queue_len = 20;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
  return twai_start() == ESP_OK;
}

// ---------- Task 1: receive CAN frames ----------
void canRxTask(void *) {
  bool firstFrame = true;
  uint8_t expectedSeq = 0;
  for (;;) {
    twai_message_t msg;
    if (twai_receive(&msg, pdMS_TO_TICKS(200)) == ESP_OK) {
      framesReceived++;

      if (msg.identifier == CAN_ID_ENV_DATA && msg.data_length_code == 8) {
        EnvData d = decodeEnv(msg.data);
        if (!firstFrame && d.sequence != expectedSeq) {
          framesLost += (uint8_t)(d.sequence - expectedSeq);
        }
        firstFrame = false;
        expectedSeq = d.sequence + 1;

        xSemaphoreTake(dataMutex, portMAX_DELAY);
        latest = d;
        haveData = true;
        lastDataMs = millis();
        xSemaphoreGive(dataMutex);
        Serial.printf("[CAN RX] ENV #%u  %.2f C  %.2f %%  %.1f hPa\n", d.sequence, d.tempC, d.humidity, d.pressureHpa);

      } else if (msg.identifier == CAN_ID_HEARTBEAT && msg.data_length_code >= 5) {
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        lastHeartbeatMs = millis();
        nodeStatus = msg.data[0];
        nodeUptimeS = get32(&msg.data[1]);
        xSemaphoreGive(dataMutex);
      }
    }

    // Node watchdog: no heartbeat for NODE_TIMEOUT_MS means the sensor node is lost.
    uint32_t last;
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    last = lastHeartbeatMs;
    xSemaphoreGive(dataMutex);
    nodeOnline = (last != 0) && (millis() - last < NODE_TIMEOUT_MS);

    if (wasOnline && !nodeOnline) {
      Serial.println("[ALERT] Sensor node lost (no heartbeat)");
      logLine(timestamp() + ",,,,node_lost");
    } else if (!wasOnline && nodeOnline) {
      Serial.println("[INFO] Sensor node online");
      logLine(timestamp() + ",,,,node_online");
    }
    wasOnline = nodeOnline;
  }
}

// ---------- Task 2: OLED, LEDs and buzzer ----------
void displayTask(void *) {
  bool beep = false;
  for (;;) {
    Snapshot s = snapshot();
    bool wifiUp = (WiFi.status() == WL_CONNECTED);
    bool sensorOk = s.status & STATUS_SENSOR_OK;
    bool hot = s.haveData && s.d.tempC > TEMP_ALERT_C;

    if (oledOk) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("GATEWAY");
      display.setCursor(80, 0);
      display.print(wifiUp ? "WiFi OK" : "OFFLINE");

      if (!s.online) {
        display.setTextSize(2);
        display.setCursor(0, 16);
        display.print("NODE LOST");
        display.setTextSize(1);
        display.setCursor(0, 38);
        display.print("Check CAN wires");
      } else if (!sensorOk || !s.haveData) {
        display.setCursor(0, 20);
        display.print("Node online,");
        display.setCursor(0, 32);
        display.print("SENSOR ERROR");
      } else {
        display.setTextSize(2);
        display.setCursor(0, 12);
        display.printf("%.1f C", s.d.tempC);
        display.setTextSize(1);
        display.setCursor(0, 32);
        if (isnan(s.d.humidity)) display.print("Hum:   n/a (BMP280)");
        else display.printf("Hum:   %.1f %%", s.d.humidity);
        display.setCursor(0, 42);
        display.printf("Press: %.1f hPa", s.d.pressureHpa);
      }

      display.setCursor(0, 56);
      display.printf("%s RX:%lu", sdOk ? "SD" : "SD!", (unsigned long)framesReceived);
      if (hot) { display.setCursor(104, 56); display.print("HOT"); }
      display.display();
    }

    bool problem = !s.online || !sensorOk || !sdOk || hot;
    digitalWrite(PIN_LED_RED, problem ? HIGH : LOW);
    digitalWrite(PIN_LED_GREEN, (s.online && sensorOk) ? HIGH : LOW);
    beep = (hot || (!s.online && s.haveData)) ? !beep : false;   // no beeping before the first contact
    digitalWrite(PIN_BUZZER, beep ? HIGH : LOW);

    vTaskDelay(pdMS_TO_TICKS(DISPLAY_PERIOD_MS));
  }
}

// ---------- Task 3: SD card logger ----------
void loggerTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(LOG_PERIOD_MS));
    Snapshot s = snapshot();
    if (!s.haveData || !s.online) continue;   // node events are logged by canRxTask
    char line[128];
    String hum = isnan(s.d.humidity) ? String("") : String(s.d.humidity, 2);   // empty cell on a BMP280
    snprintf(line, sizeof(line), "%s,%.2f,%s,%.1f,%s",
             timestamp().c_str(), s.d.tempC, hum.c_str(), s.d.pressureHpa,
             WiFi.status() == WL_CONNECTED ? "online" : "offline");
    logLine(String(line));
    Serial.printf("[SD] %s (lines: %lu)\n", sdOk ? "Logged" : "Failed", (unsigned long)linesLogged);
  }
}

// ---------- Task 4: Wi-Fi and ThingSpeak ----------
void cloudTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(UPLOAD_PERIOD_MS));

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WIFI] Offline, reconnecting. Data is still being saved to SD.");
      WiFi.reconnect();
      continue;
    }

    Snapshot s = snapshot();
    String url = "http://api.thingspeak.com/update?api_key=" + String(THINGSPEAK_WRITE_KEY);
    if (s.haveData && s.online) {
      url += "&field1=" + String(s.d.tempC, 2) + "&field3=" + String(s.d.pressureHpa, 1);
      if (!isnan(s.d.humidity)) url += "&field2=" + String(s.d.humidity, 2);   // skipped on a BMP280
    }
    url += "&field4=" + String(s.online ? 1 : 0);   // node status is uploaded even when it is lost

    HTTPClient http;
    http.setTimeout(5000);
    http.begin(url);
    int code = http.GET();
    String body = (code > 0) ? http.getString() : "";
    http.end();

    if (code == 200 && body.toInt() > 0) {
      Serial.printf("[CLOUD] Uploaded, entry %s\n", body.c_str());
    } else {
      Serial.printf("[CLOUD] Upload failed, HTTP %d, reply '%s'\n", code, body.c_str());
    }
  }
}

// ---------- Task 5: CAN health ----------
void canHealthTask(void *) {
  for (;;) {
    twai_status_info_t st;
    if (twai_get_status_info(&st) == ESP_OK) {
      if (st.state == TWAI_STATE_BUS_OFF) {
        Serial.println("[CAN] Bus off, starting recovery");
        twai_initiate_recovery();
      } else if (st.state == TWAI_STATE_STOPPED) {
        twai_start();
      }
      Serial.printf("[CAN] rx=%lu lost=%lu rxErr=%lu missed=%lu node=%s\n",
                    (unsigned long)framesReceived, (unsigned long)framesLost,
                    (unsigned long)st.rx_error_counter, (unsigned long)st.rx_missed_count,
                    nodeOnline ? "online" : "LOST");
    }
    vTaskDelay(pdMS_TO_TICKS(HEALTH_PERIOD_MS));
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== EnviroNode Gateway Node (ESP32 #2) ===");

  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  dataMutex = xSemaphoreCreateMutex();
  sdMutex = xSemaphoreCreateMutex();

  // OLED (only the display task uses I2C on this node, so no I2C mutex is needed here)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Gateway starting...");
    display.display();
  } else {
    Serial.println("[OLED] Not found at 0x3C");
  }

  // SD card
  sdOk = SD.begin(PIN_SD_CS);
  if (sdOk) {
    if (!SD.exists(LOG_FILE)) {
      File f = SD.open(LOG_FILE, FILE_WRITE);
      if (f) { f.println("timestamp,temperature_c,humidity_pct,pressure_hpa,event_or_wifi"); f.close(); }
    }
    Serial.println("[SD] Card ready");
  } else {
    Serial.println("[SD] Card not found");
  }

  // CAN
  if (!canBegin()) {
    Serial.println("[CAN] Driver failed to start. Check pins in config.h.");
  } else {
    Serial.println("[CAN] Started at 500 kbit/s, waiting for the sensor node...");
  }

  // Wi-Fi (non-blocking after a short try)
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting");
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " not yet, will keep trying");
  configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);

  xTaskCreatePinnedToCore(canRxTask,     "canRx",     4096, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(displayTask,   "display",   4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(loggerTask,    "logger",    6144, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(cloudTask,     "cloud",     8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(canHealthTask, "canHealth", 4096, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
