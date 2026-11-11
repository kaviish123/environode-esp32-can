// config.h  (Gateway Node, ESP32 #2)
// Edit Wi-Fi and ThingSpeak details before uploading.

#pragma once

// ---------- Wi-Fi (2.4 GHz only) ----------
#define WIFI_SSID      "YOUR_WIFI_NAME"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

// ---------- ThingSpeak ----------
// Channel fields: 1 = Temperature, 2 = Humidity, 3 = Pressure, 4 = Sensor node online (1/0)
#define THINGSPEAK_WRITE_KEY  "YOUR_WRITE_API_KEY"

// ---------- Time (India = UTC+5:30) ----------
#define GMT_OFFSET_SEC   19800
#define NTP_SERVER       "pool.ntp.org"

// ---------- Pins ----------
#define PIN_CAN_TX    17     // -> CTX on SN65HVD230
#define PIN_CAN_RX    16     // <- CRX on SN65HVD230
#define PIN_I2C_SDA   21     // OLED
#define PIN_I2C_SCL   22
#define PIN_SD_CS     5      // SD: SCK 18, MISO 19, MOSI 23
#define PIN_LED_GREEN 25
#define PIN_LED_RED   26
#define PIN_BUZZER    27

// ---------- OLED ----------
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_ADDR     0x3C

// ---------- Timing (milliseconds) ----------
#define NODE_TIMEOUT_MS    3000    // no heartbeat for 3 s = sensor node lost
#define DISPLAY_PERIOD_MS  1000
#define LOG_PERIOD_MS      30000
#define UPLOAD_PERIOD_MS   20000   // ThingSpeak free plan: max 1 update per 15 s
#define HEALTH_PERIOD_MS   2000

// ---------- Alerts ----------
#define TEMP_ALERT_C       40.0f

// ---------- Log file ----------
#define LOG_FILE  "/log.csv"
