/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * ENHANCED VERSION with Robust Sensor Fallback & Recovery
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file replaces wx_utils.cpp with improved error handling:
 * - Timeout-based I2C operations (prevents hanging)
 * - Graceful degradation (system continues without sensor)
 * - Auto-recovery mechanism (periodic retry)
 * - Comprehensive error reporting
 * - Sensor health monitoring
 */

#include <TinyGPS++.h>
#ifdef LIGHTGATEWAY_PLUS_1_0
#include "Adafruit_SHTC3.h"
#endif
#include "configuration.h"
#include "board_pinout.h"
#include "wx_utils.h"
#include "display.h"

#define SEALEVELPRESSURE_HPA    (1013.25)
#define CORRECTION_FACTOR       (8.2296)      // for meters

// Timeout & Retry Configuration
#define I2C_SCAN_TIMEOUT_MS         1000      // Max time for I2C scan
#define SENSOR_INIT_TIMEOUT_MS      2000      // Max time for sensor init
#define SENSOR_READ_TIMEOUT_MS      1000      // Max time for sensor read
#define MAX_CONSECUTIVE_FAILURES    5         // After this, disable sensor
#define RECOVERY_ATTEMPT_INTERVAL   300000    // 5 minutes between recovery attempts
#define SENSOR_HEALTH_CHECK_INTERVAL 60000    // Check health every minute

extern Configuration            Config;
extern String                   fifthLine;
#ifdef HAS_GPS
extern TinyGPSPlus              gps;
#endif

// Sensor State Management
struct SensorState {
    int moduleType;                  // 0=none, 1=BME280, 2=BMP280, 3=BME680, 4=Si7021, 5=SHTC3
    uint8_t moduleAddress;           // I2C address
    bool isInitialized;              // Successfully initialized
    bool isHealthy;                  // Currently working
    uint32_t consecutiveFailures;    // Failure counter
    uint32_t lastSuccessfulRead;     // Timestamp of last good read
    uint32_t lastRecoveryAttempt;    // Timestamp of last recovery try
    uint32_t lastHealthCheck;        // Timestamp of last health check
    uint32_t totalReadSuccess;       // Statistics
    uint32_t totalReadFailures;      // Statistics
    String lastError;                // Last error message
};

SensorState sensorState = {0, 0x00, false, false, 0, 0, 0, 0, 0, 0, ""};

float newHum, newTemp, newPress, newGas;

// Sensor objects
Adafruit_BME280     bme280;
#if defined(HELTEC_V3) || defined(HELTEC_V3_2)
Adafruit_BMP280     bmp280(&Wire1);
Adafruit_Si7021     si7021  = Adafruit_Si7021();
#else
Adafruit_BMP280     bmp280;
Adafruit_BME680     bme680;
Adafruit_Si7021     si7021  = Adafruit_Si7021();
#endif
#ifdef LIGHTGATEWAY_PLUS_1_0
Adafruit_SHTC3      shtc3   = Adafruit_SHTC3();
#endif

namespace WX_Utils {

    /**
     * @brief Timeout-sichere I2C-Adress-Suche
     * @param timeoutMs Maximale Zeit für Scan
     * @return true wenn Sensor gefunden
     */
    bool scanForSensorWithTimeout(uint32_t timeoutMs) {
        uint32_t startTime = millis();
        uint8_t err, addr;

        Serial.println("[WX] Scanning I2C bus for weather sensors...");

        for(addr = 1; addr < 0x7F; addr++) {
            // Timeout-Check
            if (millis() - startTime > timeoutMs) {
                Serial.println("[WX] I2C scan timeout!");
                return false;
            }

            #if defined(HELTEC_V3) || defined(HELTEC_V3_2) || defined(HELTEC_WSL_V3) || defined(HELTEC_WSL_V3_DISPLAY)
                Wire1.beginTransmission(addr);
                err = Wire1.endTransmission();
            #else
                Wire.beginTransmission(addr);
                #ifdef LIGHTGATEWAY_PLUS_1_0
                    Wire.write(0x35);
                    Wire.write(0x17);
                #endif
                err = Wire.endTransmission();
            #endif

            delay(5);

            if (err == 0) {
                // Device found - check if it's a weather sensor
                if (addr == 0x76 || addr == 0x77) {
                    Serial.printf("[WX] BME/BMP sensor found at 0x%02X\n", addr);
                    sensorState.moduleAddress = addr;
                    return true;
                } else if (addr == 0x40) {
                    Serial.printf("[WX] Si7021 sensor found at 0x%02X\n", addr);
                    sensorState.moduleAddress = addr;
                    return true;
                } else if (addr == 0x70) {
                    Serial.printf("[WX] SHTC3 sensor found at 0x%02X\n", addr);
                    sensorState.moduleAddress = addr;
                    return true;
                }
            }
        }

        Serial.println("[WX] No weather sensor found on I2C bus");
        return false;
    }

