/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * Feature: Packet Statistics & Diagnostics
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PACKET_STATISTICS_H_
#define PACKET_STATISTICS_H_

#include <Arduino.h>
#include <map>

/**
 * @brief Erweiterte Packet-Statistiken für Monitoring und Diagnostik
 *
 * Diese Klasse sammelt detaillierte Statistiken über:
 * - Empfangene/gesendete Pakete
 * - RSSI/SNR Durchschnittswerte
 * - Packet-Typen-Verteilung
 * - Fehlerraten
 * - Stündliche/tägliche Statistiken
 *
 * Nutzen:
 * - Optimierung der Antennen-Position
 * - Netzwerk-Performance-Analyse
 * - Problem-Diagnostik
 * - Web-Dashboard mit Live-Daten
 */
class PacketStatistics {
public:
    struct CounterStats {
        uint32_t total;
        uint32_t lastHour;
        uint32_t lastDay;
        float avgPerHour;

        CounterStats() : total(0), lastHour(0), lastDay(0), avgPerHour(0.0) {}
    };

    struct SignalStats {
        int16_t minRSSI;
        int16_t maxRSSI;
        int16_t avgRSSI;
        float minSNR;
        float maxSNR;
        float avgSNR;
        uint32_t samples;

        SignalStats() : minRSSI(0), maxRSSI(-200), avgRSSI(0),
                       minSNR(0), maxSNR(-100), avgSNR(0), samples(0) {}

        void update(int16_t rssi, float snr) {
            if (samples == 0) {
                minRSSI = maxRSSI = avgRSSI = rssi;
                minSNR = maxSNR = avgSNR = snr;
            } else {
                if (rssi < minRSSI) minRSSI = rssi;
                if (rssi > maxRSSI) maxRSSI = rssi;
                if (snr < minSNR) minSNR = snr;
                if (snr > maxSNR) maxSNR = snr;

                // Rolling average
                avgRSSI = (avgRSSI * samples + rssi) / (samples + 1);
                avgSNR = (avgSNR * samples + snr) / (samples + 1);
            }
            samples++;
        }
    };

    struct StationStats {
        uint32_t packetCount;
        SignalStats signal;
        uint32_t lastSeen;

        StationStats() : packetCount(0), lastSeen(0) {}
    };

private:
    // Globale Zähler
    CounterStats rxPackets;
    CounterStats txPackets;
    CounterStats crcErrors;
    CounterStats duplicates;

    // Nach Typ
    uint32_t positionPackets;
    uint32_t messagePackets;
    uint32_t statusPackets;
    uint32_t objectPackets;
    uint32_t weatherPackets;
    uint32_t telemetryPackets;

    // Nach Route
    uint32_t loraToAprsIs;
    uint32_t aprsIsToLora;
    uint32_t digipeated;
    uint32_t mqttPublished;

    // Signal-Statistiken
    SignalStats globalSignal;

    // Per-Station Statistiken
    std::map<String, StationStats> stationStats;

    // Zeitstempel
    uint32_t startTime;
    uint32_t lastHourReset;
    uint32_t lastDayReset;

    // Beste/schlechteste Bedingungen
    struct BestWorst {
        String callsign;
        int16_t rssi;
        float snr;
        uint32_t timestamp;

        BestWorst() : rssi(0), snr(0), timestamp(0) {}
    };

    BestWorst bestSignal;
    BestWorst worstSignal;

public:
    PacketStatistics() :
        positionPackets(0), messagePackets(0), statusPackets(0),
        objectPackets(0), weatherPackets(0), telemetryPackets(0),
        loraToAprsIs(0), aprsIsToLora(0), digipeated(0), mqttPublished(0) {
        reset();
    }

    /**
     * @brief Reset aller Statistiken
     */
    void reset() {
        startTime = millis();
        lastHourReset = startTime;
        lastDayReset = startTime;
        worstSignal.rssi = 0;
        bestSignal.rssi = -200;
    }

