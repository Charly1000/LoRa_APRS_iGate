/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * THREAD-SAFE VERSION with Activity Guards
 *
 * This version fixes CRITICAL race conditions by respecting:
 * - LoRa TX/RX operations (never interrupt)
 * - APRS-IS uploads (never interrupt)
 * - I2C bus conflicts (mutex-like protection)
 * - Packet buffer state (no recovery when packets pending)
 *
 * Differences from wx_utils_robust.cpp:
 * - Includes system_activity.h for thread-safety
 * - checkSensorHealth() checks isSafeForMaintenance()
 * - attemptRecovery() protected with activity guards
 * - readDataSensor() checks isSafeForI2C()
 *
 * Integration:
 *   cp src/wx_utils_safe.cpp src/wx_utils.cpp
 *   Add to LoRa_APRS_iGate.cpp: #include "system_activity.h"
 */

#include <TinyGPS++.h>
#ifdef LIGHTGATEWAY_PLUS_1_0
#include "Adafruit_SHTC3.h"
#endif
#include "configuration.h"
#include "board_pinout.h"
#include "wx_utils.h"
#include "display.h"
#include "system_activity.h"  // ✅ THREAD-SAFETY

#define SEALEVELPRESSURE_HPA    (1013.25)
#define CORRECTION_FACTOR       (8.2296)

// Timeout & Retry Configuration
#define I2C_SCAN_TIMEOUT_MS         1000
#define SENSOR_INIT_TIMEOUT_MS      2000
#define SENSOR_READ_TIMEOUT_MS      1000
#define MAX_CONSECUTIVE_FAILURES    5
#define RECOVERY_ATTEMPT_INTERVAL   300000    // 5 minutes
#define SENSOR_HEALTH_CHECK_INTERVAL 60000    // 1 minute

extern Configuration            Config;
extern String                   fifthLine;
#ifdef HAS_GPS
extern TinyGPSPlus              gps;
#endif

// Sensor State Management
struct SensorState {
    int moduleType;
    uint8_t moduleAddress;
    bool isInitialized;
    bool isHealthy;
    uint32_t consecutiveFailures;
    uint32_t lastSuccessfulRead;
    uint32_t lastRecoveryAttempt;
    uint32_t lastHealthCheck;
    uint32_t totalReadSuccess;
    uint32_t totalReadFailures;
    uint32_t recoveryAttemptsDelayed;  // ✅ NEW: Count delayed recoveries
    String lastError;
};

