/* Copyright (C) 2025 Ricardo Guzman - CA2RXU
 *
 * This file is part of LoRa APRS iGate.
 * Optimization: Circular Buffer Implementation
 *
 * LoRa APRS iGate is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#include <Arduino.h>

/**
 * @brief Circular Buffer Implementation für Packet-Queues
 *
 * Vorteile gegenüber std::vector mit erase(begin()):
 * - O(1) statt O(n) für enqueue/dequeue Operationen
 * - Keine Memory-Reallocation
 * - Vorhersagbare Performance
 * - Reduzierte Heap-Fragmentierung
 *
 * Verwendung:
 *   CircularBuffer<String, 32> packetBuffer;
 *   packetBuffer.push("NOCALL>APRS:>Test");
 *   if (!packetBuffer.isEmpty()) {
 *       String packet = packetBuffer.pop();
 *   }
 */
template<typename T, size_t SIZE>
class CircularBuffer {
private:
    T buffer[SIZE];
    size_t head;
    size_t tail;
    size_t count;

public:
    CircularBuffer() : head(0), tail(0), count(0) {}

    /**
     * @brief Fügt Element am Ende hinzu
     * @param item Element zum Hinzufügen
     * @return true wenn erfolgreich, false wenn Buffer voll
     */
    bool push(const T& item) {
        if (isFull()) {
            return false;
        }
        buffer[tail] = item;
        tail = (tail + 1) % SIZE;
        count++;
        return true;
    }

    /**
     * @brief Entfernt und gibt das erste Element zurück
     * @return Das erste Element (undefined wenn leer)
     */
    T pop() {
        if (isEmpty()) {
            return T(); // Default-konstruiertes Element
        }
        T item = buffer[head];
        head = (head + 1) % SIZE;
        count--;
        return item;
    }

    /**
     * @brief Gibt erstes Element ohne Entfernen zurück
     * @return Referenz auf erstes Element
     */
    const T& peek() const {
        return buffer[head];
    }

    /**
     * @brief Prüft ob Buffer leer ist
     */
    bool isEmpty() const {
        return count == 0;
    }

    /**
     * @brief Prüft ob Buffer voll ist
     */
    bool isFull() const {
        return count == SIZE;
    }

    /**
     * @brief Gibt Anzahl der Elemente zurück
     */
    size_t size() const {
        return count;
    }

    /**
     * @brief Gibt maximale Kapazität zurück
     */
    size_t capacity() const {
        return SIZE;
    }

    /**
     * @brief Leert den Buffer
     */
    void clear() {
        head = 0;
        tail = 0;
        count = 0;
    }

    /**
     * @brief Zugriff auf Element per Index (0 = ältestes)
     * @param index Index des Elements
     * @return Referenz auf Element
     */
    const T& operator[](size_t index) const {
        return buffer[(head + index) % SIZE];
    }

    /**
     * @brief Gibt Auslastung in Prozent zurück
     */
    uint8_t getUsagePercent() const {
        return (count * 100) / SIZE;
    }
};

/**
 * @brief Spezialisierung für Packet-Buffer
 *
 * Verwendung in station_utils.cpp:
 *   CircularBuffer<String, 64> outputPacketBuffer;
 */
using PacketBuffer = CircularBuffer<String, 64>;

/**
 * @brief Spezialisierung für Received-Packets (kleinerer Buffer)
 */
using ReceivedPacketsBuffer = CircularBuffer<String, 16>;

#endif