    /**
     * @brief Versucht Sensor zu initialisieren mit Timeout
     * @return true wenn erfolgreich
     */
    bool initializeSensorWithTimeout() {
        if (sensorState.moduleAddress == 0x00) {
            return false;
        }

        uint32_t startTime = millis();
        bool success = false;

        Serial.printf("[WX] Attempting to initialize sensor at 0x%02X...\n",
                     sensorState.moduleAddress);

        // Try different sensor types based on address
        if (sensorState.moduleAddress == 0x76 || sensorState.moduleAddress == 0x77) {
            // BME280 / BMP280 / BME680

            #if defined(HELTEC_V3) || defined(HELTEC_V3_2) || defined(HELTEC_WSL_V3) || defined(HELTEC_WSL_V3_DISPLAY)
                if (bme280.begin(sensorState.moduleAddress, &Wire1)) {
                    bme280.setSampling(Adafruit_BME280::MODE_FORCED,
                                Adafruit_BME280::SAMPLING_X1,
                                Adafruit_BME280::SAMPLING_X1,
                                Adafruit_BME280::SAMPLING_X1,
                                Adafruit_BME280::FILTER_OFF);
                    sensorState.moduleType = 1;
                    Serial.println("[WX] ✓ BME280 initialized successfully");
                    success = true;
                }
            #else
                // Try BME280 first
                if (bme280.begin(sensorState.moduleAddress)) {
                    bme280.setSampling(Adafruit_BME280::MODE_FORCED,
                                Adafruit_BME280::SAMPLING_X1,
                                Adafruit_BME280::SAMPLING_X1,
                                Adafruit_BME280::SAMPLING_X1,
                                Adafruit_BME280::FILTER_OFF);
                    sensorState.moduleType = 1;
                    Serial.println("[WX] ✓ BME280 initialized successfully");
                    success = true;
                }

                // Try BME680 if BME280 failed
                if (!success && millis() - startTime < SENSOR_INIT_TIMEOUT_MS) {
                    if (bme680.begin(sensorState.moduleAddress)) {
                        bme680.setTemperatureOversampling(BME680_OS_1X);
                        bme680.setHumidityOversampling(BME680_OS_1X);
                        bme680.setPressureOversampling(BME680_OS_1X);
                        bme680.setIIRFilterSize(BME680_FILTER_SIZE_0);
                        sensorState.moduleType = 3;
                        Serial.println("[WX] ✓ BME680 initialized successfully");
                        success = true;
                    }
                }
            #endif

            // Try BMP280 as last resort
            if (!success && millis() - startTime < SENSOR_INIT_TIMEOUT_MS) {
                if (bmp280.begin(sensorState.moduleAddress)) {
                    bmp280.setSampling(Adafruit_BMP280::MODE_FORCED,
                                Adafruit_BMP280::SAMPLING_X1,
                                Adafruit_BMP280::SAMPLING_X1,
                                Adafruit_BMP280::FILTER_OFF);
                    sensorState.moduleType = 2;
                    Serial.println("[WX] ✓ BMP280 initialized successfully");
                    success = true;
                }
            }

        } else if (sensorState.moduleAddress == 0x40) {
            // Si7021
            if (si7021.begin()) {
                sensorState.moduleType = 4;
                Serial.println("[WX] ✓ Si7021 initialized successfully");
                success = true;
            }

        } else if (sensorState.moduleAddress == 0x70) {
            // SHTC3
            #ifdef LIGHTGATEWAY_PLUS_1_0
                if (shtc3.begin()) {
                    sensorState.moduleType = 5;
                    Serial.println("[WX] ✓ SHTC3 initialized successfully");
                    success = true;
                }
            #endif
        }

        if (success) {
            sensorState.isInitialized = true;
            sensorState.isHealthy = true;
            sensorState.consecutiveFailures = 0;
            sensorState.lastSuccessfulRead = millis();
            sensorState.lastError = "";
        } else {
            sensorState.lastError = "Sensor init failed (timeout or wrong type)";
            Serial.printf("[WX] ✗ Failed to initialize sensor (took %lu ms)\n",
                         millis() - startTime);
        }

        return success;
    }

