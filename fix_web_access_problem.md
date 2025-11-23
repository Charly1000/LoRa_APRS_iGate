# 🔧 Fix: Website nicht erreichbar

## Problem
Nach dem Firmware-Upload:
- ✅ Display zeigt Verbindung zum AP
- ✅ IP-Adresse wird angezeigt
- ❌ Website ist nicht über Browser erreichbar

## Ursache
Die alte Config-Datei `/igate_conf.json` auf dem ESP32 enthält die neuen 
`customText` Felder nicht. Beim Versuch, die Config zu aktualisieren, 
könnte es zu Problemen kommen.

## Lösung 1: Config zurücksetzen (EMPFOHLEN)

### Schritt 1: ESP32 via Serial Monitor verbinden
Öffnen Sie den Serial Monitor in PlatformIO oder Arduino IDE:
- Baudrate: 115200
- Port: Ihr ESP32-Port

### Schritt 2: Suchen Sie nach Fehlermeldungen
Schauen Sie nach Meldungen wie:
- `Config file corrupted`
- `Failed to write config`
- `Out of memory`
- `Web server failed to start`

### Schritt 3: Config löschen über Serial Commands
Wenn der ESP32 einen Command-Mode hat, versuchen Sie:
```
factory reset
```

Oder verwenden Sie die Web-API (falls erreichbar):
```
http://[ESP32-IP]/reset
```

## Lösung 2: Firmware mit leerer Config flashen

### Für PlatformIO:
```bash
# 1. Filesystem löschen
pio run --target erase

# 2. Firmware neu flashen
pio run --target upload

# 3. Filesystem neu flashen (data_embed)
pio run --target uploadfs
```

### Für Arduino IDE:
1. Tools → Flash Erase → "All Flash Contents"
2. Sketch → Upload

## Lösung 3: Manuell Config-Datei löschen

Falls Sie Zugriff auf das Dateisystem haben:
1. Verbinden via SPIFFS/LittleFS Browser
2. Datei `/igate_conf.json` löschen
3. ESP32 neu starten

## Lösung 4: Alte Firmware flashen (Notfall)

Wenn nichts funktioniert:
1. Flashen Sie die original Firmware zurück
2. Speichern Sie Ihre Konfiguration
3. Flashen Sie dann die neue Firmware
4. Konfiguration neu eingeben

## Was beim nächsten Start passieren sollte

Nach dem Reset sollten Sie im Serial Monitor sehen:
```
[Config] Using default values
[Web] Starting web server on port 80
[Web] Web server started successfully
[WiFi] Connected to AP
[WiFi] IP address: 192.168.x.x
```

## Überprüfung

1. **Serial Monitor Output:** Zeigt keine Fehler?
2. **LED Blinken:** Normales Blink-Muster?
3. **Display:** Zeigt "Web: Ready" oder ähnliches?
4. **Browser:** `http://[IP-Adresse]` erreichbar?

## Wenn es immer noch nicht funktioniert

Senden Sie mir den Serial Monitor Output, insbesondere:
- Die ersten 100 Zeilen nach dem Boot
- Alle Zeilen mit `[Error]`, `[FAIL]`, oder `[WARNING]`

## Debug-Befehle (falls verfügbar)

Im Serial Monitor versuchen:
```
status
wifi status
config show
memory
```

