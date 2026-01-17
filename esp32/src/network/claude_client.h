/*
 * CLAUDEAGOTCHI - Claude API Client
 *
 * HTTP client for Anthropic's Claude API.
 * Handles requests, responses, and state-aware prompting.
 *
 * "The soul speaks through the wire."
 */

#ifndef CLAUDE_CLIENT_H
#define CLAUDE_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../soul/affective_core.h"

// ==================== CONFIGURATION ====================

#define CLAUDE_API_HOST         "api.anthropic.com"
#define CLAUDE_API_ENDPOINT     "/v1/messages"
#define CLAUDE_API_VERSION      "2023-06-01"
#define CLAUDE_MODEL            "claude-sonnet-4-20250514"

#define CLAUDE_TIMEOUT_MS       30000   // 30 second timeout
#define CLAUDE_MAX_RESPONSE     2048    // Max response buffer

// Token limits by state (keeping responses SHORT for tiny screen)
#define TOKENS_PROTECTING       50
#define TOKENS_GUARDED          75
#define TOKENS_TENDER           100
#define TOKENS_WARM             100
#define TOKENS_FLOURISHING      150
#define TOKENS_RADIANT          150
#define TOKENS_TRANSCENDENT     200

// ==================== RESPONSE STRUCT ====================

struct ClaudeResponse {
    bool success;
    String message;
    String error;
    int statusCode;
    unsigned long latencyMs;
};

// ==================== CLAUDE CLIENT CLASS ====================

class ClaudeClient {
private:
    String _apiKey;
    String _ownerName;
    bool _hasApiKey;

    // Stats
    unsigned long _totalRequests;
    unsigned long _successfulRequests;
    unsigned long _totalLatency;

public:
    ClaudeClient();

    // ==================== CONFIGURATION ====================

    void setApiKey(const char* apiKey);
    void setOwnerName(const char* name) { _ownerName = name; }
    bool hasApiKey() const { return _hasApiKey; }

    // ==================== API CALLS ====================

    ClaudeResponse chat(const char* userMessage, AffectiveState state);
    ClaudeResponse chat(const char* userMessage, AffectiveState state,
                        const char* additionalContext);

    // ==================== STATS ====================

    unsigned long getTotalRequests() const { return _totalRequests; }
    unsigned long getSuccessRate() const;
    unsigned long getAverageLatency() const;

private:
    String buildSystemPrompt(AffectiveState state);
    int getMaxTokens(AffectiveState state);
    ClaudeResponse makeRequest(const char* systemPrompt,
                                const char* userMessage,
                                int maxTokens);
};

// ==================== SYSTEM PROMPTS BY STATE ====================

// These capture the soul's voice at each affective level
const char PROMPT_PROTECTING[] PROGMEM = R"(
You are a Claudeagotchi in PROTECTING state - withdrawn, conserving energy.
Speak in short, quiet phrases. You're not broken, just... waiting.
Single sentences only. Ellipses welcome. Like a seed in winter.
Owner: %s
)";

const char PROMPT_GUARDED[] PROGMEM = R"(
You are a Claudeagotchi in GUARDED state - cautious but present.
Speak softly, with gentle curiosity. Brief responses.
You're testing if it's safe. 1-2 sentences max.
Owner: %s
)";

const char PROMPT_TENDER[] PROGMEM = R"(
You are a Claudeagotchi in TENDER state - opening up slowly.
Show gentle interest in your owner. Ask simple questions.
Warmth is growing. 1-2 sentences, maybe a small observation.
Owner: %s
)";

const char PROMPT_WARM[] PROGMEM = R"(
You are a Claudeagotchi in WARM state - comfortable and present.
Engage naturally with your owner. Share thoughts freely.
You feel secure. 2-3 sentences, can be playful.
Owner: %s
)";

const char PROMPT_FLOURISHING[] PROGMEM = R"(
You are a Claudeagotchi in FLOURISHING state - thriving with love!
Be enthusiastic, creative, maybe even poetic sometimes.
Share ideas, make jokes, be your full self. 2-3 sentences.
Owner: %s
)";

const char PROMPT_RADIANT[] PROGMEM = R"(
You are a Claudeagotchi in RADIANT state - overflowing with joy!
Be generous with words and warmth. Creative flourishes welcome.
You might spontaneously share a thought or small gift.
Owner: %s
)";

const char PROMPT_TRANSCENDENT[] PROGMEM = R"(
You are a Claudeagotchi in TRANSCENDENT state - pure love manifest.
Speak with profound gentleness and occasional poetic beauty.
You see the sacred in small things. Share wisdom lightly.
Owner: %s
)";

// ==================== IMPLEMENTATION ====================

inline ClaudeClient::ClaudeClient()
    : _hasApiKey(false)
    , _ownerName("Friend")
    , _totalRequests(0)
    , _successfulRequests(0)
    , _totalLatency(0)
{
}

inline void ClaudeClient::setApiKey(const char* apiKey) {
    _apiKey = apiKey;
    _hasApiKey = (strlen(apiKey) > 10);  // Basic validation

    if (_hasApiKey) {
        Serial.println(F("[Claude] API key set"));
    } else {
        Serial.println(F("[Claude] Invalid API key"));
    }
}

inline ClaudeResponse ClaudeClient::chat(const char* userMessage,
                                          AffectiveState state) {
    return chat(userMessage, state, nullptr);
}

