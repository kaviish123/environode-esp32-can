// Stage2_Sensor_Node.ino  (upload to ESP32 #1)
// EnviroNode sensor node: reads the BME280 with our own I2C driver and sends the
// readings plus a heartbeat over CAN bus to the gateway node.
//
// FreeRTOS tasks:
//   sensorTask    reads the BME280 every second
//   canTxTask     sends ENV_DATA and HEARTBEAT frames every second
//   canHealthTask watches the CAN controller and recovers from "bus off"
//
// No extra libraries needed: CAN uses the ESP32's built-in TWAI controller.
// Board: Tools > Board > esp32 > "ESP32 Dev Module"

#include <Wire.h>
#include "driver/twai.h"
#include "config.h"
#include "can_protocol.h"
#include "BME280Driver.h"

BME280Driver bme;

EnvData latest = {NAN, NAN, NAN, 0, 0};
SemaphoreHandle_t dataMutex;
volatile bool sensorOk = false;
volatile uint32_t framesSent = 0;
volatile uint32_t framesFailed = 0;

// ---------- CAN setup ----------
bool canBegin() {
  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)PIN_CAN_TX, (gpio_num_t)PIN_CAN_RX, TWAI_MODE_NORMAL);
  g.tx_queue_len = 10;
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &t, &f) != ESP_OK) return false;
  return twai_start() == ESP_OK;
}

bool canSend(uint32_t id, const uint8_t *data, uint8_t len) {
  twai_message_t msg = {};
  msg.identifier = id;
  msg.data_length_code = len;
  memcpy(msg.data, data, len);
  if (twai_transmit(&msg, pdMS_TO_TICKS(50)) == ESP_OK) {
    framesSent++;
    return true;
  }
  framesFailed++;
  return false;
}

// ---------- Task 1: read the sensor ----------
void sensorTask(void *) {
  for (;;) {
    float t, h, p;
    bool ok = bme.read(t, h, p);
    if (ok) {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      latest.tempC = t;
      latest.humidity = h;
      latest.pressureHpa = p;
      xSemaphoreGive(dataMutex);
    }
    sensorOk = ok;
    vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));
  }
}

// ---------- Task 2: send data and heartbeat over CAN ----------
void canTxTask(void *) {
  uint8_t seq = 0;
  uint32_t lastEnv = 0, lastBeat = 0;
  for (;;) {
    uint32_t now = millis();
    uint8_t status = sensorOk ? STATUS_SENSOR_OK : 0;

    if (now - lastEnv >= ENV_TX_PERIOD_MS) {
      lastEnv = now;
      if (sensorOk) {
        EnvData d;
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        d = latest;
        xSemaphoreGive(dataMutex);
        d.sequence = seq++;
        d.status = status;
        uint8_t buf[8];
        encodeEnv(d, buf);
        if (canSend(CAN_ID_ENV_DATA, buf, 8)) {
          digitalWrite(PIN_LED, HIGH);
          Serial.printf("[CAN TX] ENV #%u  %.2f C  %.2f %%  %.1f hPa\n", d.sequence, d.tempC, d.humidity, d.pressureHpa);
        }
      }
    }

    if (now - lastBeat >= HEARTBEAT_PERIOD_MS) {
      lastBeat = now;
      uint8_t hb[5];
      hb[0] = status;
      put32(&hb[1], millis() / 1000);
      canSend(CAN_ID_HEARTBEAT, hb, 5);   // heartbeat goes out even if the sensor fails
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    digitalWrite(PIN_LED, LOW);
  }
}

// ---------- Task 3: CAN health and recovery ----------
void canHealthTask(void *) {
  for (;;) {
    twai_status_info_t s;
    if (twai_get_status_info(&s) == ESP_OK) {
      if (s.state == TWAI_STATE_BUS_OFF) {
        Serial.println("[CAN] Bus off (too many errors). Starting recovery.");
        twai_initiate_recovery();
      } else if (s.state == TWAI_STATE_STOPPED) {
        Serial.println("[CAN] Recovered, restarting controller.");
        twai_start();
      } else if (s.tx_error_counter > 96) {
        Serial.println("[CAN] Frames not acknowledged. Is the gateway connected and running?");
      }
      Serial.printf("[CAN] sent=%lu failed=%lu txErr=%lu rxErr=%lu sensor=%s\n",
                    (unsigned long)framesSent, (unsigned long)framesFailed,
                    (unsigned long)s.tx_error_counter, (unsigned long)s.rx_error_counter,
                    sensorOk ? "OK" : "ERROR");
    }
    vTaskDelay(pdMS_TO_TICKS(HEALTH_PERIOD_MS));
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== EnviroNode Sensor Node (ESP32 #1) ===");
  pinMode(PIN_LED, OUTPUT);
  dataMutex = xSemaphoreCreateMutex();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  sensorOk = bme.begin(Wire);
  if (!sensorOk) Serial.println("[BME280] Not ready. Heartbeats will still be sent with status = error.");

  if (!canBegin()) {
    Serial.println("[CAN] Driver failed to start. Check pins in config.h.");
    while (true) { digitalWrite(PIN_LED, !digitalRead(PIN_LED)); delay(100); }  // fast blink = fatal error
  }
  Serial.println("[CAN] Started at 500 kbit/s");

  xTaskCreatePinnedToCore(sensorTask,    "sensor",    4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(canTxTask,     "canTx",     4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(canHealthTask, "canHealth", 4096, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
