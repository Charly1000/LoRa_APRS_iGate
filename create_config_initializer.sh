#!/bin/bash
echo "Creating config initializer patch..."

# This will add a function to check and initialize customText fields if missing
cat >> /tmp/config_init_patch.txt << 'PATCH'

Add this to configuration.cpp after readFile():

void Configuration::ensureCustomTextFieldsExist() {
    // Check if customText fields are in the file
    File file = SPIFFS.open("/igate_conf.json", "r");
    if (!file) return;
    
    DynamicJsonDocument doc(JSON_BUFFER_SIZE);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) return;
    
    bool needsUpdate = false;
    
    // Check if customText section exists
    if (!doc.containsKey("customText")) {
        needsUpdate = true;
    } else {
        // Check if all fields exist
        JsonObject ct = doc["customText"];
        if (!ct.containsKey("aprsIsAuthSoftware") ||
            !ct.containsKey("queryResponseId") ||
            !ct.containsKey("syslogIdentifier") ||
            !ct.containsKey("wifiHostnamePrefix") ||
            !ct.containsKey("epaperInitText")) {
            needsUpdate = true;
        }
    }
    
    if (needsUpdate) {
        Serial.println("[Config] Initializing missing customText fields...");
        writeFile();  // Write current config with all defaults
    }
}

PATCH
cat /tmp/config_init_patch.txt