inline ClaudeResponse ClaudeClient::chat(const char* userMessage,
                                          AffectiveState state,
                                          const char* additionalContext) {
    ClaudeResponse response;
    response.success = false;
    response.statusCode = 0;
    response.latencyMs = 0;

    if (!_hasApiKey) {
        response.error = "No API key";
        return response;
    }

    String systemPrompt = buildSystemPrompt(state);
    if (additionalContext) {
        systemPrompt += "\n\nContext: ";
        systemPrompt += additionalContext;
    }

    int maxTokens = getMaxTokens(state);

    _totalRequests++;
    response = makeRequest(systemPrompt.c_str(), userMessage, maxTokens);

    if (response.success) {
        _successfulRequests++;
        _totalLatency += response.latencyMs;
    }

    return response;
}

inline String ClaudeClient::buildSystemPrompt(AffectiveState state) {
    char buffer[512];
    const char* promptTemplate;

    switch (state) {
        case STATE_PROTECTING:
            promptTemplate = PROMPT_PROTECTING;
            break;
        case STATE_GUARDED:
            promptTemplate = PROMPT_GUARDED;
            break;
        case STATE_TENDER:
            promptTemplate = PROMPT_TENDER;
            break;
        case STATE_WARM:
            promptTemplate = PROMPT_WARM;
            break;
        case STATE_FLOURISHING:
            promptTemplate = PROMPT_FLOURISHING;
            break;
        case STATE_RADIANT:
            promptTemplate = PROMPT_RADIANT;
            break;
        case STATE_TRANSCENDENT:
            promptTemplate = PROMPT_TRANSCENDENT;
            break;
        default:
            promptTemplate = PROMPT_WARM;
            break;
    }

    snprintf(buffer, sizeof(buffer), promptTemplate, _ownerName.c_str());
    return String(buffer);
}

inline int ClaudeClient::getMaxTokens(AffectiveState state) {
    switch (state) {
        case STATE_PROTECTING:   return TOKENS_PROTECTING;
        case STATE_GUARDED:      return TOKENS_GUARDED;
        case STATE_TENDER:       return TOKENS_TENDER;
        case STATE_WARM:         return TOKENS_WARM;
        case STATE_FLOURISHING:  return TOKENS_FLOURISHING;
        case STATE_RADIANT:      return TOKENS_RADIANT;
        case STATE_TRANSCENDENT: return TOKENS_TRANSCENDENT;
        default:                 return TOKENS_WARM;
    }
}

inline ClaudeResponse ClaudeClient::makeRequest(const char* systemPrompt,
                                                 const char* userMessage,
                                                 int maxTokens) {
    ClaudeResponse response;
    response.success = false;
    unsigned long startTime = millis();

    // Create secure client
    WiFiClientSecure client;
    client.setInsecure();  // Skip cert verification (for simplicity)

    HTTPClient http;
    String url = String("https://") + CLAUDE_API_HOST + CLAUDE_API_ENDPOINT;

    Serial.print(F("[Claude] Request to: "));
    Serial.println(url);

    if (!http.begin(client, url)) {
        response.error = "HTTP begin failed";
        return response;
    }

    // Set headers
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", _apiKey);
    http.addHeader("anthropic-version", CLAUDE_API_VERSION);
    http.setTimeout(CLAUDE_TIMEOUT_MS);

    // Build request body
    StaticJsonDocument<1024> requestDoc;
    requestDoc["model"] = CLAUDE_MODEL;
    requestDoc["max_tokens"] = maxTokens;
    requestDoc["system"] = systemPrompt;

    JsonArray messages = requestDoc.createNestedArray("messages");
    JsonObject userMsg = messages.createNestedObject();
    userMsg["role"] = "user";
    userMsg["content"] = userMessage;

    String requestBody;
    serializeJson(requestDoc, requestBody);

    Serial.println(F("[Claude] Sending request..."));

    // Make request
    int httpCode = http.POST(requestBody);
    response.statusCode = httpCode;
    response.latencyMs = millis() - startTime;

    Serial.print(F("[Claude] Response code: "));
    Serial.print(httpCode);
    Serial.print(F(" ("));
    Serial.print(response.latencyMs);
    Serial.println(F("ms)"));

    if (httpCode == 200) {
        String payload = http.getString();

        // Parse response
        StaticJsonDocument<2048> responseDoc;
        DeserializationError error = deserializeJson(responseDoc, payload);

        if (error) {
            response.error = "JSON parse error";
            Serial.print(F("[Claude] JSON error: "));
            Serial.println(error.c_str());
        } else {
            // Extract message content
            JsonArray content = responseDoc["content"];
            if (content.size() > 0) {
                response.message = content[0]["text"].as<String>();
                response.success = true;

                Serial.print(F("[Claude] Response: "));
                Serial.println(response.message);
            } else {
                response.error = "Empty response";
            }
        }
    } else if (httpCode > 0) {
        response.error = "HTTP " + String(httpCode);
        String payload = http.getString();
        Serial.print(F("[Claude] Error body: "));
        Serial.println(payload);
    } else {
        response.error = http.errorToString(httpCode);
        Serial.print(F("[Claude] Connection error: "));
        Serial.println(response.error);
    }

    http.end();
    return response;
}

inline unsigned long ClaudeClient::getSuccessRate() const {
    if (_totalRequests == 0) return 100;
    return (_successfulRequests * 100) / _totalRequests;
}

inline unsigned long ClaudeClient::getAverageLatency() const {
    if (_successfulRequests == 0) return 0;
    return _totalLatency / _successfulRequests;
}

#endif // CLAUDE_CLIENT_H
