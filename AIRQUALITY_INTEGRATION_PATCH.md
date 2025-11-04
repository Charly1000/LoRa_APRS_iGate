# Air Quality Integration - Code Patch

## Übersicht

Diese Datei zeigt die **minimalen Änderungen** die nötig sind, um Luftqualität in APRS-Beacons einzubauen.

---

## Änderung 1: utils.cpp - Luftqualität zum Beacon hinzufügen

**Datei:** `src/utils.cpp`

**Position:** Nach Zeile 182 (nach dem Wetterdaten hinzufügen)

**Änderung:**
```cpp
// Originale Zeilen 178-184:
if (Config.wxsensor.active) {
    String sensorData = (wxModuleType == 0) ? ".../...g...t..." : WX_Utils::readDataSensor();
    beaconPacket            += sensorData;
    secondaryBeaconPacket   += sensorData;
}

// ✅ NEU: Luftqualität hinzufügen (NACH Wetterdaten, VOR comment)
if (Config.wxsensor.active) {
    String airQuality = WX_Utils::getAirQualityForBeacon();
    if (!airQuality.isEmpty()) {
        beaconPacket            += " ";  // Leerzeichen vor AQ
        beaconPacket            += airQuality;
        secondaryBeaconPacket   += " ";
        secondaryBeaconPacket   += airQuality;
    }
}

beaconPacket            += Config.beacon.comment;
secondaryBeaconPacket   += Config.beacon.comment;
```

**Komplette geänderte Sektion (Zeilen 178-189):**
```cpp
if (Config.wxsensor.active) {
    String sensorData = (wxModuleType == 0) ? ".../...g...t..." : WX_Utils::readDataSensor();
    beaconPacket            += sensorData;
    secondaryBeaconPacket   += sensorData;

    // ✅ NEW: Add air quality if available (BME680)
    String airQuality = WX_Utils::getAirQualityForBeacon();
    if (!airQuality.isEmpty()) {
        beaconPacket            += " ";
        beaconPacket            += airQuality;
        secondaryBeaconPacket   += " ";
        secondaryBeaconPacket   += airQuality;
    }
}
beaconPacket            += Config.beacon.comment;
secondaryBeaconPacket   += Config.beacon.comment;
```

---

## Änderung 2: wx_utils.h - Neue Funktionen deklarieren

**Datei:** `include/wx_utils.h`

**Hinzufügen am Ende (vor `}` von namespace):**
```cpp
namespace WX_Utils {

    void    getWxModuleAddres();
    void    setup();
    String  generateTempString(const float sensorTemp);
    String  generateHumString(const float sensorHum);
    String  generatePresString(const float sensorPres);
    String  readDataSensor();

    // ✅ NEW: Air quality functions
    String  getAirQualityForBeacon();      // Returns "AQ:95%" or empty
    String  getAirQualityStatsJSON();      // Returns JSON stats for web API

}
```

---

## Änderung 3: wx_utils.cpp - Luftqualität implementieren

**Datei:** `src/wx_utils.cpp`

**Am Anfang hinzufügen (nach anderen includes):**
```cpp
#include "wx_utils_airquality.h"  // ✅ NEW
```

**Nach den globalen Variablen hinzufügen:**
```cpp
extern Configuration            Config;
extern String                   fifthLine;

// ✅ NEW: Air quality export
String airQualityString = "";
AirQuality::Statistics airQualityStats;
```

**In readDataSensor() - BME680 case (ca. Zeile 250-260) ändern:**
```cpp
// VORHER:
case 3: // BME680
    #if !defined(HELTEC_V3) && !defined(HELTEC_V3_2)
        bme680.performReading();
        delay(50);
        if (bme680.endReading()) {
            newTemp     = bme680.temperature;
            newPress    = (bme680.pressure / 100.0F);
            newHum      = bme680.humidity;
            newGas      = bme680.gas_resistance / 1000.0; // in Kilo ohms
        }
    #endif
    break;

// NACHHER:
case 3: // BME680
    #if !defined(HELTEC_V3) && !defined(HELTEC_V3_2)
        bme680.performReading();
        delay(50);
        if (bme680.endReading()) {
            newTemp     = bme680.temperature;
            newPress    = (bme680.pressure / 100.0F);
            newHum      = bme680.humidity;
            newGas      = bme680.gas_resistance / 1000.0;

            // ✅ NEW: Calculate air quality
            uint8_t airQuality = AirQuality::calculateAirQualityPercent(newGas);
            airQualityStats.update(airQuality);
            airQualityString = AirQuality::formatForAPRS(newGas, false);

            Serial.printf("[WX] BME680: Gas=%.1fkOhm AQ=%d%%\n", newGas, airQuality);
        }
    #endif
    break;
```

**Am Ende der readDataSensor() Funktion (VORHER return wxPayload):**
```cpp
// Alte Zeilen 311-316 ENTFERNEN:
if (wxModuleType == 3) {
    wxPayload += "Gas: ";
    wxPayload += String(newGas);
    wxPayload += "Kohms";
}

// ✅ NICHT mehr zum wxPayload hinzufügen!
// Luftqualität wird über airQualityString exportiert

return wxPayload;
```