    /**
     * @brief Hauptinitialisierung mit Fallback
     */
    void setup() {
        if (!Config.wxsensor.active) {
            Serial.println("[WX] Weather sensor disabled in config");
            return;
        }

        Serial.println("[WX] ========== Weather Sensor Setup ==========");

        // Phase 1: Scan I2C bus with timeout
        bool sensorFound = scanForSensorWithTimeout(I2C_SCAN_TIMEOUT_MS);

        if (!sensorFound) {
            Serial.println("[WX] ✗ No weather sensor found");
            Serial.println("[WX] ⚠️  System will continue WITHOUT weather data");
            displayShow("WARNING", "", "Weather sensor not found", "System continues...", 2000);
            return;
        }

        // Phase 2: Initialize sensor with timeout
        bool initSuccess = initializeSensorWithTimeout();

        if (!initSuccess) {
            Serial.println("[WX] ✗ Sensor initialization failed");
            Serial.println("[WX] ⚠️  System will continue WITHOUT weather data");
            Serial.println("[WX] ⚠️  Will retry in 5 minutes");
            displayShow("WARNING", "", "Sensor init failed", "Will retry later...", 2000);
            sensorState.lastRecoveryAttempt = millis();
            return;
        }

        Serial.println("[WX] ========== Setup Complete ==========");
        Serial.printf("[WX] Sensor Type: ");
        switch(sensorState.moduleType) {
            case 1: Serial.println("BME280 (Temp/Hum/Press)"); break;
            case 2: Serial.println("BMP280 (Temp/Press)"); break;
            case 3: Serial.println("BME680 (Temp/Hum/Press/Gas)"); break;
            case 4: Serial.println("Si7021 (Temp/Hum)"); break;
            case 5: Serial.println("SHTC3 (Temp/Hum)"); break;
        }
    }

    /**
     * @brief Versucht Sensor wiederherzustellen
     */
    void attemptRecovery() {
        Serial.println("[WX] ========== Recovery Attempt ==========");

        // Reset state
        sensorState.isInitialized = false;
        sensorState.isHealthy = false;
        sensorState.moduleType = 0;
        sensorState.moduleAddress = 0x00;

        // Re-scan and re-initialize
        if (scanForSensorWithTimeout(I2C_SCAN_TIMEOUT_MS)) {
            if (initializeSensorWithTimeout()) {
                Serial.println("[WX] ✓ Recovery successful!");
                sensorState.consecutiveFailures = 0;
                displayShow("INFO", "", "Weather sensor", "recovered!", 2000);
            } else {
                Serial.println("[WX] ✗ Recovery failed");
            }
        }

        sensorState.lastRecoveryAttempt = millis();
    }

