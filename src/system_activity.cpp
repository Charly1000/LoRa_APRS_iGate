/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * System Activity Tracking - Implementation
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "system_activity.h"

namespace SystemActivity {

    // Initialize all flags to false (idle state)
    volatile bool isLoRaTxActive = false;
    volatile bool isLoRaRxActive = false;
    volatile bool isAPRSISUploadActive = false;
    volatile bool isI2CActive = false;
    volatile bool isCriticalOperation = false;
    volatile uint16_t pendingPacketCount = 0;

    volatile uint32_t lastLoRaTxTime = 0;
    volatile uint32_t lastLoRaRxTime = 0;

}
