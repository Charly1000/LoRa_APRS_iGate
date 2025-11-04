/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * System Activity Tracking - Thread-Safety for Critical Operations
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CRITICAL: Prevents race conditions between:
 * - LoRa TX/RX operations
 * - APRS-IS uploads
 * - I2C operations (Display, Sensor)
 * - System maintenance (Auto-Recovery, OTA)
 */

#ifndef SYSTEM_ACTIVITY_H_
#define SYSTEM_ACTIVITY_H_

#include <Arduino.h>

/**
 * @brief Global Activity Flags - Thread-Safety Tracking
 *
 * These flags prevent concurrent access to shared resources:
 * - LoRa Radio (only one operation at a time)
 * - I2C Bus (Display + Sensor share bus)
 * - Network (APRS-IS uploads)
 * - Critical system operations
 *
 * Usage:
 *   Before ANY operation that might conflict, check if safe:
 *   if (SystemActivity::isSafeForMaintenance()) {
 *       // Safe to do maintenance/recovery
 *   }
 */
namespace SystemActivity {

    // ===== Activity Flags =====

    /**
     * @brief LoRa Radio Transmission in progress
     * Set by: LoRa_Utils::sendNewPacket()
     * Duration: 100-200ms (depends on spreading factor)
     * Conflicts with: I2C operations (CPU load), Recovery
     */
    extern volatile bool isLoRaTxActive;

    /**
     * @brief LoRa Radio Reception in progress
     * Set by: LoRa_Utils::receivePacket()
     * Duration: Variable (until packet complete)
     * Conflicts with: Heavy CPU operations
     */
    extern volatile bool isLoRaRxActive;

    /**
     * @brief APRS-IS TCP Upload in progress
     * Set by: APRS_IS_Utils::upload()
     * Duration: 50-500ms (network dependent)
     * Conflicts with: WiFi operations, Heavy CPU load
     */
    extern volatile bool isAPRSISUploadActive;

    /**
     * @brief I2C Bus operation in progress
     * Set by: Display updates, Sensor reads
     * Duration: 10-100ms
     * Conflicts with: Other I2C operations
     */
    extern volatile bool isI2CActive;

    /**
     * @brief Critical operation in progress (generic)
     * Set by: Config writes, OTA updates, etc.
     * Duration: Variable
     * Conflicts with: Everything
     */
    extern volatile bool isCriticalOperation;

    /**
     * @brief Packet Buffer has pending transmissions
     * Checked by: Recovery routines
     * Conflicts with: System maintenance
     */
    extern volatile uint16_t pendingPacketCount;

    // ===== Timestamps for Rate-Limiting =====

    /**
     * @brief Last LoRa TX timestamp
     * Used for: TX rate limiting (3s between packets)
     */
    extern volatile uint32_t lastLoRaTxTime;

    /**
     * @brief Last LoRa RX timestamp
     * Used for: RX monitoring, Timeout detection
     */
    extern volatile uint32_t lastLoRaRxTime;

    // ===== Helper Functions =====

    /**
     * @brief Check if system is safe for maintenance operations
     *
     * Returns true only if:
     * - No LoRa TX/RX active
     * - No APRS-IS upload active
     * - No I2C operations active
     * - No packets waiting in buffer
     * - No critical operations running
     *
     * Use this before:
     * - Sensor auto-recovery
     * - Config file writes
     * - Display updates (optional)
     * - Any non-urgent maintenance
     *
     * @return true if safe to perform maintenance
     */
    inline bool isSafeForMaintenance() {
        if (isLoRaTxActive) return false;
        if (isLoRaRxActive) return false;
        if (isAPRSISUploadActive) return false;
        if (isCriticalOperation) return false;
        if (pendingPacketCount > 0) return false;
        return true;
    }

    /**
     * @brief Check if I2C bus is safe to use
     *
     * Use before sensor reads or display updates
     *
     * @return true if I2C bus available
     */
    inline bool isSafeForI2C() {
        if (isI2CActive) return false;
        if (isCriticalOperation) return false;
        return true;
    }

    /**
     * @brief Check if LoRa is idle and safe for TX
     *
     * Enforces 3-second gap between transmissions
     *
     * @return true if safe to transmit
     */
    inline bool isSafeForLoRaTx() {
        if (isLoRaTxActive) return false;
        if (isLoRaRxActive) return false;
        if (millis() - lastLoRaTxTime < 3000) return false;
        if (millis() - lastLoRaRxTime < 3000) return false;
        return true;
    }

    /**
     * @brief Reset all activity flags (for emergency reset)
     *
     * WARNING: Only use this if system is truly stuck!
     * Normal operations should manage their own flags.
     */
    inline void resetAllFlags() {
        isLoRaTxActive = false;
        isLoRaRxActive = false;
        isAPRSISUploadActive = false;
        isI2CActive = false;
        isCriticalOperation = false;
        pendingPacketCount = 0;
    }

    /**
     * @brief Get human-readable status string
     *
     * For debugging and logging
     */
    inline String getStatusString() {
        String status = "Activity: ";
        if (isLoRaTxActive) status += "LoRaTX ";
        if (isLoRaRxActive) status += "LoRaRX ";
        if (isAPRSISUploadActive) status += "APRSIS ";
        if (isI2CActive) status += "I2C ";
        if (isCriticalOperation) status += "CRITICAL ";
        if (pendingPacketCount > 0) status += "Packets:" + String(pendingPacketCount) + " ";
        if (isSafeForMaintenance()) status += "[IDLE]";
        return status;
    }

}

/**
 * @brief RAII Guard for automatic flag management
 *
 * Automatically sets flag on construction, clears on destruction.
 * Prevents forgetting to clear flags.
 *
 * Usage:
 *   void sendPacket() {
 *       ActivityGuard guard(SystemActivity::isLoRaTxActive);
 *       // ... LoRa TX code ...
 *       // Flag automatically cleared when guard goes out of scope
 *   }
 */
class ActivityGuard {
private:
    volatile bool& flag;
    bool wasSet;

public:
    ActivityGuard(volatile bool& f) : flag(f), wasSet(f) {
        flag = true;
    }

    ~ActivityGuard() {
        if (!wasSet) {  // Only clear if we set it
            flag = false;
        }
    }

    // Prevent copying
    ActivityGuard(const ActivityGuard&) = delete;
    ActivityGuard& operator=(const ActivityGuard&) = delete;
};

#endif