    /**
     * @brief Überprüft Sensor-Gesundheit und triggert Recovery
     */
    void checkSensorHealth() {
        if (!Config.wxsensor.active) return;

        uint32_t now = millis();

        // Periodic health check
        if (now - sensorState.lastHealthCheck < SENSOR_HEALTH_CHECK_INTERVAL) {
            return;
        }
        sensorState.lastHealthCheck = now;

        // Check if we should attempt recovery
        bool shouldRecover = false;

        if (!sensorState.isInitialized) {
            // Never initialized - try periodically
            if (now - sensorState.lastRecoveryAttempt > RECOVERY_ATTEMPT_INTERVAL) {
                Serial.println("[WX] Periodic recovery attempt (not initialized)");
                shouldRecover = true;
            }
        } else if (!sensorState.isHealthy) {
            // Initialized but unhealthy
            if (sensorState.consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                if (now - sensorState.lastRecoveryAttempt > RECOVERY_ATTEMPT_INTERVAL) {
                    Serial.printf("[WX] Attempting recovery after %u failures\n",
                                 sensorState.consecutiveFailures);
                    shouldRecover = true;
                }
            }
        } else {
            // Healthy - check if readings are stale
            if (now - sensorState.lastSuccessfulRead > 5 * 60 * 1000) {
                Serial.println("[WX] No successful reads for 5 minutes - recovery needed");
                sensorState.isHealthy = false;
                shouldRecover = true;
            }
        }

        if (shouldRecover) {
            attemptRecovery();
        }
    }

    // Original helper functions (unchanged)
    String generateTempString(const float sensorTemp) {
        String strTemp = String((int)sensorTemp);
        switch (strTemp.length()) {
            case 1: return "00" + strTemp;
            case 2: return "0" + strTemp;
            case 3: return strTemp;
            default: return "-999";
        }
    }

    String generateHumString(const float sensorHum) {
        String strHum = String((int)sensorHum);
        switch (strHum.length()) {
            case 1: return "0" + strHum;
            case 2: return strHum;
            case 3: return ((int)sensorHum == 100) ? "00" : "-99";
            default: return "-99";
        }
    }

    String generatePresString(const float sensorPres) {
        String strPress = String((int)sensorPres);
        String decPress = String(int((sensorPres - int(sensorPres)) * 10));
        switch (strPress.length()) {
            case 1: return "000" + strPress + decPress;
            case 2: return "00" + strPress + decPress;
            case 3: return "0" + strPress + decPress;
            case 4: return strPress + decPress;
            case 5: return strPress;
            default: return "-99999";
        }
    }

    float getAltitudeCorrection() {
        #ifdef HAS_GPS
            return Config.beacon.gpsActive ? gps.altitude.meters() : Config.wxsensor.heightCorrection;
        #else
            return Config.wxsensor.heightCorrection;
        #endif
    }

