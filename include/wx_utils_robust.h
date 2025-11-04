/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * ENHANCED VERSION with Robust Sensor Fallback & Recovery
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef WX_UTILS_ROBUST_H_
#define WX_UTILS_ROBUST_H_

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME680.h>
#include "Adafruit_Si7021.h"
#include <Arduino.h>

namespace WX_Utils {

    // Original functions (backward compatible)
    void    setup();
    String  generateTempString(const float sensorTemp);
    String  generateHumString(const float sensorHum);
    String  generatePresString(const float sensorPres);
    String  readDataSensor();

    // New functions for enhanced robustness
    bool    scanForSensorWithTimeout(uint32_t timeoutMs);
    bool    initializeSensorWithTimeout();
    void    attemptRecovery();
    void    checkSensorHealth();
    String  getSensorStatusJSON();
    void    resetSensor();

}

#endif