**Am Ende der Datei hinzufügen:**
```cpp
String getAirQualityForBeacon() {
    return airQualityString;
}

String getAirQualityStatsJSON() {
    if (wxModuleType != 3) {  // Not BME680
        return "{\"available\":false}";
    }

    String json = "{\"available\":true,";
    json += "\"gas_resistance\":" + String(newGas, 1) + ",";
    json += "\"quality\":" + airQualityStats.toJSON();
    json += "}";
    return json;
}
```

---

## Änderung 4: (Optional) Web-API für Luftqualität

**Datei:** `src/web_utils.cpp`

**In der setup() Funktion hinzufügen:**
```cpp
// Neuer API-Endpunkt für Luftqualität
server.on("/api/airquality", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = WX_Utils::getAirQualityStatsJSON();
    request->send(200, "application/json", json);
});
```

---

## Beispiel APRS-Beacon

### Vorher (ohne Luftqualität):
```
N0CALL-10>APLRG1,WIDE1-1:!4807.04N/01131.00E#PHG5132/A=001234.../...g...t068h45b10156LoRa APRS iGate
```

### Nachher (mit Luftqualität):
```
N0CALL-10>APLRG1,WIDE1-1:!4807.04N/01131.00E#PHG5132/A=001234.../...g...t068h45b10156 AQ:95%LoRa APRS iGate
                                                                                         ^^^^^^^^
                                                                                         NEU!
```

**Format:**
- `.../...g...t068h45b10156` - Standard APRS Wetter-Format
- ` AQ:95%` - Luftqualität (Leerzeichen davor)
- `LoRa APRS iGate` - Original Comment

---

## Interpretation der Werte

| Gas-Widerstand | Luftqualität | Beschreibung |
|----------------|--------------|--------------|
| > 150 kOhm | 100% | Excellent - Frische Luft |
| 80-150 kOhm | 80-99% | Good - Normale Raumluft |
| 40-80 kOhm | 60-79% | Moderate - Leicht belastet |
| 20-40 kOhm | 40-59% | Poor - Belastet (Kochen, etc.) |
| 10-20 kOhm | 20-39% | Bad - Stark belastet |
| 2-10 kOhm | 1-19% | Hazardous - Sehr schlecht |

**Beispiele:**
- `AQ:98%` - Sehr gute Luft (frische Außenluft)
- `AQ:75%` - Gute Luft (normale Wohnung)
- `AQ:45%` - Mittelmäßig (beim Kochen)
- `AQ:15%` - Schlecht (Rauch, starke VOCs)

---

## Kalibrierung (Optional)

Für genauere Werte kann die Kalibrierung angepasst werden:

**Datei:** `include/wx_utils_airquality.h`

**Zeilen 28-36 ändern:**
```cpp
const Calibration DEFAULT_CALIBRATION = {
    .excellentThreshold = 150.0,  // Anpassen an Ihre Umgebung
    .goodThreshold = 80.0,
    .moderateThreshold = 40.0,
    .poorThreshold = 20.0,
    .badThreshold = 10.0,
    .worstThreshold = 2.0
};
```

**Kalibrierungs-Prozedur:**
1. Sensor 30 Min in frischer Außenluft laufen lassen
2. Durchschnittlichen Gas-Widerstand notieren (z.B. 120 kOhm)
3. `excellentThreshold = 120.0` setzen
4. Andere Werte proportional anpassen

---

## Testing

### Test 1: Serial Monitor

Nach Integration sollten Sie sehen:
```
[WX] BME680: Gas=95.3kOhm AQ=82% (Good)
```

### Test 2: APRS.fi

Beacon sollte auf aprs.fi erscheinen mit:
```
Comment: .../...g...t068h45b10156 AQ:82%LoRa APRS iGate
```

### Test 3: Web API

```bash
curl http://192.168.1.100/api/airquality
```

Response:
```json
{
  "available": true,
  "gas_resistance": 95.3,
  "quality": {
    "current": 82,
    "min": 65,
    "max": 98,
    "avg": 78,
    "samples": 123
  }
}
```

---

## Troubleshooting

### Luftqualität erscheint nicht im Beacon

**Check:**
1. Ist BME680 verbunden? (nicht BME280!)
2. Serial Monitor: Erscheint `[WX] BME680:...`?
3. `airQualityString` leer? → Gas-Widerstand invalide

### Luftqualität immer 100% oder 1%

**Lösung:**
- Kalibrierung anpassen (siehe oben)
- Gas-Widerstand-Wert im Serial Monitor prüfen

### Beacon zu lang

**Lösung:**
- `formatForAPRS(newGas, false)` verwendet (ohne Beschreibung)
- Evtl. Beacon-Comment kürzen

---

## Zusammenfassung

**Geänderte Dateien:**
1. ✅ `src/utils.cpp` - 6 Zeilen hinzugefügt
2. ✅ `include/wx_utils.h` - 2 Zeilen hinzugefügt
3. ✅ `src/wx_utils.cpp` - ~15 Zeilen geändert
4. ✅ `include/wx_utils_airquality.h` - NEU (350 Zeilen)

**Aufwand:** 30-45 Minuten Integration

**Ergebnis:**
- 📊 Luftqualität in jedem APRS-Beacon
- 📊 Format: `AQ:95%` (kompakt)
- 📊 100% = beste, 1% = schlechteste Luft
- 📊 Automatische Berechnung aus Gas-Widerstand

---

**73 de CA2RXU** 📡
