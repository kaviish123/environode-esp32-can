// BME280Driver.h
// A small BME280 / BMP280 driver written from the Bosch datasheets.
// BME280 (chip ID 0x60): temperature, humidity, pressure.
// BMP280 (chip ID 0x58): temperature and pressure only (humidity is reported as NAN).
// It talks to the sensor register by register over I2C, without the Adafruit library.

#pragma once
#include <Arduino.h>
#include <Wire.h>

class BME280Driver {
public:
  // Finds the sensor at 0x76 or 0x77, checks its chip ID,
  // reads the calibration data and starts measuring.
  bool begin(TwoWire &wire);

  // Reads one set of values. Returns false if the I2C read fails.
  // On a BMP280, humidityPct is set to NAN (not a number) because the chip has no humidity sensor.
  bool read(float &tempC, float &humidityPct, float &pressureHpa);

  uint8_t address() const { return _addr; }
  uint8_t chipId() const { return _chipId; }
  bool hasHumidity() const { return _chipId == 0x60; }
  const char *name() const { return hasHumidity() ? "BME280" : "BMP280"; }

private:
  // Register addresses from the datasheet
  static const uint8_t REG_CHIP_ID   = 0xD0;  // should read 0x60 for BME280
  static const uint8_t REG_RESET     = 0xE0;  // write 0xB6 to reset
  static const uint8_t REG_CTRL_HUM  = 0xF2;
  static const uint8_t REG_STATUS    = 0xF3;
  static const uint8_t REG_CTRL_MEAS = 0xF4;
  static const uint8_t REG_CONFIG    = 0xF5;
  static const uint8_t REG_DATA      = 0xF7;  // 8 bytes: pressure(3), temp(3), humidity(2)
  static const uint8_t REG_CALIB_A   = 0x88;  // 26 bytes
  static const uint8_t REG_CALIB_B   = 0xE1;  // 7 bytes

  bool writeReg(uint8_t reg, uint8_t value);
  bool readRegs(uint8_t reg, uint8_t *buf, size_t len);
  bool readCalibration();

  int32_t  compensateTemp(int32_t adcT);       // returns 0.01 degC
  uint32_t compensatePressure(int32_t adcP);   // returns Pa * 256
  uint32_t compensateHumidity(int32_t adcH);   // returns %RH * 1024

  TwoWire *_wire = nullptr;
  uint8_t  _addr = 0;
  uint8_t  _chipId = 0;
  int32_t  _tFine = 0;

  // Factory calibration values stored inside each sensor
  uint16_t dig_T1; int16_t dig_T2, dig_T3;
  uint16_t dig_P1; int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
  uint8_t  dig_H1, dig_H3; int16_t dig_H2, dig_H4, dig_H5; int8_t dig_H6;
};
