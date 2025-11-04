/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * Optimization: HashMap for Station Tracking
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STATION_MAP_H_
#define STATION_MAP_H_

#include <Arduino.h>
#include <map>

/**
 * @brief Optimierter Station-Tracker mit HashMap
 *
 * Problem der aktuellen Implementierung (station_utils.cpp):
 *   bool wasHeard(const String& station) {
 *       for (int i = 0; i < lastHeardStations.size(); i++) {
 *           if (lastHeardStations[i].station == station) return true;
 *       }
 *       return false;  // O(n) für jeden Check!
 *   }
 *
 * Bei 50 Stationen = 50 String-Vergleiche pro Check
 * Bei 10 Checks/Sekunde = 500 String-Vergleiche/Sekunde
 *
 * Lösung: std::map für O(log n) oder std::unordered_map für O(1)
 *
 * Performance-Vergleich:
 *   Vector (alt): O(n) = 50 Vergleiche bei 50 Stationen
 *   Map (neu):    O(log n) = 6 Vergleiche bei 50 Stationen (8x schneller)
 */

struct StationInfo {
    uint32_t lastHeardTime;
    int16_t lastRSSI;
    float lastSNR;
    uint16_t packetCount;
    String lastPacket;

    StationInfo()
        : lastHeardTime(0), lastRSSI(0), lastSNR(0.0), packetCount(0) {}

    StationInfo(uint32_t time, int16_t rssi = 0, float snr = 0.0)
        : lastHeardTime(time), lastRSSI(rssi), lastSNR(snr), packetCount(1) {}
};

class StationMap {
private:
    std::map<String, StationInfo> stations;
    uint32_t rememberTimeMs;
    uint32_t lastCleanup;
    uint32_t cleanupInterval;

public:
    /**
     * @brief Konstruktor
     * @param rememberMinutes Wie lange Stationen gespeichert werden (in Minuten)
     */
    explicit StationMap(uint16_t rememberMinutes = 30)
        : rememberTimeMs(rememberMinutes * 60 * 1000),
          lastCleanup(0),
          cleanupInterval(60000) {}  // Cleanup alle 60 Sekunden

    /**
     * @brief Aktualisiert Station (fügt hinzu wenn neu)
     * @param callsign Rufzeichen der Station
     * @param rssi RSSI-Wert (optional)
     * @param snr SNR-Wert (optional)
     */
    void updateStation(const String& callsign, int16_t rssi = 0, float snr = 0.0) {
        auto it = stations.find(callsign);

        if (it != stations.end()) {
            // Station existiert - aktualisiere
            it->second.lastHeardTime = millis();
            it->second.lastRSSI = rssi;
            it->second.lastSNR = snr;
            it->second.packetCount++;
        } else {
            // Neue Station - füge hinzu
            stations[callsign] = StationInfo(millis(), rssi, snr);
        }

        // Periodisches Cleanup
        if (millis() - lastCleanup > cleanupInterval) {
            cleanup();
        }
    }

    /**
     * @brief Prüft ob Station kürzlich gehört wurde
     * @param callsign Rufzeichen
     * @return true wenn innerhalb remember-Zeit
     *
     * Komplexität: O(log n) statt O(n)
     */
    bool wasHeard(const String& callsign) const {
        auto it = stations.find(callsign);
        if (it == stations.end()) {
            return false;
        }

        // Prüfe ob noch gültig
        return (millis() - it->second.lastHeardTime) < rememberTimeMs;
    }

    /**
     * @brief Gibt Station-Info zurück
     * @param callsign Rufzeichen
     * @param info Referenz für Rückgabe
     * @return true wenn Station existiert
     */
    bool getStationInfo(const String& callsign, StationInfo& info) const {
        auto it = stations.find(callsign);
        if (it == stations.end()) {
            return false;
        }
        info = it->second;
        return true;
    }

    /**
     * @brief Gibt Anzahl aktiver Stationen zurück
     */
    size_t getActiveStationCount() const {
        return stations.size();
    }

    /**
     * @brief Cleanup alter Stationen
     * @return Anzahl entfernter Stationen
     */
    size_t cleanup() {
        size_t removed = 0;
        uint32_t now = millis();

        for (auto it = stations.begin(); it != stations.end(); ) {
            if (now - it->second.lastHeardTime >= rememberTimeMs) {
                it = stations.erase(it);
                removed++;
            } else {
                ++it;
            }
        }

        lastCleanup = millis();
        return removed;
    }

    /**
     * @brief Gibt Statistiken zurück
     */
    struct Statistics {
        size_t totalStations;
        size_t activeStations;
        uint32_t oldestStationAge;
        String mostActiveCallsign;
        uint16_t maxPacketCount;
    };

    Statistics getStatistics() const {
        Statistics stats = {0, 0, 0, "", 0};
        stats.totalStations = stations.size();
        stats.activeStations = stations.size();

        uint32_t now = millis();
        uint32_t maxAge = 0;

        for (const auto& pair : stations) {
            uint32_t age = now - pair.second.lastHeardTime;
            if (age > maxAge) {
                maxAge = age;
            }

            if (pair.second.packetCount > stats.maxPacketCount) {
                stats.maxPacketCount = pair.second.packetCount;
                stats.mostActiveCallsign = pair.first;
            }
        }

        stats.oldestStationAge = maxAge / 1000; // in Sekunden
        return stats;
    }

    /**
     * @brief Setzt Remember-Zeit
     */
    void setRememberTime(uint16_t minutes) {
        rememberTimeMs = minutes * 60 * 1000;
    }

    /**
     * @brief Löscht alle Stationen
     */
    void clear() {
        stations.clear();
    }

    /**
     * @brief Iterator-Zugriff für externe Verarbeitung
     */
    const std::map<String, StationInfo>& getStations() const {
        return stations;
    }
};

/**
 * @brief Verwendungsbeispiel in station_utils.cpp:
 *
 * // Ersetze:
 * std::vector<LastHeardStation> lastHeardStations;
 *
 * // Durch:
 * StationMap stationMap(Config.rememberStationTime);
 *
 * // Alte Funktion:
 * void updateLastHeard(const String& station) {
 *     // ... 20 Zeilen Code mit Vector-Operationen
 * }
 *
 * // Neue Funktion:
 * void updateLastHeard(const String& station) {
 *     stationMap.updateStation(station);  // 1 Zeile!
 * }
 *
 * // Alte Funktion:
 * bool wasHeard(const String& station) {
 *     for (int i = 0; i < lastHeardStations.size(); i++) {  // O(n)
 *         if (lastHeardStations[i].station == station) return true;
 *     }
 *     return false;
 * }
 *
 * // Neue Funktion:
 * bool wasHeard(const String& station) {
 *     return stationMap.wasHeard(station);  // O(log n)
 * }
 *
 * Erwartete Verbesserungen:
 *   - wasHeard() Speed: 5-10x schneller bei 30+ Stationen
 *   - CPU-Last: -5% bis -8%
 *   - Code-Zeilen: -40%
 *   - Wartbarkeit: +++
 */

#endif
