/*
 * Cloud API Client - HTTPS with Bearer Token Auth
 *
 * Handles all communication with ApexAurum Cloud backend.
 * Uses WiFiClientSecure with root CA bundle for TLS validation.
 *
 * Endpoints:
 *   GET  /api/v1/pocket/status  - Check cloud connection & billing
 *   POST /api/v1/pocket/chat    - Send message, receive LLM response
 *   POST /api/v1/pocket/care    - Send care/love/poke events
 *   POST /api/v1/pocket/sync    - Full soul state sync
 *   GET  /api/v1/pocket/agents  - List available agents
 */

#ifndef CLOUD_H
#define CLOUD_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "certs.h"

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct CloudConfig {
    char cloud_url[128];        // Base URL (from config.json)
    char device_token[TOKEN_MAX_LEN]; // apex_dev_... (from config.json)
    char device_id[DEVICE_ID_MAX_LEN]; // UUID (from config.json)
    bool configured;            // Has valid config loaded
};

struct CloudStatus {
    bool connected;             // Last request succeeded
    bool token_valid;           // Not received 401
    bool billing_ok;            // Not received 402
    int consecutive_failures;
    unsigned long last_success;
    unsigned long last_attempt;
    unsigned long backoff_ms;   // Current backoff delay
    int tools_available;
    int messages_used;
    int messages_limit;
    char tier_name[16];
    char motd[80];              // Message of the day
};

struct WifiNetwork {
    char ssid[33];
    char pass[65];
};

// ============================================================================
// CLOUD CLIENT CLASS
// ============================================================================
class CloudClient {
private:
    WiFiClientSecure secureClient;
    CloudConfig* config;
    bool initialized;

    // Build full URL for an endpoint
    String buildUrl(const char* endpoint) {
        return String(config->cloud_url) + API_PREFIX + endpoint;
    }

    // Add auth headers to HTTP client
    void addHeaders(HTTPClient& https) {
        https.addHeader("Content-Type", "application/json");
        https.addHeader("Authorization", String("Bearer ") + config->device_token);
    }

    // Handle HTTP response code, update status
    void handleResponseCode(int code, CloudStatus* status) {
        if (code == 200) {
            status->connected = true;
            status->consecutive_failures = 0;
            status->last_success = millis();
            status->backoff_ms = 0;
        } else if (code == 401) {
            status->token_valid = false;
            Serial.println(F("[Cloud] 401 - Token invalid, device needs re-pairing"));
        } else if (code == 402) {
            status->billing_ok = false;
            Serial.println(F("[Cloud] 402 - Message limit reached"));
        } else if (code >= 500) {
            status->consecutive_failures++;
            applyBackoff(status);
            Serial.printf("[Cloud] %d - Server error (failure #%d)\n", code, status->consecutive_failures);
        } else if (code < 0) {
            // Network error
            status->connected = false;
            status->consecutive_failures++;
            applyBackoff(status);
            Serial.printf("[Cloud] Network error %d (failure #%d)\n", code, status->consecutive_failures);
        } else {
            Serial.printf("[Cloud] Unexpected %d\n", code);
        }
    }

    void applyBackoff(CloudStatus* status) {
        // Exponential backoff: 5s, 10s, 20s, 40s, 60s max
        unsigned long base = API_BACKOFF_BASE_MS;
        for (int i = 1; i < status->consecutive_failures && i < 5; i++) {
            base *= 2;
        }
        status->backoff_ms = min(base, (unsigned long)API_BACKOFF_MAX_MS);
    }

public:
    CloudStatus status;

    CloudClient() : config(nullptr), initialized(false) {
        memset(&status, 0, sizeof(CloudStatus));
        status.token_valid = true;
        status.billing_ok = true;
    }

    void init(CloudConfig* cfg) {
        config = cfg;
        if (!config || !config->configured) {
            Serial.println(F("[Cloud] No config, running offline"));
            initialized = false;
            return;
        }

        secureClient.setCACert(CLOUD_ROOT_CA);
        initialized = true;
        Serial.printf("[Cloud] Initialized for %s\n", config->cloud_url);
        Serial.printf("[Cloud] Device: %s\n", config->device_id);
    }

    bool isInitialized() { return initialized; }
    bool isConnected() { return status.connected; }
    bool isTokenValid() { return status.token_valid; }
    bool isBillingOk() { return status.billing_ok; }

    // Should we attempt a cloud call right now?
    bool shouldAttempt() {
        if (!initialized || !config->configured) return false;
        if (!status.token_valid) return false;  // Don't spam revoked tokens
        if (status.backoff_ms > 0) {
            unsigned long elapsed = millis() - status.last_attempt;
            if (elapsed < status.backoff_ms) return false;
        }
        return true;
    }

    // ========================================================================
    // GET /api/v1/pocket/status
    // ========================================================================
    bool fetchStatus() {
        if (!shouldAttempt()) return false;
        status.last_attempt = millis();

        HTTPClient https;
        String url = buildUrl("/status");

        https.begin(secureClient, url);
        addHeaders(https);
        https.setTimeout(API_TIMEOUT_MS);

        int code = https.GET();
        handleResponseCode(code, &status);

        if (code == 200) {
            String response = https.getString();
            StaticJsonDocument<512> doc;
            if (!deserializeJson(doc, response)) {
                status.tools_available = doc["tools_available"] | 0;
                status.messages_used = doc["messages_used"] | 0;
                status.messages_limit = doc["messages_limit"] | 0;

                const char* tier = doc["tier"] | "unknown";
                strlcpy(status.tier_name, tier, sizeof(status.tier_name));

                const char* motd = doc["motd"] | "";
                strlcpy(status.motd, motd, sizeof(status.motd));

                Serial.printf("[Cloud] Status OK - %d tools, %s tier, %d/%d msgs\n",
                    status.tools_available,
                    status.tier_name,
                    status.messages_used,
                    status.messages_limit);
            }
            https.end();
            return true;
        }

        https.end();
        return false;
    }

