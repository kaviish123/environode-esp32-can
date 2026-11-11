// BME280Driver.cpp
// Register-level BME280 driver. Formulas follow section 4.2.3 and 8.2 of the Bosch datasheet.

#include "BME280Driver.h"

bool BME280Driver::begin(TwoWire &wire) {
  _wire = &wire;

  // 1. Find the sensor: it answers at 0x76 or 0x77 depending on the SDO pin.
  const uint8_t candidates[] = {0x76, 0x77};
  _addr = 0;
  for (uint8_t a : candidates) {
    _wire->beginTransmission(a);
    if (_wire->endTransmission() == 0) { _addr = a; break; }
  }
  if (_addr == 0) {
    Serial.println("[BME280] No device found at 0x76 or 0x77. Check wiring.");
    return false;
  }

  // 2. Check the chip ID. 0x60 = BME280. 0x58 = BMP280 (no humidity).
  if (!readRegs(REG_CHIP_ID, &_chipId, 1)) return false;
  if (_chipId == 0x58) {
    Serial.println("[BME280] BMP280 detected: temperature and pressure only, no humidity.");
  } else if (_chipId != 0x60) {
    Serial.printf("[BME280] Unexpected chip ID 0x%02X\n", _chipId);
    return false;
  }

  // 3. Soft reset, then wait until the sensor has copied its calibration into registers.
  writeReg(REG_RESET, 0xB6);
  delay(10);
  uint8_t status = 0;
  for (int i = 0; i < 20; i++) {
    readRegs(REG_STATUS, &status, 1);
    if ((status & 0x01) == 0) break;   // bit 0 "im_update" clears when done
    delay(5);
  }

  // 4. Read the factory calibration values.
  if (!readCalibration()) return false;

  // 5. Configure: humidity x1 oversampling (must be written before ctrl_meas),
  //    temperature x1, pressure x1, normal mode, 1000 ms standby, filter off.
  if (hasHumidity()) writeReg(REG_CTRL_HUM, 0x01);   // the BMP280 has no humidity register
  writeReg(REG_CONFIG, 0xA0);
  writeReg(REG_CTRL_MEAS, 0x27);

  Serial.printf("[BME280] %s found at 0x%02X, chip ID 0x%02X\n", name(), _addr, _chipId);
  delay(20);  // give it time for the first measurement
  return true;
}

bool BME280Driver::read(float &tempC, float &humidityPct, float &pressureHpa) {
  uint8_t d[8] = {0};
  // BME280 has 8 data bytes (pressure, temperature, humidity). BMP280 has 6 (no humidity).
  if (!readRegs(REG_DATA, d, hasHumidity() ? 8 : 6)) return false;

  // Raw values are 20-bit (pressure, temperature) and 16-bit (humidity).
  int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
  int32_t adcH = ((int32_t)d[6] << 8)  |  d[7];

  // 0x80000 means "no measurement yet"
  if (adcT == 0x80000) return false;

  // Temperature must be calculated first: it sets _tFine, which the others need.
  tempC       = compensateTemp(adcT) / 100.0f;
  pressureHpa = compensatePressure(adcP) / 256.0f / 100.0f;
  humidityPct = hasHumidity() ? compensateHumidity(adcH) / 1024.0f : NAN;
  return true;
}

// ---------- I2C helpers ----------

bool BME280Driver::writeReg(uint8_t reg, uint8_t value) {
  _wire->beginTransmission(_addr);
  _wire->write(reg);
  _wire->write(value);
  return _wire->endTransmission() == 0;
}

bool BME280Driver::readRegs(uint8_t reg, uint8_t *buf, size_t len) {
  _wire->beginTransmission(_addr);
  _wire->write(reg);                              // tell the sensor which register to start from
  if (_wire->endTransmission(false) != 0) return false;   // repeated start, keep the bus
  size_t got = _wire->requestFrom((uint16_t)_addr, (size_t)len, true);
  if (got != len) return false;
  for (size_t i = 0; i < len; i++) buf[i] = _wire->read();
  return true;
}

bool BME280Driver::readCalibration() {
  uint8_t a[26];
  uint8_t b[7] = {0};
  if (!readRegs(REG_CALIB_A, a, 26)) return false;
  if (hasHumidity() && !readRegs(REG_CALIB_B, b, 7)) return false;   // humidity calibration (BME280 only)

  // Values are stored little-endian (low byte first).
  dig_T1 = (uint16_t)(a[1] << 8 | a[0]);
  dig_T2 = (int16_t)(a[3] << 8 | a[2]);
  dig_T3 = (int16_t)(a[5] << 8 | a[4]);
  dig_P1 = (uint16_t)(a[7] << 8 | a[6]);
  dig_P2 = (int16_t)(a[9] << 8 | a[8]);
  dig_P3 = (int16_t)(a[11] << 8 | a[10]);
  dig_P4 = (int16_t)(a[13] << 8 | a[12]);
  dig_P5 = (int16_t)(a[15] << 8 | a[14]);
  dig_P6 = (int16_t)(a[17] << 8 | a[16]);
  dig_P7 = (int16_t)(a[19] << 8 | a[18]);
  dig_P8 = (int16_t)(a[21] << 8 | a[20]);
  dig_P9 = (int16_t)(a[23] << 8 | a[22]);
  dig_H1 = a[25];                                       // register 0xA1

  dig_H2 = (int16_t)(b[1] << 8 | b[0]);                 // 0xE1, 0xE2
  dig_H3 = b[2];                                        // 0xE3
  dig_H4 = (int16_t)(((int8_t)b[3]) * 16) | (b[4] & 0x0F);   // 0xE4, 0xE5[3:0]
  dig_H5 = (int16_t)(((int8_t)b[5]) * 16) | (b[4] >> 4);     // 0xE6, 0xE5[7:4]
  dig_H6 = (int8_t)b[6];                                // 0xE7
  return true;
}

// ---------- Compensation formulas (from the datasheet) ----------

int32_t BME280Driver::compensateTemp(int32_t adcT) {
  int32_t var1 = ((((adcT >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
  int32_t var2 = (((((adcT >> 4) - ((int32_t)dig_T1)) * ((adcT >> 4) - ((int32_t)dig_T1))) >> 12) *
                  ((int32_t)dig_T3)) >> 14;
  _tFine = var1 + var2;
  return (_tFine * 5 + 128) >> 8;
}

uint32_t BME280Driver::compensatePressure(int32_t adcP) {
  int64_t var1 = ((int64_t)_tFine) - 128000;
  int64_t var2 = var1 * var1 * (int64_t)dig_P6;
  var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
  var2 = var2 + (((int64_t)dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
  if (var1 == 0) return 0;   // avoid division by zero
  int64_t p = 1048576 - adcP;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
  return (uint32_t)p;
}

uint32_t BME280Driver::compensateHumidity(int32_t adcH) {
  int32_t v = (_tFine - ((int32_t)76800));
  v = (((((adcH << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v)) + ((int32_t)16384)) >> 15) *
       (((((((v * ((int32_t)dig_H6)) >> 10) * (((v * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
          ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
  v = (v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
  v = (v < 0 ? 0 : v);
  v = (v > 419430400 ? 419430400 : v);
  return (uint32_t)(v >> 12);
}
