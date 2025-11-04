/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * Air Quality Extension for BME680 Sensor
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FEATURE: Adds Air Quality percentage to APRS beacon comment
 * - 100% = Best air quality
 * - 1% = Worst air quality
 * - Based on BME680 gas resistance (VOC sensor)
 */

#ifndef WX_UTILS_AIRQUALITY_H_
#define WX_UTILS_AIRQUALITY_H_

#include <Arduino.h>

namespace AirQuality {

    /**
     * @brief Air Quality Configuration
     *
     * BME680 Gas Resistance typical ranges:
     * - Excellent air: 100-500 kOhm (fresh outdoor air)
     * - Good air: 50-100 kOhm (normal indoor air)
     * - Moderate air: 20-50 kOhm (slightly polluted)
     * - Poor air: 5-20 kOhm (polluted, cooking, etc.)
     * - Bad air: 1-5 kOhm (heavily polluted)
     *
     * These values can be calibrated based on your environment.
     */
    struct Calibration {
        float excellentThreshold;   // kOhm for 100% quality
        float goodThreshold;         // kOhm for 80% quality
        float moderateThreshold;     // kOhm for 60% quality
        float poorThreshold;         // kOhm for 40% quality
        float badThreshold;          // kOhm for 20% quality
        float worstThreshold;        // kOhm for 1% quality
    };

    // Default calibration (can be adjusted via config)
    const Calibration DEFAULT_CALIBRATION = {
        .excellentThreshold = 150.0,  // > 150 kOhm = 100%
        .goodThreshold = 80.0,        // > 80 kOhm = 80%
        .moderateThreshold = 40.0,    // > 40 kOhm = 60%
        .poorThreshold = 20.0,        // > 20 kOhm = 40%
        .badThreshold = 10.0,         // > 10 kOhm = 20%
        .worstThreshold = 2.0         // > 2 kOhm = 1%
    };

    /**
     * @brief Convert gas resistance to air quality percentage
     *
     * @param gasResistanceKOhm Gas resistance in kilo-ohms (from BME680)
     * @param cal Calibration settings (optional, uses default if nullptr)
     * @return Air quality percentage (1-100%)
     *         100% = best air quality
     *         1% = worst air quality
     *
     * Algorithm:
     * - Uses piecewise linear interpolation between thresholds
     * - Clamps values to 1-100% range
     * - Higher resistance = better air quality
     *
     * Example:
     *   gasResistanceKOhm = 90.0 kOhm
     *   Result: ~82% (zwischen good und excellent)
     */
    inline uint8_t calculateAirQualityPercent(float gasResistanceKOhm,
                                               const Calibration* cal = nullptr) {
        // Use default calibration if not provided
        const Calibration& c = cal ? *cal : DEFAULT_CALIBRATION;

        // Sanity check
        if (gasResistanceKOhm <= 0 || isnan(gasResistanceKOhm)) {
            return 0;  // Invalid reading
        }

        // Piecewise linear interpolation
        uint8_t quality = 0;

        if (gasResistanceKOhm >= c.excellentThreshold) {
            // Excellent: 100%
            quality = 100;

        } else if (gasResistanceKOhm >= c.goodThreshold) {
            // Between good (80%) and excellent (100%)
            float range = c.excellentThreshold - c.goodThreshold;
            float position = gasResistanceKOhm - c.goodThreshold;
            quality = 80 + (position / range) * 20;

        } else if (gasResistanceKOhm >= c.moderateThreshold) {
            // Between moderate (60%) and good (80%)
            float range = c.goodThreshold - c.moderateThreshold;
            float position = gasResistanceKOhm - c.moderateThreshold;
            quality = 60 + (position / range) * 20;

        } else if (gasResistanceKOhm >= c.poorThreshold) {
            // Between poor (40%) and moderate (60%)
            float range = c.moderateThreshold - c.poorThreshold;
            float position = gasResistanceKOhm - c.poorThreshold;
            quality = 40 + (position / range) * 20;

        } else if (gasResistanceKOhm >= c.badThreshold) {
            // Between bad (20%) and poor (40%)
            float range = c.poorThreshold - c.badThreshold;
            float position = gasResistanceKOhm - c.badThreshold;
            quality = 20 + (position / range) * 20;

        } else if (gasResistanceKOhm >= c.worstThreshold) {
            // Between worst (1%) and bad (20%)
            float range = c.badThreshold - c.worstThreshold;
            float position = gasResistanceKOhm - c.worstThreshold;
            quality = 1 + (position / range) * 19;

        } else {
            // Below worst threshold: 1%
            quality = 1;
        }

        // Clamp to valid range
        if (quality > 100) quality = 100;
        if (quality < 1) quality = 1;

        return quality;
    }

