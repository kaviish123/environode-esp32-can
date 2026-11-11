// can_protocol.h
// The "language" both nodes speak on the CAN bus.
// This file must be identical in Stage2_Sensor_Node and Stage2_Gateway_Node.
//
// CAN bus speed: 500 kbit/s, standard 11-bit IDs.
//
// ID 0x100  ENV_DATA   (sensor node -> gateway), every 1 s, 8 bytes:
//   byte 0-1  temperature  int16,  degC x 100   (2945 = 29.45 C)
//   byte 2-3  humidity     uint16, %RH x 100    (6120 = 61.20 %), 0xFFFF = no humidity sensor (BMP280)
//   byte 4-5  pressure     uint16, hPa x 10     (10083 = 1008.3 hPa)
//   byte 6    sequence     uint8, counts 0..255 so the gateway can spot lost frames
//   byte 7    status       bit0 = sensor OK
//
// ID 0x101  HEARTBEAT  (sensor node -> gateway), every 1 s, 5 bytes:
//   byte 0    status       bit0 = sensor OK
//   byte 1-4  uptime       uint32, seconds since the sensor node started
//
// If the gateway hears no heartbeat for NODE_TIMEOUT_MS it raises "SENSOR NODE LOST".

#pragma once
#include <Arduino.h>

#define CAN_ID_ENV_DATA    0x100
#define CAN_ID_HEARTBEAT   0x101

#define STATUS_SENSOR_OK   0x01
#define HUMIDITY_NONE      0xFFFF

struct EnvData {
  float tempC;
  float humidity;
  float pressureHpa;
  uint8_t sequence;
  uint8_t status;
};

// Little-endian packing helpers (low byte first)
inline void put16(uint8_t *b, uint16_t v) { b[0] = v & 0xFF; b[1] = v >> 8; }
inline uint16_t get16(const uint8_t *b) { return (uint16_t)b[0] | ((uint16_t)b[1] << 8); }
inline void put32(uint8_t *b, uint32_t v) { for (int i = 0; i < 4; i++) b[i] = (v >> (8 * i)) & 0xFF; }
inline uint32_t get32(const uint8_t *b) {
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

// Floats are sent as scaled integers: smaller, and both sides agree exactly on the format.
inline void encodeEnv(const EnvData &d, uint8_t out[8]) {
  put16(&out[0], (uint16_t)(int16_t)lroundf(d.tempC * 100.0f));
  put16(&out[2], isnan(d.humidity) ? HUMIDITY_NONE : (uint16_t)lroundf(d.humidity * 100.0f));
  put16(&out[4], (uint16_t)lroundf(d.pressureHpa * 10.0f));
  out[6] = d.sequence;
  out[7] = d.status;
}

inline EnvData decodeEnv(const uint8_t in[8]) {
  EnvData d;
  d.tempC = (int16_t)get16(&in[0]) / 100.0f;
  uint16_t h = get16(&in[2]);
  d.humidity = (h == HUMIDITY_NONE) ? NAN : h / 100.0f;
  d.pressureHpa = get16(&in[4]) / 10.0f;
  d.sequence = in[6];
  d.status = in[7];
  return d;
}