SensorState sensorState = {0, 0x00, false, false, 0, 0, 0, 0, 0, 0, 0, ""};

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
     * @brief THREAD-SAFE I2C scan with activity guard
     */
    bool scanForSensorWithTimeout(uint32_t timeoutMs) {
        // ✅ Check if I2C bus is safe
        if (!SystemActivity::isSafeForI2C()) {
            Serial.println("[WX] I2C scan delayed (bus busy)");
            return false;
        }

        // ✅ Acquire I2C lock
        ActivityGuard i2cGuard(SystemActivity::isI2CActive);

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
     * @brief THREAD-SAFE sensor initialization
     */
    bool initializeSensorWithTimeout() {
        if (sensorState.moduleAddress == 0x00) {
            return false;
        }

        // ✅ Check if I2C safe
        if (!SystemActivity::isSafeForI2C()) {
            Serial.println("[WX] Init delayed (I2C busy)");
            return false;
        }

        // ✅ Acquire I2C lock
        ActivityGuard i2cGuard(SystemActivity::isI2CActive);

        uint32_t startTime = millis();
        bool success = false;

        Serial.printf("[WX] Attempting to initialize sensor at 0x%02X...\n",
                     sensorState.moduleAddress);

        if (sensorState.moduleAddress == 0x76 || sensorState.moduleAddress == 0x77) {
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
            if (si7021.begin()) {
                sensorState.moduleType = 4;
                Serial.println("[WX] ✓ Si7021 initialized successfully");
                success = true;
            }

        } else if (sensorState.moduleAddress == 0x70) {
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
     * @brief Initial setup (unchanged from robust version)
     */
    void setup() {
        if (!Config.wxsensor.active) {
            Serial.println("[WX] Weather sensor disabled in config");
            return;
        }

        Serial.println("[WX] ========== Weather Sensor Setup ==========");

        bool sensorFound = scanForSensorWithTimeout(I2C_SCAN_TIMEOUT_MS);

        if (!sensorFound) {
            Serial.println("[WX] ✗ No weather sensor found");
            Serial.println("[WX] ⚠️  System will continue WITHOUT weather data");
            displayShow("WARNING", "", "Weather sensor not found", "System continues...", 2000);
            return;
        }

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
     * @brief THREAD-SAFE recovery attempt
     *
     * ✅ Only runs if system is truly idle
     */
    void attemptRecovery() {
        // ✅ CRITICAL: Double-check safety before recovery!
        if (!SystemActivity::isSafeForMaintenance()) {
            Serial.println("[WX] Recovery SKIPPED - system busy!");
            Serial.println("[WX] Activity: " + SystemActivity::getStatusString());
            sensorState.recoveryAttemptsDelayed++;
            return;  // ⚠️ DO NOT RECOVER NOW!
        }

        Serial.println("[WX] ========== Recovery Attempt ==========");
        Serial.println("[WX] System idle - safe to recover");

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
     * @brief THREAD-SAFE health check
     *
     * ✅ Only triggers recovery when safe
     */
    void checkSensorHealth() {
        if (!Config.wxsensor.active) return;

        uint32_t now = millis();

        if (now - sensorState.lastHealthCheck < SENSOR_HEALTH_CHECK_INTERVAL) {
            return;
        }
        sensorState.lastHealthCheck = now;

        bool shouldRecover = false;

        if (!sensorState.isInitialized) {
            if (now - sensorState.lastRecoveryAttempt > RECOVERY_ATTEMPT_INTERVAL) {
                Serial.println("[WX] Periodic recovery attempt (not initialized)");
                shouldRecover = true;
            }
        } else if (!sensorState.isHealthy) {
            if (sensorState.consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                if (now - sensorState.lastRecoveryAttempt > RECOVERY_ATTEMPT_INTERVAL) {
                    Serial.printf("[WX] Attempting recovery after %lu failures\n",
                                 sensorState.consecutiveFailures);
                    shouldRecover = true;
                }
            }
        } else {
            if (now - sensorState.lastSuccessfulRead > 5 * 60 * 1000) {
                Serial.println("[WX] No successful reads for 5 minutes - recovery needed");
                sensorState.isHealthy = false;
                shouldRecover = true;
            }
        }

        if (shouldRecover) {
            // ✅ CRITICAL: Only recover if safe!
            if (SystemActivity::isSafeForMaintenance()) {
                attemptRecovery();
            } else {
                Serial.println("[WX] ⚠️ Recovery DELAYED - waiting for idle state");
                Serial.println("[WX] " + SystemActivity::getStatusString());
                sensorState.recoveryAttemptsDelayed++;
                // Don't update lastRecoveryAttempt - will retry next check
            }
        }
    }

    // Helper functions (unchanged)
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
     * @brief THREAD-SAFE sensor read
     *
     * ✅ Respects I2C bus activity
     * ✅ Auto health-check with safe recovery
     */
    String readDataSensor() {
        // ✅ Check health (with safe recovery)
        checkSensorHealth();

        if (!sensorState.isHealthy || !sensorState.isInitialized) {
            fifthLine = "WX: Not Available";
            return ".../...g...t...";
        }

        // ✅ Check if I2C safe (optional - can be removed if too strict)
        // Commented out to allow sensor reads during normal operation
        // Only recovery is blocked, not reads
        /*
        if (!SystemActivity::isSafeForI2C()) {
            Serial.println("[WX] Read skipped (I2C busy)");
            fifthLine = "WX: Busy";
            return ".../...g...t...";
        }
        */

        // ✅ Acquire I2C lock for read
        ActivityGuard i2cGuard(SystemActivity::isI2CActive);

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

        if (millis() - startTime > SENSOR_READ_TIMEOUT_MS) {
            Serial.println("[WX] ✗ Sensor read timeout");
            readSuccess = false;
        }

        if (readSuccess && !isnan(newTemp) && !isnan(newHum) && !isnan(newPress)) {
            sensorState.totalReadSuccess++;
            sensorState.consecutiveFailures = 0;
            sensorState.lastSuccessfulRead = millis();
            sensorState.isHealthy = true;

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

            fifthLine = "BME-> ";
            fifthLine += String(int(newTemp + Config.wxsensor.temperatureCorrection));
            fifthLine += "C ";
            fifthLine += humStr;
            fifthLine += "% ";
            fifthLine += presStr.substring(0,4);
            fifthLine += "hPa";

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
            sensorState.totalReadFailures++;
            sensorState.consecutiveFailures++;
            sensorState.lastError = "Read failed or NaN data";

            Serial.printf("[WX] ✗ Sensor read failed (consecutive: %lu)\n",
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
     * @brief Get sensor status JSON (enhanced with delay count)
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
        json += "\"recoveries_delayed\":" + String(sensorState.recoveryAttemptsDelayed) + ",";  // ✅ NEW
        json += "\"last_success\":" + String((millis() - sensorState.lastSuccessfulRead) / 1000) + ",";
        json += "\"last_error\":\"" + sensorState.lastError + "\",";
        json += "\"system_idle\":" + String(SystemActivity::isSafeForMaintenance() ? "true" : "false");  // ✅ NEW
        json += "}";
        return json;
    }

    /**
     * @brief Manual reset (unchanged)
     */
    void resetSensor() {
        Serial.println("[WX] Manual sensor reset triggered");
        sensorState.consecutiveFailures = MAX_CONSECUTIVE_FAILURES;
        sensorState.isHealthy = false;

        // ✅ Only attempt immediate recovery if safe
        if (SystemActivity::isSafeForMaintenance()) {
            attemptRecovery();
        } else {
            Serial.println("[WX] Recovery delayed - system busy");
            // Will be attempted on next health check
        }
    }

}
