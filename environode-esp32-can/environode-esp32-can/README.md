# EnviroNode: Two-Node Environmental Monitor over CAN Bus (ESP32)

Two ESP32 boards working as a small CAN network. The **sensor node** reads temperature and pressure from a BMP280 and sends them over a **CAN bus** every second, together with a heartbeat. The **gateway node** receives the data, shows it on an OLED screen and uploads it to ThingSpeak over Wi-Fi. If the sensor node stops sending heartbeats, the gateway shows **NODE LOST** within 3 seconds and recovers on its own when the node comes back.

![Both nodes running](media/setup.jpg)
<!-- Put your photo in the media folder with this name, or change the file name above. -->

**Demo video:** https://drive.google.com/file/d/1ptrkx0qqCUBiUvJTJoKWSxSjPf0UULCK/view?usp=drivesdk

## What it does

| Feature | How |
|---|---|
| Sensor reading | Own register-level BMP280/BME280 driver over I2C, written from the Bosch datasheet (chip ID check, calibration, compensation formulas). No sensor library. |
| Node-to-node link | ESP32 built-in CAN controller (TWAI) + SN65HVD230 transceivers, 500 kbit/s, twisted pair |
| Custom CAN protocol | ENV_DATA (ID 0x100) with scaled integers and a sequence number; HEARTBEAT (ID 0x101) with status and uptime |
| Fault detection | Gateway watchdog: no heartbeat for 3 s = NODE LOST alert; automatic bus-off recovery on both nodes |
| Display | 0.96" SSD1306 OLED on the gateway: temperature, pressure, Wi-Fi state, frames received, node status |
| Cloud | ThingSpeak upload every 20 s (temperature, pressure, sensor node online 1/0), with automatic Wi-Fi reconnect |
| Concurrency | FreeRTOS tasks and mutexes on both nodes (sensor, CAN TX, CAN RX, display, cloud, CAN health) |

## System design

```
[Sensor node: ESP32 #2]  ==== CAN bus, 500 kbit/s ====  [Gateway node: ESP32 #1]
 BMP280 (I2C, own driver)                                OLED (I2C)
 SN65HVD230 transceiver                                  SN65HVD230 transceiver
 Sends ENV_DATA 0x100 + HEARTBEAT 0x101 every 1 s        Node-lost alert after 3 s of silence
 FreeRTOS: sensor, CAN TX, CAN health                    ThingSpeak upload over Wi-Fi
                                                         FreeRTOS: CAN RX, display, cloud, CAN health
```

### CAN messages

| ID | Name | Bytes | Content |
|---|---|---|---|
| 0x100 | ENV_DATA | 8 | temperature x100 (int16), humidity x100 (uint16, 0xFFFF = not available), pressure x10 (uint16), sequence, status |
| 0x101 | HEARTBEAT | 5 | status, uptime in seconds (uint32) |

Values are sent as scaled integers, low byte first. The sequence number lets the gateway count lost frames.

## Results

- Stage 1 (single ESP32): BMP280 readings on the OLED and on ThingSpeak, Wi-Fi drop and reconnect handled.
- Stage 2 (two ESP32s over CAN): 120+ frames received with **0 lost** and **0 receive errors**.
- Removing power from the sensor node: gateway showed **NODE LOST** within 3 s; recovered automatically when power returned.

<!-- Add the ThingSpeak graph screenshot: media/thingspeak.png -->

## Hardware

| Part | Qty |
|---|---|
| ESP32 DevKit V1 (30 pin) | 2 |
| BMP280 sensor module (I2C) | 1 |
| 0.96" SSD1306 OLED, I2C | 1 |
| SN65HVD230 CAN transceiver module | 2 |
| MB-102 breadboard | 2 |
| Twisted pair wire for CANH/CANL, jumper wires | - |

Wiring diagrams: [Stage 1](docs/stage1_wiring.png) · [Stage 2](docs/stage2_wiring.png)

## Folder guide

| Folder | What it is |
|---|---|
| `00_Tests/` | Small test sketches used to bring up each part (blink, I2C scanner, sensor, OLED, Wi-Fi, CAN sender/receiver) and debug tools (I2C line check, power check, SD diagnostics) |
| `Stage1_Env_Monitor/` | Stage 1: everything on one ESP32 |
| `Stage2_Sensor_Node/` | Stage 2: sensor node (BMP280 + CAN) |
| `Stage2_Gateway_Node/` | Stage 2: gateway node (CAN + OLED + Wi-Fi) |
| `docs/` | Wiring diagrams |

## How to run

1. Install the Arduino IDE and the **esp32 by Espressif Systems** board package.
2. Install the libraries **Adafruit SSD1306** and **Adafruit GFX Library**.
3. In `Stage2_Gateway_Node/config.h` (and `Stage1_Env_Monitor/config.h`) fill in your Wi-Fi name, password and ThingSpeak Write API Key. **Never commit real keys.**
4. Upload `Stage2_Sensor_Node` to the sensor board and `Stage2_Gateway_Node` to the gateway board. Serial Monitor: 115200 baud.

## Problems I hit and how I fixed them

| Problem | Cause | Fix |
|---|---|---|
| I2C "probe device timeout", no devices found | Breadboard power rails are split in the middle; modules were on the unpowered half | Moved all power wires to the rail half next to the ESP32 |
| Readings jumping between 0 V and 3.3 V | Worn contacts at the end of the breadboard where the ESP32 sat | Measured with an ESP32-based voltage probe sketch, moved the ESP32 to fresh holes |
| CAN "Send failed" | The other node was not running a CAN program, so its transceiver held the bus | Checked the CRX line level, then started the receiver first |
| Compile error "bootloader.bin was unexpected" | Brackets in the Windows folder path | Saved sketches in a plain folder path |
| SD card module not detected | Module with 5 V regulator and level shifter did not respond (MISO floating in every test) | Diagnosed with CMD0 and wire-test sketches; SD logging moved to future work |

## Future work

- SD card logging (code is in place; needs a working 3.3 V SD module)
- BME280 for humidity
- Custom PCB for each node in KiCad
