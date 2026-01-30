/*
 * SD Card Configuration & Chat History
 *
 * Reads config.json from SD card for cloud credentials and WiFi networks.
 * Backs up config to LittleFS for operation without SD card.
 * Logs chat history to SD card (one file per day).
 *
 * config.json schema:
 * {
 *   "cloud_url": "https://backend-production-507c.up.railway.app",
 *   "device_token": "apex_dev_...",
 *   "device_id": "550e8400-...",
 *   "api_version": "v1",
 *   "wifi": [
 *     {"ssid": "HomeWiFi", "pass": "password1"},
 *     {"ssid": "WorkWiFi", "pass": "password2"}
 *   ]
 * }
 */

#ifndef SDCONFIG_H
#define SDCONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"
#include "cloud.h"

#if USE_LITTLEFS
#include <LittleFS.h>
#endif

// ============================================================================
// SD CARD INITIALIZATION
// ============================================================================

inline bool sdInit() {
    #ifdef FEATURE_SD_CARD
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println(F("[SD] Card init failed or not present"));
        return false;
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card mounted, size: %llu MB\n", cardSize);
    return true;
    #else
    Serial.println(F("[SD] SD card feature not enabled"));
    return false;
    #endif
}

// ============================================================================
// CONFIG.JSON READER
// ============================================================================

inline bool sdReadConfig(CloudConfig* cloudCfg, WifiNetwork* networks, int* networkCount) {
    #ifndef FEATURE_SD_CARD
    return false;
    #endif

    if (!SD.exists(CONFIG_FILENAME)) {
        Serial.println(F("[SD] No config.json found"));
        return false;
    }

    File f = SD.open(CONFIG_FILENAME, FILE_READ);
    if (!f) {
        Serial.println(F("[SD] Failed to open config.json"));
        return false;
    }

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.print(F("[SD] JSON parse error: "));
        Serial.println(err.c_str());
        return false;
    }

    // Read cloud config
    const char* url = doc["cloud_url"] | DEFAULT_CLOUD_URL;
    strlcpy(cloudCfg->cloud_url, url, sizeof(cloudCfg->cloud_url));

    const char* token = doc["device_token"] | "";
    strlcpy(cloudCfg->device_token, token, sizeof(cloudCfg->device_token));

    const char* devId = doc["device_id"] | "";
    strlcpy(cloudCfg->device_id, devId, sizeof(cloudCfg->device_id));

    // Config is valid if we have at least a token
    cloudCfg->configured = (strlen(cloudCfg->device_token) > 0);

    Serial.printf("[SD] Cloud URL: %s\n", cloudCfg->cloud_url);
    Serial.printf("[SD] Device ID: %.8s...\n", cloudCfg->device_id);
    Serial.printf("[SD] Token: %.12s...\n", cloudCfg->device_token);

    // Read WiFi networks (optional array)
    *networkCount = 0;
    if (doc.containsKey("wifi")) {
        JsonArray wifiArr = doc["wifi"].as<JsonArray>();
        for (JsonVariant net : wifiArr) {
            if (*networkCount >= MAX_WIFI_NETWORKS) break;

            const char* ssid = net["ssid"] | "";
            const char* pass = net["pass"] | "";

            if (strlen(ssid) > 0) {
                strlcpy(networks[*networkCount].ssid, ssid, sizeof(networks[0].ssid));
                strlcpy(networks[*networkCount].pass, pass, sizeof(networks[0].pass));
                Serial.printf("[SD] WiFi %d: %s\n", *networkCount + 1, ssid);
                (*networkCount)++;
            }
        }
    }

    if (*networkCount == 0) {
        Serial.println(F("[SD] No WiFi networks in config, using defaults"));
    }

    return cloudCfg->configured;
}

// ============================================================================
// LITTLEFS CONFIG BACKUP
// ============================================================================

inline void sdSaveConfigToLittleFS(CloudConfig* cfg) {
    #if USE_LITTLEFS
    StaticJsonDocument<256> doc;
    doc["cloud_url"] = cfg->cloud_url;
    doc["device_token"] = cfg->device_token;
    doc["device_id"] = cfg->device_id;
    doc["configured"] = cfg->configured;

    File f = LittleFS.open(CLOUD_CONFIG_FILE, "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
        Serial.println(F("[SD] Config backed up to LittleFS"));
    } else {
        Serial.println(F("[SD] Failed to backup config to LittleFS"));
    }
    #endif
}

inline bool sdLoadConfigFromLittleFS(CloudConfig* cfg) {
    #if USE_LITTLEFS
    if (!LittleFS.exists(CLOUD_CONFIG_FILE)) {
        Serial.println(F("[SD] No cached config in LittleFS"));
        return false;
    }

    File f = LittleFS.open(CLOUD_CONFIG_FILE, "r");
    if (!f) return false;

    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) return false;

    const char* url = doc["cloud_url"] | DEFAULT_CLOUD_URL;
    strlcpy(cfg->cloud_url, url, sizeof(cfg->cloud_url));

    const char* token = doc["device_token"] | "";
    strlcpy(cfg->device_token, token, sizeof(cfg->device_token));

    const char* devId = doc["device_id"] | "";
    strlcpy(cfg->device_id, devId, sizeof(cfg->device_id));

    cfg->configured = doc["configured"] | false;

    if (cfg->configured) {
        Serial.println(F("[SD] Loaded config from LittleFS cache"));
        Serial.printf("[SD] Cloud URL: %s\n", cfg->cloud_url);
    }

    return cfg->configured;
    #else
    return false;
    #endif
}

// ============================================================================
// CHAT HISTORY LOGGING
// ============================================================================

inline bool sdLogChat(const char* agent, const char* message,
                      const char* response, float E) {
    #if !defined(FEATURE_CHAT_LOG) || !defined(FEATURE_SD_CARD)
    return false;
    #endif

    // Ensure history directory exists
    if (!SD.exists(HISTORY_DIR)) {
        SD.mkdir(HISTORY_DIR);
    }

    // Build filename: /history/YYYY-MM-DD.txt
    // Without RTC, use millis-based day counter from boot
    // (Real timestamps would need NTP, which we add later)
    unsigned long days = millis() / 86400000UL;
    char filename[32];
    snprintf(filename, sizeof(filename), "%s/day_%04lu.txt", HISTORY_DIR, days);

    // Check file size - truncate if over limit
    if (SD.exists(filename)) {
        File check = SD.open(filename, FILE_READ);
        if (check) {
            size_t sz = check.size();
            check.close();
            if (sz > MAX_HISTORY_FILE_KB * 1024) {
                // File too large, start a new one
                char newName[40];
                snprintf(newName, sizeof(newName), "%s/day_%04lu_old.txt", HISTORY_DIR, days);
                SD.rename(filename, newName);
            }
        }
    }

    File f = SD.open(filename, FILE_APPEND);
    if (!f) {
        Serial.println(F("[SD] Failed to open history file"));
        return false;
    }

    // Format: [MM:SS] AGENT> User: message | Response: response | E=X.X
    unsigned long secs = (millis() / 1000) % 86400;
    unsigned long mins = secs / 60;
    secs = secs % 60;

    f.printf("[%02lu:%02lu] %s> User: ", mins % 60, secs, agent);
    f.print(message);
    f.print(" | Response: ");
    f.print(response);
    f.printf(" | E=%.1f\n", E);
    f.close();

    return true;
}

#endif // SDCONFIG_H