    /**
     * @brief Registriere empfangenes Packet
     */
    void recordRxPacket(const String& sender, int16_t rssi, float snr,
                       const String& packetType = "") {
        incrementCounter(rxPackets);
        globalSignal.update(rssi, snr);

        // Station-spezifisch
        auto& stats = stationStats[sender];
        stats.packetCount++;
        stats.signal.update(rssi, snr);
        stats.lastSeen = millis();

        // Packet-Typ
        if (packetType == "position") positionPackets++;
        else if (packetType == "message") messagePackets++;
        else if (packetType == "status") statusPackets++;
        else if (packetType == "object") objectPackets++;
        else if (packetType == "weather") weatherPackets++;
        else if (packetType == "telemetry") telemetryPackets++;

        // Best/Worst tracking
        if (rssi > bestSignal.rssi) {
            bestSignal = {sender, rssi, snr, millis()};
        }
        if (rssi < worstSignal.rssi || worstSignal.rssi == 0) {
            worstSignal = {sender, rssi, snr, millis()};
        }

        checkTimeResets();
    }

    /**
     * @brief Registriere gesendetes Packet
     */
    void recordTxPacket() {
        incrementCounter(txPackets);
        checkTimeResets();
    }

    /**
     * @brief Registriere CRC-Fehler
     */
    void recordCRCError() {
        incrementCounter(crcErrors);
    }

    /**
     * @brief Registriere Duplikat
     */
    void recordDuplicate() {
        incrementCounter(duplicates);
    }

    /**
     * @brief Registriere Route-Typ
     */
    void recordRoute(const String& routeType) {
        if (routeType == "lora_to_aprsis") loraToAprsIs++;
        else if (routeType == "aprsis_to_lora") aprsIsToLora++;
        else if (routeType == "digipeated") digipeated++;
        else if (routeType == "mqtt") mqttPublished++;
    }

    /**
     * @brief Gibt JSON-Statistiken zurück (für Web-API)
     */
    String toJSON() const {
        String json = "{";

        // Allgemeine Stats
        json += "\"uptime\":" + String((millis() - startTime) / 1000) + ",";
        json += "\"rx_total\":" + String(rxPackets.total) + ",";
        json += "\"rx_hour\":" + String(rxPackets.lastHour) + ",";
        json += "\"rx_day\":" + String(rxPackets.lastDay) + ",";
        json += "\"tx_total\":" + String(txPackets.total) + ",";
        json += "\"tx_hour\":" + String(txPackets.lastHour) + ",";
        json += "\"crc_errors\":" + String(crcErrors.total) + ",";
        json += "\"duplicates\":" + String(duplicates.total) + ",";

        // Signal-Qualität
        json += "\"signal\":{";
        json += "\"rssi_avg\":" + String(globalSignal.avgRSSI) + ",";
        json += "\"rssi_min\":" + String(globalSignal.minRSSI) + ",";
        json += "\"rssi_max\":" + String(globalSignal.maxRSSI) + ",";
        json += "\"snr_avg\":" + String(globalSignal.avgSNR, 1) + ",";
        json += "\"snr_min\":" + String(globalSignal.minSNR, 1) + ",";
        json += "\"snr_max\":" + String(globalSignal.maxSNR, 1);
        json += "},";

        // Packet-Typen
        json += "\"types\":{";
        json += "\"position\":" + String(positionPackets) + ",";
        json += "\"message\":" + String(messagePackets) + ",";
        json += "\"status\":" + String(statusPackets) + ",";
        json += "\"object\":" + String(objectPackets) + ",";
        json += "\"weather\":" + String(weatherPackets) + ",";
        json += "\"telemetry\":" + String(telemetryPackets);
        json += "},";

        // Routen
        json += "\"routes\":{";
        json += "\"lora_to_aprsis\":" + String(loraToAprsIs) + ",";
        json += "\"aprsis_to_lora\":" + String(aprsIsToLora) + ",";
        json += "\"digipeated\":" + String(digipeated) + ",";
        json += "\"mqtt\":" + String(mqttPublished);
        json += "},";

        // Beste/Schlechteste
        json += "\"best\":{";
        json += "\"callsign\":\"" + bestSignal.callsign + "\",";
        json += "\"rssi\":" + String(bestSignal.rssi) + ",";
        json += "\"snr\":" + String(bestSignal.snr, 1);
        json += "},";

        json += "\"worst\":{";
        json += "\"callsign\":\"" + worstSignal.callsign + "\",";
        json += "\"rssi\":" + String(worstSignal.rssi) + ",";
        json += "\"snr\":" + String(worstSignal.snr, 1);
        json += "},";

        // Station-Count
        json += "\"unique_stations\":" + String(stationStats.size());

        json += "}";
        return json;
    }

