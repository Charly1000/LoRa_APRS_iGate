/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * Weather Sensor Test Tool
 * Standalone test program to verify sensor functionality
 *
 * Usage:
 *   1. Upload this to ESP32
 *   2. Open Serial Monitor (115200 baud)
 *   3. Tests all sensor functions with detailed output
 *
 * This helps diagnose sensor issues before integrating into main firmware
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME680.h>
#include "Adafruit_Si7021.h"

// Test Configuration
#define TEST_DURATION_MS        30000     // Run tests for 30 seconds
#define TEST_READ_INTERVAL_MS   2000      // Read sensor every 2 seconds

// Sensor objects
Adafruit_BME280 bme280;
Adafruit_BMP280 bmp280;
Adafruit_BME680 bme680;
Adafruit_Si7021 si7021;

uint8_t sensorAddress = 0x00;
int sensorType = 0;  // 0=none, 1=BME280, 2=BMP280, 3=BME680, 4=Si7021

/**
 * @brief Scan I2C bus and find all devices
 */
void scanI2CBus() {
    Serial.println("\n========== I2C Bus Scan ==========");
    uint8_t count = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            Serial.printf("Device found at 0x%02X", addr);

            // Identify known devices
            switch (addr) {
                case 0x3C:
                case 0x3D:
                    Serial.print(" (OLED Display)");
                    break;
                case 0x40:
                    Serial.print(" (Si7021 Temp/Hum Sensor) ★");
                    sensorAddress = addr;
                    break;
                case 0x70:
                    Serial.print(" (SHTC3 Temp/Hum Sensor) ★");
                    sensorAddress = addr;
                    break;
                case 0x76:
                case 0x77:
                    Serial.print(" (BME280/BMP280/BME680 Sensor) ★");
                    sensorAddress = addr;
                    break;
                default:
                    Serial.print(" (Unknown)");
                    break;
            }
            Serial.println();
            count++;
        }
    }

    Serial.printf("Found %u device(s)\n", count);
    Serial.println("==================================\n");
}

/**
 * @brief Try to initialize sensor at found address
 */
bool initializeSensor() {
    if (sensorAddress == 0x00) {
        Serial.println("[ERROR] No sensor address found!");
        return false;
    }

    Serial.printf("\nTrying to initialize sensor at 0x%02X...\n", sensorAddress);

    // Try BME280 / BMP280 / BME680
    if (sensorAddress == 0x76 || sensorAddress == 0x77) {
        // Try BME280 first
        if (bme280.begin(sensorAddress)) {
            Serial.println("✓ BME280 initialized successfully!");
            sensorType = 1;
            bme280.setSampling(Adafruit_BME280::MODE_FORCED,
                        Adafruit_BME280::SAMPLING_X1,
                        Adafruit_BME280::SAMPLING_X1,
                        Adafruit_BME280::SAMPLING_X1,
                        Adafruit_BME280::FILTER_OFF);
            return true;
        }
        delay(100);

        // Try BME680
        if (bme680.begin(sensorAddress)) {
            Serial.println("✓ BME680 initialized successfully!");
            sensorType = 3;
            bme680.setTemperatureOversampling(BME680_OS_1X);
            bme680.setHumidityOversampling(BME680_OS_1X);
            bme680.setPressureOversampling(BME680_OS_1X);
            bme680.setIIRFilterSize(BME680_FILTER_SIZE_0);
            return true;
        }
        delay(100);

        // Try BMP280
        if (bmp280.begin(sensorAddress)) {
            Serial.println("✓ BMP280 initialized successfully!");
            sensorType = 2;
            bmp280.setSampling(Adafruit_BMP280::MODE_FORCED,
                        Adafruit_BMP280::SAMPLING_X1,
                        Adafruit_BMP280::SAMPLING_X1,
                        Adafruit_BMP280::FILTER_OFF);
            return true;
        }

        Serial.println("✗ Failed to initialize BME280/BMP280/BME680");
        return false;
    }

    // Try Si7021
    if (sensorAddress == 0x40) {
        if (si7021.begin()) {
            Serial.println("✓ Si7021 initialized successfully!");
            sensorType = 4;
            return true;
        }
        Serial.println("✗ Failed to initialize Si7021");
        return false;
    }

    Serial.println("✗ Unknown sensor type");
    return false;
}

/**
 * @brief Read and display sensor data
 */
