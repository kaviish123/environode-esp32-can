// config.h  (Sensor Node, ESP32 #1)

#pragma once

// I2C: BME280
#define PIN_I2C_SDA   21
#define PIN_I2C_SCL   22

// CAN transceiver (SN65HVD230): ESP32 TX -> CTX, ESP32 RX <- CRX
#define PIN_CAN_TX    17
#define PIN_CAN_RX    16

// Onboard LED: blinks each time a frame is sent successfully
#define PIN_LED       2

// Timing (milliseconds)
#define SENSOR_PERIOD_MS     1000
#define ENV_TX_PERIOD_MS     1000
#define HEARTBEAT_PERIOD_MS  1000
#define HEALTH_PERIOD_MS     2000