    /**
     * @brief Liest Sensor-Daten mit Timeout und Fehlerbehandlung
     */
    String readDataSensor() {
        // Check health and attempt recovery if needed
        checkSensorHealth();

        // If sensor not healthy, return placeholder
        if (!sensorState.isHealthy || !sensorState.isInitialized) {
            fifthLine = "WX: Not Available";
            return ".../...g...t...";
        }

        uint32_t startTime = millis();
        bool readSuccess = false;

        try {
            switch (sensorState.moduleType) {
                case 1: // BME280
                    if (bme280.takeForcedMeasurement()) {
                        newTemp = bme280.readTemperature();
                        newPress = (bme280.readPressure() / 100.0F);
                        newHum = bme280.readHumidity();
                        readSuccess = true;
                    }
                    break;

                case 2: // BMP280
                    if (bmp280.takeForcedMeasurement()) {
                        newTemp = bmp280.readTemperature();
                        newPress = (bmp280.readPressure() / 100.0F);
                        newHum = 0;
                        readSuccess = true;
                    }
                    break;

                case 3: // BME680
                    #if !defined(HELTEC_V3) && !defined(HELTEC_V3_2)
                        if (bme680.performReading()) {
                            delay(50);
                            if (bme680.endReading()) {
                                newTemp = bme680.temperature;
                                newPress = (bme680.pressure / 100.0F);
                                newHum = bme680.humidity;
                                newGas = bme680.gas_resistance / 1000.0;
                                readSuccess = true;
                            }
                        }
                    #endif
                    break;

                case 4: // Si7021
                    newTemp = si7021.readTemperature();
                    newHum = si7021.readHumidity();
                    newPress = 0;
                    readSuccess = true;
                    break;

                case 5: // SHTC3
                    #ifdef LIGHTGATEWAY_PLUS_1_0
                        sensors_event_t humidity, temp;
                        shtc3.getEvent(&humidity, &temp);
                        newTemp = temp.temperature;
                        newHum = humidity.relative_humidity;
                        newPress = 0;
                        readSuccess = true;
                    #endif
                    break;
            }
        } catch (...) {
            Serial.println("[WX] ✗ Exception during sensor read");
            readSuccess = false;
        }

        // Check for timeout
        if (millis() - startTime > SENSOR_READ_TIMEOUT_MS) {
            Serial.println("[WX] ✗ Sensor read timeout");
            readSuccess = false;
        }

        // Validate data
        if (readSuccess && !isnan(newTemp) && !isnan(newHum) && !isnan(newPress)) {
            // Success - update statistics
            sensorState.totalReadSuccess++;
            sensorState.consecutiveFailures = 0;
            sensorState.lastSuccessfulRead = millis();
            sensorState.isHealthy = true;

            // Generate output strings
            String tempStr = generateTempString(((newTemp + Config.wxsensor.temperatureCorrection) * 1.8) + 32);

            String humStr;
            if (sensorState.moduleType == 1 || sensorState.moduleType == 3 ||
                sensorState.moduleType == 4 || sensorState.moduleType == 5) {
                humStr = generateHumString(newHum);
            } else {
                humStr = "..";
            }

            String presStr = (sensorState.moduleType == 4 || sensorState.moduleType == 5)
                ? "....."
                : generatePresString(newPress + getAltitudeCorrection() / CORRECTION_FACTOR);

            // Display line
            fifthLine = "BME-> ";
            fifthLine += String(int(newTemp + Config.wxsensor.temperatureCorrection));
            fifthLine += "C ";
            fifthLine += humStr;
            fifthLine += "% ";
            fifthLine += presStr.substring(0,4);
            fifthLine += "hPa";

            // APRS payload
            String wxPayload = ".../...g...t";
            wxPayload += tempStr;
            wxPayload += "h";
            wxPayload += humStr;
            wxPayload += "b";
            wxPayload += presStr;

            if (sensorState.moduleType == 3) {
                wxPayload += "Gas: ";
                wxPayload += String(newGas);
                wxPayload += "Kohms";
            }

            return wxPayload;

        } else {
            // Failure
            sensorState.totalReadFailures++;
            sensorState.consecutiveFailures++;
            sensorState.lastError = "Read failed or NaN data";

            Serial.printf("[WX] ✗ Sensor read failed (consecutive: %u)\n",
                         sensorState.consecutiveFailures);

            if (sensorState.consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                Serial.println("[WX] ⚠️  Too many failures - marking sensor unhealthy");
                sensorState.isHealthy = false;
            }

            fifthLine = "WX: Read Error";
            return ".../...g...t...";
        }
    }

    /**
     * @brief Gibt Sensor-Status für Diagnostik zurück
     */
    String getSensorStatusJSON() {
        String json = "{";
        json += "\"active\":" + String(Config.wxsensor.active ? "true" : "false") + ",";
        json += "\"initialized\":" + String(sensorState.isInitialized ? "true" : "false") + ",";
        json += "\"healthy\":" + String(sensorState.isHealthy ? "true" : "false") + ",";
        json += "\"type\":" + String(sensorState.moduleType) + ",";
        json += "\"address\":\"0x" + String(sensorState.moduleAddress, HEX) + "\",";
        json += "\"failures\":" + String(sensorState.consecutiveFailures) + ",";
        json += "\"success_total\":" + String(sensorState.totalReadSuccess) + ",";
        json += "\"failures_total\":" + String(sensorState.totalReadFailures) + ",";
        json += "\"last_success\":" + String((millis() - sensorState.lastSuccessfulRead) / 1000) + ",";
        json += "\"last_error\":\"" + sensorState.lastError + "\"";
        json += "}";
        return json;
    }

    /**
     * @brief Manueller Reset des Sensors (für Remote-Management)
     */
    void resetSensor() {
        Serial.println("[WX] Manual sensor reset triggered");
        sensorState.consecutiveFailures = MAX_CONSECUTIVE_FAILURES;
        sensorState.isHealthy = false;
        attemptRecovery();
    }

}
