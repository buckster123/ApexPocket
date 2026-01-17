/*
 * CLAUDEAGOTCHI - WiFi Manager
 *
 * Handles WiFi connection, reconnection, and status tracking.
 * Designed for reliability - the soul needs its connection!
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

// ==================== CONFIGURATION ====================

#define WIFI_CONNECT_TIMEOUT_MS     15000   // 15 seconds to connect
#define WIFI_RECONNECT_INTERVAL_MS  30000   // Try reconnect every 30s
#define WIFI_MAX_RETRIES            3       // Retries before giving up (per cycle)

// ==================== CONNECTION STATE ====================

enum WiFiState {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_FAILED,
    WIFI_STATE_NO_CREDENTIALS
};

// ==================== WIFI MANAGER CLASS ====================

class WiFiManager {
private:
    // Credentials
    String _ssid;
    String _password;
    bool _hasCredentials;

    // State
    WiFiState _state;
    unsigned long _lastConnectAttempt;
    unsigned long _connectedSince;
    uint8_t _retryCount;

    // Callbacks
    void (*_onConnected)();
    void (*_onDisconnected)();

public:
    WiFiManager();

    // ==================== LIFECYCLE ====================

    void begin();
    void update();  // Call in main loop

    // ==================== CREDENTIALS ====================

    void setCredentials(const char* ssid, const char* password);
    void clearCredentials();
    bool hasCredentials() const { return _hasCredentials; }
    const char* getSSID() const { return _ssid.c_str(); }

    // ==================== CONNECTION ====================

    bool connect();
    void disconnect();
    bool isConnected() const { return _state == WIFI_STATE_CONNECTED; }
    WiFiState getState() const { return _state; }

    // ==================== STATUS ====================

    int getRSSI() const;
    String getIP() const;
    unsigned long getUptime() const;  // ms since connected
    const char* getStateName() const;

    // ==================== CALLBACKS ====================

    void onConnected(void (*callback)()) { _onConnected = callback; }
    void onDisconnected(void (*callback)()) { _onDisconnected = callback; }

private:
    void checkConnection();
    void attemptReconnect();
};

// ==================== IMPLEMENTATION ====================

inline WiFiManager::WiFiManager()
    : _hasCredentials(false)
    , _state(WIFI_STATE_NO_CREDENTIALS)
    , _lastConnectAttempt(0)
    , _connectedSince(0)
    , _retryCount(0)
    , _onConnected(nullptr)
    , _onDisconnected(nullptr)
{
}

inline void WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    Serial.println(F("[WiFi] Manager initialized"));

    if (_hasCredentials) {
        connect();
    } else {
        Serial.println(F("[WiFi] No credentials set"));
        _state = WIFI_STATE_NO_CREDENTIALS;
    }
}

inline void WiFiManager::update() {
    // Check current connection status
    bool wasConnected = (_state == WIFI_STATE_CONNECTED);
    bool nowConnected = (WiFi.status() == WL_CONNECTED);

    // Handle state transitions
    if (wasConnected && !nowConnected) {
        // Lost connection
        Serial.println(F("[WiFi] Connection lost!"));
        _state = WIFI_STATE_DISCONNECTED;
        if (_onDisconnected) _onDisconnected();
    }
    else if (!wasConnected && nowConnected) {
        // Gained connection
        Serial.print(F("[WiFi] Connected! IP: "));
        Serial.println(WiFi.localIP());
        _state = WIFI_STATE_CONNECTED;
        _connectedSince = millis();
        _retryCount = 0;
        if (_onConnected) _onConnected();
    }

    // Handle reconnection attempts
    if (_state == WIFI_STATE_DISCONNECTED || _state == WIFI_STATE_FAILED) {
        if (_hasCredentials) {
            unsigned long now = millis();
            if (now - _lastConnectAttempt > WIFI_RECONNECT_INTERVAL_MS) {
                attemptReconnect();
            }
        }
    }

    // Update connecting state
    if (_state == WIFI_STATE_CONNECTING) {
        if (millis() - _lastConnectAttempt > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println(F("[WiFi] Connection timeout"));
            _retryCount++;
            if (_retryCount >= WIFI_MAX_RETRIES) {
                Serial.println(F("[WiFi] Max retries reached, will retry later"));
                _state = WIFI_STATE_FAILED;
            } else {
                _state = WIFI_STATE_DISCONNECTED;
            }
        }
    }
}

inline void WiFiManager::setCredentials(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
    _hasCredentials = (strlen(ssid) > 0);

    Serial.print(F("[WiFi] Credentials set for: "));
    Serial.println(_ssid);

    if (_state == WIFI_STATE_NO_CREDENTIALS && _hasCredentials) {
        _state = WIFI_STATE_DISCONNECTED;
    }
}

inline void WiFiManager::clearCredentials() {
    _ssid = "";
    _password = "";
    _hasCredentials = false;
    _state = WIFI_STATE_NO_CREDENTIALS;
    disconnect();
    Serial.println(F("[WiFi] Credentials cleared"));
}

inline bool WiFiManager::connect() {
    if (!_hasCredentials) {
        Serial.println(F("[WiFi] Cannot connect - no credentials"));
        return false;
    }

    if (_state == WIFI_STATE_CONNECTED) {
        return true;  // Already connected
    }

    Serial.print(F("[WiFi] Connecting to: "));
    Serial.println(_ssid);

    WiFi.disconnect(true);
    delay(100);

    WiFi.begin(_ssid.c_str(), _password.c_str());

    _state = WIFI_STATE_CONNECTING;
    _lastConnectAttempt = millis();

    return true;
}

inline void WiFiManager::disconnect() {
    WiFi.disconnect(true);
    _state = _hasCredentials ? WIFI_STATE_DISCONNECTED : WIFI_STATE_NO_CREDENTIALS;
    Serial.println(F("[WiFi] Disconnected"));
}

inline void WiFiManager::attemptReconnect() {
    if (_retryCount >= WIFI_MAX_RETRIES) {
        _retryCount = 0;  // Reset for next cycle
    }

    Serial.print(F("[WiFi] Reconnect attempt "));
    Serial.print(_retryCount + 1);
    Serial.print(F("/"));
    Serial.println(WIFI_MAX_RETRIES);

    connect();
}

inline int WiFiManager::getRSSI() const {
    if (_state != WIFI_STATE_CONNECTED) return 0;
    return WiFi.RSSI();
}

inline String WiFiManager::getIP() const {
    if (_state != WIFI_STATE_CONNECTED) return "0.0.0.0";
    return WiFi.localIP().toString();
}

inline unsigned long WiFiManager::getUptime() const {
    if (_state != WIFI_STATE_CONNECTED) return 0;
    return millis() - _connectedSince;
}

inline const char* WiFiManager::getStateName() const {
    switch (_state) {
        case WIFI_STATE_DISCONNECTED:    return "DISCONNECTED";
        case WIFI_STATE_CONNECTING:      return "CONNECTING";
        case WIFI_STATE_CONNECTED:       return "CONNECTED";
        case WIFI_STATE_FAILED:          return "FAILED";
        case WIFI_STATE_NO_CREDENTIALS:  return "NO_CREDS";
        default:                         return "UNKNOWN";
    }
}

#endif // WIFI_MANAGER_H