void testSensorRead() {
    Serial.println("\n---------- Sensor Reading ----------");

    float temp = NAN, hum = NAN, press = NAN, gas = NAN;
    uint32_t startTime = millis();

    switch (sensorType) {
        case 1: // BME280
            if (bme280.takeForcedMeasurement()) {
                temp = bme280.readTemperature();
                hum = bme280.readHumidity();
                press = bme280.readPressure() / 100.0F;
            }
            break;

        case 2: // BMP280
            if (bmp280.takeForcedMeasurement()) {
                temp = bmp280.readTemperature();
                press = bmp280.readPressure() / 100.0F;
            }
            break;

        case 3: // BME680
            if (bme680.performReading()) {
                temp = bme680.temperature;
                hum = bme680.humidity;
                press = bme680.pressure / 100.0F;
                gas = bme680.gas_resistance / 1000.0F;
            }
            break;

        case 4: // Si7021
            temp = si7021.readTemperature();
            hum = si7021.readHumidity();
            break;

        default:
            Serial.println("✗ No sensor initialized");
            return;
    }

    uint32_t readTime = millis() - startTime;

    // Display results
    Serial.printf("Read Time: %lu ms\n", readTime);

    if (!isnan(temp)) {
        Serial.printf("Temperature: %.2f °C (%.2f °F)\n", temp, (temp * 1.8) + 32);
    } else {
        Serial.println("Temperature: ERROR (NaN)");
    }

    if (!isnan(hum)) {
        Serial.printf("Humidity: %.1f %%\n", hum);
    } else if (sensorType == 1 || sensorType == 3 || sensorType == 4) {
        Serial.println("Humidity: ERROR (NaN)");
    } else {
        Serial.println("Humidity: N/A (sensor has no humidity)");
    }

    if (!isnan(press)) {
        Serial.printf("Pressure: %.2f hPa\n", press);
    } else if (sensorType == 1 || sensorType == 2 || sensorType == 3) {
        Serial.println("Pressure: ERROR (NaN)");
    } else {
        Serial.println("Pressure: N/A (sensor has no pressure)");
    }

    if (!isnan(gas) && sensorType == 3) {
        Serial.printf("Gas Resistance: %.2f kOhms\n", gas);
    }

    // Validation
    bool valid = !isnan(temp);
    if (sensorType == 1 || sensorType == 3 || sensorType == 4) {
        valid &= !isnan(hum);
    }
    if (sensorType == 1 || sensorType == 2 || sensorType == 3) {
        valid &= !isnan(press);
    }

    if (valid) {
        Serial.println("Status: ✓ VALID");
    } else {
        Serial.println("Status: ✗ INVALID (NaN detected)");
    }

    Serial.println("------------------------------------");
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n\n");
    Serial.println("╔═══════════════════════════════════════════╗");
    Serial.println("║   LoRa APRS iGate - WX Sensor Test Tool  ║");
    Serial.println("║        Weather Sensor Diagnostics         ║");
    Serial.println("╚═══════════════════════════════════════════╝");
    Serial.println();

    // Initialize I2C
    Serial.println("Initializing I2C bus...");
    Wire.begin();
    delay(100);
    Serial.println("✓ I2C initialized\n");

    // Scan for devices
    scanI2CBus();

    // Initialize sensor
    if (sensorAddress != 0x00) {
        if (initializeSensor()) {
            Serial.println("\n✓ Sensor ready for testing");
            Serial.printf("   Type: ");
            switch (sensorType) {
                case 1: Serial.println("BME280 (Temp/Hum/Press)"); break;
                case 2: Serial.println("BMP280 (Temp/Press)"); break;
                case 3: Serial.println("BME680 (Temp/Hum/Press/Gas)"); break;
                case 4: Serial.println("Si7021 (Temp/Hum)"); break;
            }
            Serial.printf("   Address: 0x%02X\n", sensorAddress);
            Serial.println();
            Serial.printf("Starting %d-second continuous test...\n", TEST_DURATION_MS / 1000);
        } else {
            Serial.println("\n✗ Sensor initialization FAILED");
            Serial.println("   Check:");
            Serial.println("   - Wiring (SDA, SCL, VCC, GND)");
            Serial.println("   - I2C address");
            Serial.println("   - Power supply");
            Serial.println("   - Sensor is not defective");
        }
    } else {
        Serial.println("✗ No weather sensor found!");
        Serial.println("   Check:");
        Serial.println("   - Sensor is connected");
        Serial.println("   - I2C wiring");
        Serial.println("   - Correct I2C bus (Wire vs Wire1)");
    }
}

void loop() {
    static uint32_t lastRead = 0;
    static uint32_t testStart = millis();
    static uint32_t readCount = 0;
    static uint32_t failCount = 0;

    // Check if test duration exceeded
    if (millis() - testStart > TEST_DURATION_MS) {
        Serial.println("\n\n========== TEST SUMMARY ==========");
        Serial.printf("Total Reads: %lu\n", readCount);
        Serial.printf("Failed Reads: %lu\n", failCount);
        Serial.printf("Success Rate: %.1f%%\n",
                     readCount > 0 ? ((readCount - failCount) * 100.0 / readCount) : 0.0);
        Serial.println("==================================");
        Serial.println("\nTest complete. Reset to run again.");

        while (1) {
            delay(1000);
        }
    }

    // Periodic sensor read
    if (millis() - lastRead > TEST_READ_INTERVAL_MS) {
        if (sensorType != 0) {
            testSensorRead();
            readCount++;
        }
        lastRead = millis();
    }
}