    /**
     * @brief Get human-readable air quality description
     *
     * @param percentage Air quality percentage (1-100)
     * @return String description
     */
    inline String getQualityDescription(uint8_t percentage) {
        if (percentage >= 90) return "Excellent";
        if (percentage >= 70) return "Good";
        if (percentage >= 50) return "Moderate";
        if (percentage >= 30) return "Poor";
        if (percentage >= 10) return "Bad";
        return "Hazardous";
    }

    /**
     * @brief Format air quality for APRS beacon comment
     *
     * @param gasResistanceKOhm Gas resistance from BME680
     * @param includeDescription Include text description (default: false)
     * @return Formatted string for APRS comment, e.g. "AQ:95%"
     *
     * Format options:
     * - Short: "AQ:95%"
     * - Long: "AQ:95%(Excellent)"
     */
    inline String formatForAPRS(float gasResistanceKOhm, bool includeDescription = false) {
        uint8_t quality = calculateAirQualityPercent(gasResistanceKOhm);

        if (quality == 0) {
            return "";  // Invalid reading - don't add to beacon
        }

        String result = "AQ:";
        result += String(quality);
        result += "%";

        if (includeDescription) {
            result += "(";
            result += getQualityDescription(quality);
            result += ")";
        }

        return result;
    }

    /**
     * @brief Format air quality for display (longer format)
     *
     * @param gasResistanceKOhm Gas resistance from BME680
     * @return Formatted string for display, e.g. "AQ:95% Good"
     */
    inline String formatForDisplay(float gasResistanceKOhm) {
        uint8_t quality = calculateAirQualityPercent(gasResistanceKOhm);

        if (quality == 0) {
            return "AQ: N/A";
        }

        String result = "AQ:";
        result += String(quality);
        result += "% ";
        result += getQualityDescription(quality);

        return result;
    }

    /**
     * @brief Advanced: Calculate baseline-corrected air quality
     *
     * BME680 gas resistance varies with temperature/humidity.
     * This function applies a simple baseline correction.
     *
     * @param gasResistanceKOhm Current gas resistance
     * @param baselineKOhm Baseline resistance (measured in clean air)
     * @return Corrected percentage
     *
     * Baseline Calibration:
     * 1. Expose sensor to fresh outdoor air for 30 minutes
     * 2. Record average gas resistance = baseline
     * 3. Store in config
     * 4. Use this function for more accurate readings
     */
    inline uint8_t calculateWithBaseline(float gasResistanceKOhm, float baselineKOhm) {
        if (baselineKOhm <= 0 || gasResistanceKOhm <= 0) {
            return calculateAirQualityPercent(gasResistanceKOhm);
        }

        // Normalize to baseline
        float ratio = gasResistanceKOhm / baselineKOhm;

        // Adjust thresholds proportionally
        Calibration adjusted = DEFAULT_CALIBRATION;
        adjusted.excellentThreshold = baselineKOhm * 1.0;   // 100% of baseline
        adjusted.goodThreshold = baselineKOhm * 0.6;        // 60% of baseline
        adjusted.moderateThreshold = baselineKOhm * 0.3;    // 30% of baseline
        adjusted.poorThreshold = baselineKOhm * 0.15;       // 15% of baseline
        adjusted.badThreshold = baselineKOhm * 0.08;        // 8% of baseline
        adjusted.worstThreshold = baselineKOhm * 0.02;      // 2% of baseline

        return calculateAirQualityPercent(gasResistanceKOhm, &adjusted);
    }

    /**
     * @brief Statistics tracking for air quality
     */
    struct Statistics {
        uint8_t current;
        uint8_t min;
        uint8_t max;
        uint8_t avg;
        uint32_t samples;
        uint32_t lastUpdate;

        Statistics() : current(0), min(100), max(0), avg(0), samples(0), lastUpdate(0) {}

        void update(uint8_t quality) {
            current = quality;
            if (quality < min) min = quality;
            if (quality > max) max = quality;

            // Rolling average
            if (samples == 0) {
                avg = quality;
            } else {
                avg = (avg * samples + quality) / (samples + 1);
            }

            samples++;
            lastUpdate = millis();
        }

        String toJSON() const {
            String json = "{";
            json += "\"current\":" + String(current) + ",";
            json += "\"min\":" + String(min) + ",";
            json += "\"max\":" + String(max) + ",";
            json += "\"avg\":" + String(avg) + ",";
            json += "\"samples\":" + String(samples);
            json += "}";
            return json;
        }
    };

}

#endif
