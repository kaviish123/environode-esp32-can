// config.h
// All settings in one place. Edit this file before uploading.

#pragma once

// ---------- Wi-Fi ----------
#define WIFI_SSID      "YOUR_WIFI_NAME"
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"

// ---------- ThingSpeak ----------
// Create a free channel at thingspeak.com with 3 fields:
// Field 1 = Temperature, Field 2 = Humidity, Field 3 = Pressure
// Then paste the channel's "Write API Key" here.
#define THINGSPEAK_WRITE_KEY  "YOUR_WRITE_API_KEY"

// ---------- Time (India = UTC+5:30) ----------
#define GMT_OFFSET_SEC   19800
#define NTP_SERVER       "pool.ntp.org"

// ---------- Pins ----------
// I2C (BME280 + OLED share these two wires)
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22

// SPI (micro SD card). SCK=18, MISO=19, MOSI=23 are the ESP32 defaults.
#define PIN_SD_CS     5

// Optional status outputs (leave unconnected if not used)
#define PIN_LED_GREEN 25
#define PIN_LED_RED   26
#define PIN_BUZZER    27

// ---------- OLED ----------
#define OLED_WIDTH    128
#define OLED_HEIGHT   64
#define OLED_ADDR     0x3C

// ---------- Timing (milliseconds) ----------
#define SENSOR_PERIOD_MS   2000    // read the sensor every 2 s
#define DISPLAY_PERIOD_MS  1000    // refresh the screen every 1 s
#define LOG_PERIOD_MS      30000   // write one line to the SD card every 30 s
#define UPLOAD_PERIOD_MS   20000   // ThingSpeak free plan allows 1 update per 15 s

// ---------- Alerts ----------
#define TEMP_ALERT_C       40.0f   // buzzer + red LED above this temperature

// ---------- Log file ----------
#define LOG_FILE  "/log.csv"
