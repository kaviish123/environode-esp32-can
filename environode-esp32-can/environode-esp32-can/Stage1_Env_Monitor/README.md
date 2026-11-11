# ESP32 Environment Monitor and Data Logger (Stage 1)

An ESP32 that measures temperature, humidity and air pressure, shows them on an OLED screen, logs them to a micro SD card and uploads them to ThingSpeak. Logging continues when Wi-Fi is down, so no readings are lost.

<!-- Add a photo of the setup here -->
<!-- Add a screenshot of the ThingSpeak graph here -->

## Features

- **Own BME280 driver:** reads the sensor register by register over I2C, including the calibration formulas from the Bosch datasheet (no sensor library).
- **FreeRTOS:** four tasks (sensor, display, SD logger, cloud upload) so a slow Wi-Fi connection never freezes the screen or the logging.
- **Shared I2C bus:** the sensor and OLED use the same two wires, protected by a mutex so two tasks never use the bus at once.
- **SD logging over SPI:** one CSV line every 30 seconds with a real timestamp from NTP.
- **Wi-Fi upload:** readings go to ThingSpeak every 20 seconds.
- **Failure handling:** keeps logging offline, reconnects Wi-Fi on its own, retries the SD card if it is removed, and shows sensor errors on screen.
- **Alerts:** red LED and buzzer when the temperature goes above a set limit.

## Hardware

| Part | Qty |
|---|---|
| ESP32 DevKit V1 (30 pin) | 1 |
| BME280 sensor (or BMP280: no humidity), 3.3V | 1 |
| 0.96" SSD1306 OLED, I2C, 4 pin | 1 |
| Micro SD card module (SPI) and a FAT32 card | 1 |
| Breadboard, jumper wires, micro-USB data cable | - |
| Optional: 2 LEDs with 220 ohm resistors, active buzzer | - |

## Wiring

| Module pin | ESP32 pin |
|---|---|
| BME280 SDA, OLED SDA | GPIO21 |
| BME280 SCL, OLED SCL | GPIO22 |
| BME280 VCC, OLED VCC | 3V3 |
| SD CS / SCK / MOSI / MISO | GPIO5 / GPIO18 / GPIO23 / GPIO19 |
| SD VCC | VIN (5V) if the module has a regulator, else 3V3 |
| Green LED / Red LED / Buzzer | GPIO25 / GPIO26 / GPIO27 |
| All GND | GND |

<!-- Add the wiring diagram image here -->

## How to run

1. Install the Arduino IDE and add the ESP32 board package (Boards Manager, search "esp32" by Espressif).
2. Install the libraries **Adafruit SSD1306** and **Adafruit GFX Library**.
3. Open `Stage1_Env_Monitor.ino`. Edit `config.h` with your Wi-Fi name, password and ThingSpeak Write API key.
4. Select **Tools > Board > ESP32 Dev Module** and the right COM port, then upload.
5. Open the Serial Monitor at **115200 baud** to see debug messages.

## Log file format

```
timestamp,temperature_c,humidity_pct,pressure_hpa,wifi
2026-10-05 14:30:00,29.41,61.20,1008.33,online
```

## What I learned

<!-- Write this yourself: problems you hit and how you fixed them. -->

## Next stage

Stage 2 (folders Stage2_Sensor_Node and Stage2_Gateway_Node) splits this into two ESP32 nodes connected over CAN bus.