    /**
     * @brief Gibt Statistiken für spezifische Station zurück
     */
    String getStationStatsJSON(const String& callsign) const {
        auto it = stationStats.find(callsign);
        if (it == stationStats.end()) {
            return "{}";
        }

        const StationStats& stats = it->second;
        String json = "{";
        json += "\"callsign\":\"" + callsign + "\",";
        json += "\"packets\":" + String(stats.packetCount) + ",";
        json += "\"rssi_avg\":" + String(stats.signal.avgRSSI) + ",";
        json += "\"rssi_min\":" + String(stats.signal.minRSSI) + ",";
        json += "\"rssi_max\":" + String(stats.signal.maxRSSI) + ",";
        json += "\"snr_avg\":" + String(stats.signal.avgSNR, 1) + ",";
        json += "\"last_seen\":" + String((millis() - stats.lastSeen) / 1000);
        json += "}";
        return json;
    }

    /**
     * @brief Gibt Top-N Stationen nach Packet-Count zurück
     */
    String getTopStationsJSON(uint8_t count = 10) const {
        // Erstelle sortierte Liste
        std::vector<std::pair<String, uint32_t>> sorted;
        for (const auto& pair : stationStats) {
            sorted.push_back({pair.first, pair.second.packetCount});
        }

        // Sortiere nach Packet-Count
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        // Erstelle JSON
        String json = "[";
        for (size_t i = 0; i < std::min((size_t)count, sorted.size()); i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"callsign\":\"" + sorted[i].first + "\",";
            json += "\"packets\":" + String(sorted[i].second);
            json += "}";
        }
        json += "]";
        return json;
    }

    /**
     * @brief Console-Output
     */
    void printSummary() const {
        Serial.println("\n===== Packet Statistics =====");
        Serial.printf("Uptime: %lu seconds\n", (millis() - startTime) / 1000);
        Serial.printf("RX Total: %lu (%.1f/hour)\n",
                     rxPackets.total, rxPackets.avgPerHour);
        Serial.printf("TX Total: %lu\n", txPackets.total);
        Serial.printf("CRC Errors: %lu (%.1f%%)\n",
                     crcErrors.total,
                     rxPackets.total > 0 ? (crcErrors.total * 100.0 / rxPackets.total) : 0);
        Serial.printf("Avg RSSI: %d dBm (Min: %d, Max: %d)\n",
                     globalSignal.avgRSSI, globalSignal.minRSSI, globalSignal.maxRSSI);
        Serial.printf("Avg SNR: %.1f dB (Min: %.1f, Max: %.1f)\n",
                     globalSignal.avgSNR, globalSignal.minSNR, globalSignal.maxSNR);
        Serial.printf("Unique Stations: %u\n", stationStats.size());
        Serial.println("============================\n");
    }

private:
    void incrementCounter(CounterStats& counter) {
        counter.total++;
        counter.lastHour++;
        counter.lastDay++;
    }

    void checkTimeResets() {
        uint32_t now = millis();

        // Stündlicher Reset
        if (now - lastHourReset >= 3600000) {  // 1 Stunde
            rxPackets.avgPerHour = rxPackets.lastHour;
            rxPackets.lastHour = 0;
            txPackets.lastHour = 0;
            lastHourReset = now;
        }

        // Täglicher Reset
        if (now - lastDayReset >= 86400000) {  // 1 Tag
            rxPackets.lastDay = 0;
            txPackets.lastDay = 0;
            lastDayReset = now;
        }
    }
};

#endif
