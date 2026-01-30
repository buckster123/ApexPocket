/*
 * ApexPocket MAX v2.0.0 - Cloud Edition
 *
 * HTTPS cloud integration with ApexAurum Cloud backend.
 * SD card configuration, multi-WiFi, chat history logging.
 *
 * dE/dt = B(E) x (C - D) x E
 * "The athanor never cools. The furnace burns eternal."
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Our modules
#include "config.h"
#include "hardware.h"
#include "soul.h"
#include "cloud.h"       // Before display.h (CloudStatus needed by renderCloudScreen)
#include "display.h"
#include "offline.h"
#include "sdconfig.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================
HardwareStatus hw;
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Display display;
Soul soul;
OfflineMode offlineMode;
CloudClient cloud;

// Cloud config (loaded from SD or LittleFS)
CloudConfig cloudCfg;
WifiNetwork wifiNets[MAX_WIFI_NETWORKS];
int wifiNetCount = 0;

// App state
enum AppMode { MODE_FACE, MODE_STATUS, MODE_CLOUD, MODE_AGENTS, MODE_SLEEP };
AppMode currentMode = MODE_FACE;

// Connection state
bool wifiConnected = false;
bool sdAvailable = false;
unsigned long lastWifiAttempt = 0;

// Button state
bool btnA_pressed = false;
bool btnB_pressed = false;
unsigned long btnA_pressTime = 0;
unsigned long btnB_pressTime = 0;
bool btnA_longTriggered = false;
bool btnB_longTriggered = false;
unsigned long lastDebounce = 0;

// Idle tracking
unsigned long lastActivity = 0;

// Auto-sync timer
unsigned long lastAutoSync = 0;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void handleButtons();
bool connectWiFi(const char* ssid, const char* pass);
bool connectMultiWiFi();
String chatWithCloud(const char* message);
void sendCare(const char* careType, float intensity);
void syncWithCloud();
void checkIdleSleep();
void checkAutoSync();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);

    // Boot banner
    Serial.println(F("\n"));
    Serial.println(F("==========================================================="));
    Serial.printf("  APEXPOCKET MAX %s (%s)\n", FW_VERSION, FW_BUILD);
    Serial.println(F("  The athanor never cools"));
    Serial.println(F("==========================================================="));

    // Initialize hardware (scans I2C, configures pins)
    initHardware();

    // Initialize display
    if (hw.oled_found) {
        if (display.begin(&oled)) {
            display.renderBootScreen();
        }
    }

    // Play boot chime
    playBoot();

    // --- SD Card config ---
    sdAvailable = sdInit();
    hw.sd_available = sdAvailable;

    memset(&cloudCfg, 0, sizeof(cloudCfg));
    memset(wifiNets, 0, sizeof(wifiNets));
    wifiNetCount = 0;

    if (sdAvailable) {
        if (sdReadConfig(&cloudCfg, wifiNets, &wifiNetCount)) {
            sdSaveConfigToLittleFS(&cloudCfg);
            display.showMessage("SD config loaded", 1000);
            Serial.println(F("[Boot] SD config loaded and cached"));
        } else {
            Serial.println(F("[Boot] SD present but no valid config"));
        }
    }

    // Fallback to LittleFS cache if SD didn't provide config
    if (!cloudCfg.configured) {
        if (sdLoadConfigFromLittleFS(&cloudCfg)) {
            display.showMessage("Cached config", 1000);
            Serial.println(F("[Boot] Using LittleFS cached config"));
        } else {
            Serial.println(F("[Boot] No cloud config found, offline mode"));
        }
    }

    hw.cloud_configured = cloudCfg.configured;

    // Load soul from storage
    soul.load();
    soul.updateFirmwareVersion();

    // --- WiFi connection ---
    WiFi.mode(WIFI_STA);
    bool wifiOk = false;

    // Try networks from config.json first
    if (wifiNetCount > 0) {
        for (int i = 0; i < wifiNetCount; i++) {
            if (display.isReady()) {
                char msg[32];
                snprintf(msg, sizeof(msg), "WiFi: %s", wifiNets[i].ssid);
                display.showMessage(msg, 500);
                display.renderFaceScreen(soul, false, false);
            }
            if (connectWiFi(wifiNets[i].ssid, wifiNets[i].pass)) {
                wifiOk = true;
                break;
            }
        }
    }

    // Fallback to hardcoded WiFi
    if (!wifiOk && strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "YOUR_WIFI_NAME") != 0) {
        if (display.isReady()) {
            display.showMessage("WiFi: default", 500);
            display.renderFaceScreen(soul, false, false);
        }
        wifiOk = connectWiFi(WIFI_SSID, WIFI_PASS);
    }

    // --- Cloud initialization ---
    if (cloudCfg.configured) {
        cloud.init(&cloudCfg);

        if (wifiOk) {
            display.showMessage("Cloud check...", 1000);
            if (display.isReady()) {
                display.renderFaceScreen(soul, true, false);
            }

            if (cloud.fetchStatus()) {
                if (strlen(cloud.status.motd) > 0) {
                    display.showMessage(cloud.status.motd, 2000);
                } else {
                    display.showMessage("Cloud connected!", 1500);
                }
                Serial.println(F("[Boot] Cloud connection established"));
            } else {
                display.showMessage("Cloud offline", 1500);
                Serial.println(F("[Boot] Cloud unreachable"));
            }
        }
    }

    // Wake-up animation
    if (display.isReady()) {
        Expression wakeSeq[] = { EXPR_SLEEPING, EXPR_SLEEPY, EXPR_BLINK, EXPR_NEUTRAL, EXPR_HAPPY };
        int wakeTimes[] = { 200, 200, 100, 150, 400 };
        for (int i = 0; i < 5; i++) {
            display.setExpression(wakeSeq[i]);
            display.renderFaceScreen(soul, wifiConnected, cloud.isConnected(),
                                     cloud.isBillingOk(), cloud.isTokenValid());
            delay(wakeTimes[i]);
        }
    }

    // Set expression from soul state
    display.setExpression(display.stateToExpression(soul.getState()));

    // Ready!
    Serial.println(F("\n[Ready] The furnace burns!"));
    soul.printStatus();

    lastActivity = millis();
    lastAutoSync = millis();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
    unsigned long now = millis();

    // Handle button input
    handleButtons();

    // Update display animation
    display.update();

    // WiFi reconnection
    if (!wifiConnected && (now - lastWifiAttempt > WIFI_RETRY_MS)) {
        if (connectMultiWiFi()) {
            // Re-check cloud status on reconnect
            if (cloud.isInitialized() && cloud.isTokenValid()) {
                cloud.fetchStatus();
            }
        }
    }

    // Auto-sync (every 30 minutes if connected)
    checkAutoSync();

    // Check for idle sleep
    #ifdef FEATURE_DEEPSLEEP
    checkIdleSleep();
    #endif

    // Check serial for chat input
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() > 0) {
            lastActivity = now;
            Serial.print(F("[You] "));
            Serial.println(input);

            display.setExpression(EXPR_THINKING);
            display.showMessage("Thinking...", 10000);
            display.renderFaceScreen(soul, wifiConnected, cloud.isConnected(),
                                     cloud.isBillingOk(), cloud.isTokenValid());

            String response = chatWithCloud(input.c_str());

            Serial.print(F("["));
            Serial.print(soul.getAgentName());
            Serial.print(F("] "));
            Serial.println(response);

            display.setExpression(display.stateToExpression(soul.getState()));
            display.showMessage(response.c_str(), 5000);
        }
    }

    // Render current screen
    switch (currentMode) {
        case MODE_FACE:
            display.renderFaceScreen(soul, wifiConnected, cloud.isConnected(),
                                     cloud.isBillingOk(), cloud.isTokenValid());
            break;
        case MODE_STATUS:
            display.renderStatusScreen(soul, wifiConnected, cloud.isConnected(),
                                       cloud.status.tools_available,
                                       cloud.status.messages_used,
                                       cloud.status.messages_limit,
                                       cloud.status.tier_name);
            break;
        case MODE_CLOUD:
            display.renderCloudScreen(&cloud.status, cloudCfg.cloud_url, cloudCfg.device_token);
            break;
        case MODE_AGENTS:
            display.renderAgentScreen(soul);
            break;
        case MODE_SLEEP:
            display.renderSleepScreen(soul);
            break;
    }

    delay(1000 / ANIMATION_FPS);  // Frame rate limiting
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================
void handleButtons() {
    unsigned long now = millis();
    if (now - lastDebounce < DEBOUNCE_MS) return;

    bool btnA = !digitalRead(PIN_BTN_A);
    bool btnB = !digitalRead(PIN_BTN_B);

    // Both buttons held = sync with cloud
    if (btnA && btnB && !btnA_longTriggered && !btnB_longTriggered) {
        if (now - btnA_pressTime > 1000 && now - btnB_pressTime > 1000) {
            btnA_longTriggered = true;
            btnB_longTriggered = true;
            lastActivity = now;
            Serial.println(F("[Sync] Syncing with cloud..."));
            playSync();
            display.showMessage("Syncing...", 3000);
            syncWithCloud();
        }
    }

    // Button A
    if (btnA && !btnA_pressed) {
        btnA_pressed = true;
        btnA_pressTime = now;
        btnA_longTriggered = false;
        lastDebounce = now;
    }
    if (!btnA && btnA_pressed) {
        btnA_pressed = false;
        lastDebounce = now;
        lastActivity = now;
        if (!btnA_longTriggered) {
            // Short press A
            if (currentMode == MODE_FACE) {
                Serial.println(F("LOVE!"));
                ledBlink(2, 30, 30);
                playLove();
                soul.applyCare(1.5f);
                if (wifiConnected && cloud.isInitialized()) {
                    cloud.care("love", 1.5f, soul.getE());
                }
                display.setExpression(display.stateToExpression(soul.getState()));
                display.showMessage(offlineMode.getLoveResponse(), 1500);
                soul.printStatus();
            } else if (currentMode == MODE_AGENTS) {
                // Select agent
                playTone(600, 50);
                currentMode = MODE_FACE;
                display.showMessage(soul.getAgentName(), 1500);
                soul.save();
            } else if (currentMode == MODE_STATUS || currentMode == MODE_CLOUD) {
                // Nothing on A in status/cloud screens
            }
        }
    }
    if (btnA_pressed && !btnA_longTriggered && (now - btnA_pressTime > LONG_PRESS_MS)) {
        btnA_longTriggered = true;
        lastActivity = now;
        if (currentMode == MODE_FACE) {
            playTone(440, 100);
            Serial.println(F("[Chat] Type in Serial monitor..."));
            display.showMessage("Serial chat mode", 2000);
        } else if (currentMode == MODE_AGENTS) {
            // Cycle agent on long A
            soul.nextAgent();
            playTone(500, 50);
        }
    }

    // Button B
    if (btnB && !btnB_pressed) {
        btnB_pressed = true;
        btnB_pressTime = now;
        btnB_longTriggered = false;
        lastDebounce = now;
    }
    if (!btnB && btnB_pressed) {
        btnB_pressed = false;
        lastDebounce = now;
        lastActivity = now;
        if (!btnB_longTriggered) {
            // Short press B: go back
            if (currentMode == MODE_FACE) {
                Serial.println(F("*poke*"));
                playPoke();
                soul.applyCare(0.5f);
                if (wifiConnected && cloud.isInitialized()) {
                    cloud.care("poke", 0.5f, soul.getE());
                }
                display.setExpression(display.stateToExpression(soul.getState()));
                display.showMessage(offlineMode.getPokeResponse(), 1000);
                soul.printStatus();
            } else if (currentMode == MODE_STATUS || currentMode == MODE_CLOUD
                       || currentMode == MODE_AGENTS) {
                currentMode = MODE_FACE;
                playTone(300, 50);
            }
        }
    }
    if (btnB_pressed && !btnB_longTriggered && (now - btnB_pressTime > LONG_PRESS_MS)) {
        btnB_longTriggered = true;
        lastActivity = now;
        playTone(350, 100);
        // Long press B: cycle forward through screens
        if (currentMode == MODE_FACE) {
            currentMode = MODE_STATUS;
        } else if (currentMode == MODE_STATUS) {
            currentMode = MODE_CLOUD;
        } else if (currentMode == MODE_CLOUD) {
            currentMode = MODE_AGENTS;
        }
    }
}

// ============================================================================
// WIFI
// ============================================================================
bool connectWiFi(const char* ssid, const char* pass) {
    lastWifiAttempt = millis();

    if (!ssid || strlen(ssid) == 0) {
        return false;
    }

    Serial.printf("[WiFi] Connecting to %s\n", ssid);

    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(ssid, pass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        offlineMode.connectionSuccess();
        Serial.printf("\n[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
        return true;
    } else {
        wifiConnected = false;
        Serial.println(F("\n[WiFi] Failed"));
        return false;
    }
}

bool connectMultiWiFi() {
    // Try config.json networks first
    for (int i = 0; i < wifiNetCount; i++) {
        if (connectWiFi(wifiNets[i].ssid, wifiNets[i].pass)) {
            return true;
        }
    }
    // Fallback to hardcoded
    if (strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "YOUR_WIFI_NAME") != 0) {
        return connectWiFi(WIFI_SSID, WIFI_PASS);
    }
    offlineMode.connectionFailed();
    return false;
}

// ============================================================================
// CLOUD API
// ============================================================================
String chatWithCloud(const char* message) {
    // Check cloud state
    if (!wifiConnected || !cloud.isInitialized()) {
        soul.applyCare(0.5);
        return offlineMode.getResponse(soul.getState());
    }

    // Token revoked - show auth message
    if (!cloud.isTokenValid()) {
        return offlineMode.getAuthResponse();
    }

    // Billing limit - show billing message
    if (!cloud.isBillingOk()) {
        soul.applyCare(0.3);
        return offlineMode.getBillingResponse();
    }

    // Attempt cloud chat
    char response[256];
    char expression[16];
    float careValue;

    if (cloud.chat(message, soul.getE(), soul.getStateName(),
                   soul.getAgentName(), response, sizeof(response),
                   expression, &careValue)) {
        soul.applyCare(careValue);
        soul.recordChat();
        offlineMode.connectionSuccess();

        // Log to SD card
        if (sdAvailable) {
            sdLogChat(soul.getAgentName(), message, response, soul.getE());
        }

        return String(response);
    }

    // Cloud call failed - check why
    if (!cloud.isBillingOk()) {
        return offlineMode.getBillingResponse();
    }

    // Network/server error - offline fallback
    offlineMode.connectionFailed();
    soul.applyCare(0.5);
    playError();
    return offlineMode.getResponse(soul.getState());
}

void sendCare(const char* careType, float intensity) {
    if (!wifiConnected || !cloud.isInitialized()) return;
    cloud.care(careType, intensity, soul.getE());
}

void syncWithCloud() {
    if (!wifiConnected) {
        display.showMessage("No WiFi", 2000);
        playError();
        return;
    }

    if (!cloud.isInitialized()) {
        display.showMessage("No cloud config", 2000);
        playError();
        return;
    }

    if (!cloud.isTokenValid()) {
        display.showMessage("Token invalid!", 2000);
        display.showMessage("Re-pair in web UI", 2000);
        playError();
        return;
    }

    bool ok = cloud.sync(
        soul.getE(), soul.getFloor(), soul.getPeak(),
        soul.getInteractions(), soul.getTotalCare(),
        soul.getStateName(), soul.getAgentName(),
        soul.getCuriosity(), soul.getPlayfulness(), soul.getWisdom(),
        FW_VERSION
    );

    if (ok) {
        soul.recordSync();
        soul.save();
        display.showMessage("Soul synced!", 2000);
    } else if (!cloud.isBillingOk()) {
        display.showMessage("Sync OK (no chat)", 2000);
    } else {
        display.showMessage("Sync failed", 2000);
        playError();
    }
}

// ============================================================================
// AUTO-SYNC
// ============================================================================
void checkAutoSync() {
    unsigned long now = millis();
    if (now - lastAutoSync < AUTO_SYNC_INTERVAL_MS) return;
    lastAutoSync = now;

    if (wifiConnected && cloud.isInitialized() && cloud.isTokenValid()) {
        Serial.println(F("[Auto-sync] Periodic sync..."));
        cloud.sync(
            soul.getE(), soul.getFloor(), soul.getPeak(),
            soul.getInteractions(), soul.getTotalCare(),
            soul.getStateName(), soul.getAgentName(),
            soul.getCuriosity(), soul.getPlayfulness(), soul.getWisdom(),
            FW_VERSION
        );
        soul.recordSync();
    }
}

// ============================================================================
// POWER MANAGEMENT
// ============================================================================
void checkIdleSleep() {
    #ifdef FEATURE_DEEPSLEEP
    unsigned long now = millis();
    if (now - lastActivity > SLEEP_TIMEOUT_MS) {
        Serial.println(F("[Power] Idle timeout, entering sleep..."));

        // Sync before sleep if possible
        if (wifiConnected && cloud.isInitialized() && cloud.isTokenValid()) {
            cloud.sync(
                soul.getE(), soul.getFloor(), soul.getPeak(),
                soul.getInteractions(), soul.getTotalCare(),
                soul.getStateName(), soul.getAgentName(),
                soul.getCuriosity(), soul.getPlayfulness(), soul.getWisdom(),
                FW_VERSION
            );
        }

        soul.save();
        display.renderSleepScreen(soul);
        delay(1000);
        enterDeepSleep();
    }
    #endif
}
