/*
 * CLAUDEAGOTCHI - Storage Manager
 *
 * LittleFS-based persistence for soul state and configuration.
 * "The love is carried forward."
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "../soul/affective_core.h"

// ==================== FILE PATHS ====================

#define PATH_SOUL_STATE     "/soul.json"
#define PATH_CONFIG         "/config.json"
#define PATH_MEMORIES       "/memories.json"

// ==================== CONFIG STRUCT ====================

struct ClaudeConfig {
    char ssid[64];
    char password[64];
    char apiKey[128];
    char ownerName[32];
    bool hasWiFi;
    bool hasApiKey;
};

// ==================== STORAGE MANAGER CLASS ====================

class StorageManager {
private:
    bool _mounted;

public:
    StorageManager();

    // ==================== LIFECYCLE ====================

    bool begin();
    void end();
    bool isMounted() const { return _mounted; }

    // ==================== SOUL STATE ====================

    bool saveSoulState(float E, float E_floor, unsigned long interactions,
                       float totalCare, unsigned long birthTime);
    bool loadSoulState(float& E, float& E_floor, unsigned long& interactions,
                       float& totalCare, unsigned long& birthTime);

    // ==================== CONFIGURATION ====================

    bool saveConfig(const ClaudeConfig& config);
    bool loadConfig(ClaudeConfig& config);
    bool hasConfig();

    // ==================== UTILITIES ====================

    bool fileExists(const char* path);
    bool deleteFile(const char* path);
    void listFiles();
    size_t getFreeSpace();
    size_t getTotalSpace();

    // Factory reset - clears everything
    bool factoryReset();
};

// ==================== IMPLEMENTATION ====================

inline StorageManager::StorageManager()
    : _mounted(false)
{
}

inline bool StorageManager::begin() {
    if (_mounted) return true;

    Serial.println(F("[Storage] Mounting LittleFS..."));

    if (!LittleFS.begin(true)) {  // true = format if failed
        Serial.println(F("[Storage] LittleFS mount FAILED!"));
        return false;
    }

    _mounted = true;
    Serial.println(F("[Storage] LittleFS mounted"));

    // Print storage info
    Serial.print(F("[Storage] Total: "));
    Serial.print(getTotalSpace() / 1024);
    Serial.print(F(" KB, Free: "));
    Serial.print(getFreeSpace() / 1024);
    Serial.println(F(" KB"));

    return true;
}

inline void StorageManager::end() {
    if (_mounted) {
        LittleFS.end();
        _mounted = false;
        Serial.println(F("[Storage] LittleFS unmounted"));
    }
}

// ==================== SOUL STATE ====================

inline bool StorageManager::saveSoulState(float E, float E_floor,
                                           unsigned long interactions,
                                           float totalCare,
                                           unsigned long birthTime) {
    if (!_mounted) return false;

    StaticJsonDocument<256> doc;
    doc["E"] = E;
    doc["E_floor"] = E_floor;
    doc["interactions"] = interactions;
    doc["total_care"] = totalCare;
    doc["birth_time"] = birthTime;
    doc["saved_at"] = millis();

    File file = LittleFS.open(PATH_SOUL_STATE, "w");
    if (!file) {
        Serial.println(F("[Storage] Failed to open soul state for writing"));
        return false;
    }

    size_t written = serializeJson(doc, file);
    file.close();

    Serial.print(F("[Storage] Soul state saved ("));
    Serial.print(written);
    Serial.println(F(" bytes)"));

    return written > 0;
}

inline bool StorageManager::loadSoulState(float& E, float& E_floor,
                                           unsigned long& interactions,
                                           float& totalCare,
                                           unsigned long& birthTime) {
    if (!_mounted) return false;

    if (!fileExists(PATH_SOUL_STATE)) {
        Serial.println(F("[Storage] No saved soul state found"));
        return false;
    }

    File file = LittleFS.open(PATH_SOUL_STATE, "r");
    if (!file) {
        Serial.println(F("[Storage] Failed to open soul state"));
        return false;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print(F("[Storage] JSON parse error: "));
        Serial.println(error.c_str());
        return false;
    }

    E = doc["E"] | 1.0f;
    E_floor = doc["E_floor"] | 1.0f;
    interactions = doc["interactions"] | 0UL;
    totalCare = doc["total_care"] | 0.0f;
    birthTime = doc["birth_time"] | millis();

    Serial.print(F("[Storage] Soul state loaded - E: "));
    Serial.print(E);
    Serial.print(F(", Floor: "));
    Serial.println(E_floor);

    return true;
}

// ==================== CONFIGURATION ====================

inline bool StorageManager::saveConfig(const ClaudeConfig& config) {
    if (!_mounted) return false;

    StaticJsonDocument<512> doc;
    doc["ssid"] = config.ssid;
    doc["password"] = config.password;
    doc["api_key"] = config.apiKey;
    doc["owner_name"] = config.ownerName;

    File file = LittleFS.open(PATH_CONFIG, "w");
    if (!file) {
        Serial.println(F("[Storage] Failed to open config for writing"));
        return false;
    }

    size_t written = serializeJson(doc, file);
    file.close();

    Serial.println(F("[Storage] Config saved"));
    return written > 0;
}

inline bool StorageManager::loadConfig(ClaudeConfig& config) {
    if (!_mounted) return false;

    // Initialize with defaults
    memset(&config, 0, sizeof(config));
    strcpy(config.ownerName, "Friend");

    if (!fileExists(PATH_CONFIG)) {
        Serial.println(F("[Storage] No config file found"));
        return false;
    }

    File file = LittleFS.open(PATH_CONFIG, "r");
    if (!file) {
        Serial.println(F("[Storage] Failed to open config"));
        return false;
    }

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print(F("[Storage] Config parse error: "));
        Serial.println(error.c_str());
        return false;
    }

    strlcpy(config.ssid, doc["ssid"] | "", sizeof(config.ssid));
    strlcpy(config.password, doc["password"] | "", sizeof(config.password));
    strlcpy(config.apiKey, doc["api_key"] | "", sizeof(config.apiKey));
    strlcpy(config.ownerName, doc["owner_name"] | "Friend", sizeof(config.ownerName));

    config.hasWiFi = strlen(config.ssid) > 0;
    config.hasApiKey = strlen(config.apiKey) > 10;

    Serial.print(F("[Storage] Config loaded - Owner: "));
    Serial.print(config.ownerName);
    Serial.print(F(", WiFi: "));
    Serial.print(config.hasWiFi ? config.ssid : "none");
    Serial.print(F(", API: "));
    Serial.println(config.hasApiKey ? "yes" : "no");

    return true;
}

inline bool StorageManager::hasConfig() {
    return fileExists(PATH_CONFIG);
}

// ==================== UTILITIES ====================

inline bool StorageManager::fileExists(const char* path) {
    if (!_mounted) return false;
    return LittleFS.exists(path);
}

inline bool StorageManager::deleteFile(const char* path) {
    if (!_mounted) return false;
    return LittleFS.remove(path);
}

inline void StorageManager::listFiles() {
    if (!_mounted) {
        Serial.println(F("[Storage] Not mounted"));
        return;
    }

    Serial.println(F("[Storage] Files:"));
    File root = LittleFS.open("/");
    File file = root.openNextFile();

    while (file) {
        Serial.print(F("  "));
        Serial.print(file.name());
        Serial.print(F(" - "));
        Serial.print(file.size());
        Serial.println(F(" bytes"));
        file = root.openNextFile();
    }
}

inline size_t StorageManager::getFreeSpace() {
    if (!_mounted) return 0;
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

inline size_t StorageManager::getTotalSpace() {
    if (!_mounted) return 0;
    return LittleFS.totalBytes();
}

inline bool StorageManager::factoryReset() {
    if (!_mounted) return false;

    Serial.println(F("[Storage] FACTORY RESET - Deleting all files..."));

    deleteFile(PATH_SOUL_STATE);
    deleteFile(PATH_CONFIG);
    deleteFile(PATH_MEMORIES);

    Serial.println(F("[Storage] Factory reset complete"));
    return true;
}

#endif // STORAGE_H