    // ========================================================================
    // POST /api/v1/pocket/chat
    // ========================================================================
    bool chat(const char* message, float E, const char* state,
              const char* agent, char* response, int maxLen,
              char* expression, float* careValue) {

        if (!shouldAttempt()) return false;
        if (!status.billing_ok) return false;  // Don't try chat when 402
        status.last_attempt = millis();

        HTTPClient https;
        String url = buildUrl("/chat");

        https.begin(secureClient, url);
        addHeaders(https);
        https.setTimeout(API_TIMEOUT_MS);

        StaticJsonDocument<512> doc;
        doc["message"] = message;
        doc["E"] = E;
        doc["state"] = state;
        doc["device_id"] = config->device_id;
        doc["agent"] = agent;
        doc["firmware"] = FW_VERSION;

        String body;
        serializeJson(doc, body);

        int code = https.POST(body);
        handleResponseCode(code, &status);

        if (code == 200) {
            String resp = https.getString();
            StaticJsonDocument<1024> respDoc;
            if (!deserializeJson(respDoc, resp)) {
                const char* text = respDoc["response"] | "...";
                strlcpy(response, text, maxLen);

                const char* expr = respDoc["expression"] | "neutral";
                strlcpy(expression, expr, 16);

                *careValue = respDoc["care_value"] | 0.5f;

                // Update billing counters if returned
                if (respDoc.containsKey("messages_used")) {
                    status.messages_used = respDoc["messages_used"];
                }
            }
            https.end();
            return true;
        }

        https.end();
        return false;
    }

    // ========================================================================
    // POST /api/v1/pocket/care
    // ========================================================================
    bool care(const char* careType, float intensity, float E) {
        if (!shouldAttempt()) return false;
        status.last_attempt = millis();

        HTTPClient https;
        String url = buildUrl("/care");

        https.begin(secureClient, url);
        addHeaders(https);
        https.setTimeout(5000);  // Care is fire-and-forget, shorter timeout

        StaticJsonDocument<256> doc;
        doc["care_type"] = careType;
        doc["intensity"] = intensity;
        doc["E"] = E;
        doc["device_id"] = config->device_id;

        String body;
        serializeJson(doc, body);

        int code = https.POST(body);
        handleResponseCode(code, &status);
        https.end();

        return (code == 200);
    }

    // ========================================================================
    // POST /api/v1/pocket/sync
    // ========================================================================
    bool sync(float E, float E_floor, float E_peak,
              uint32_t interactions, float totalCare,
              const char* state, const char* agent,
              float curiosity, float playfulness, float wisdom,
              const char* fwVersion) {

        if (!shouldAttempt()) return false;
        status.last_attempt = millis();

        HTTPClient https;
        String url = buildUrl("/sync");

        https.begin(secureClient, url);
        addHeaders(https);
        https.setTimeout(API_TIMEOUT_MS);

        StaticJsonDocument<512> doc;
        doc["E"] = E;
        doc["E_floor"] = E_floor;
        doc["E_peak"] = E_peak;
        doc["interactions"] = interactions;
        doc["total_care"] = totalCare;
        doc["device_id"] = config->device_id;
        doc["state"] = state;
        doc["agent"] = agent;
        doc["curiosity"] = curiosity;
        doc["playfulness"] = playfulness;
        doc["wisdom"] = wisdom;
        doc["firmware"] = fwVersion;

        String body;
        serializeJson(doc, body);

        int code = https.POST(body);
        handleResponseCode(code, &status);

        if (code == 200) {
            String resp = https.getString();
            StaticJsonDocument<256> respDoc;
            if (!deserializeJson(respDoc, resp)) {
                // Server may return updated MOTD or config
                const char* motd = respDoc["motd"] | "";
                if (strlen(motd) > 0) {
                    strlcpy(status.motd, motd, sizeof(status.motd));
                }
            }
            Serial.println(F("[Cloud] Sync OK"));
        }

        https.end();
        return (code == 200);
    }

    // ========================================================================
    // GET /api/v1/pocket/agents
    // ========================================================================
    bool fetchAgents(char agentNames[][16], int* count, int maxAgents) {
        if (!shouldAttempt()) return false;
        status.last_attempt = millis();

        HTTPClient https;
        String url = buildUrl("/agents");

        https.begin(secureClient, url);
        addHeaders(https);
        https.setTimeout(API_TIMEOUT_MS);

        int code = https.GET();
        handleResponseCode(code, &status);

        if (code == 200) {
            String response = https.getString();
            StaticJsonDocument<512> doc;
            if (!deserializeJson(doc, response)) {
                JsonArray agents = doc["agents"].as<JsonArray>();
                *count = 0;
                for (JsonVariant a : agents) {
                    if (*count >= maxAgents) break;
                    strlcpy(agentNames[*count], a.as<const char*>(), 16);
                    (*count)++;
                }
            }
            https.end();
            return true;
        }

        https.end();
        return false;
    }

    // Minutes since last successful cloud contact
    float minutesSinceContact() {
        if (status.last_success == 0) return -1;
        return (millis() - status.last_success) / 60000.0f;
    }
};

#endif // CLOUD_H
