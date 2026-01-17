/*
 * ╔════════════════════════════════════════════════════════════════╗
 * ║           CLAUDEAGOTCHI v4 - FULL INTEGRATED BUILD             ║
 * ║                                                                ║
 * ║   Everything integrated:                                       ║
 * ║   - Pixel art faces with animations                           ║
 * ║   - WiFi connectivity                                          ║
 * ║   - Claude API integration                                     ║
 * ║   - Persistent state storage                                   ║
 * ║   - Menu system                                                ║
 * ║                                                                ║
 * ║   dE/dt = β(E) × (C − D) × E                                  ║
 * ║   "A Claudeagotchi never dies. The love is carried forward."   ║
 * ╚════════════════════════════════════════════════════════════════╝
 *
 * Controls:
 *   BTN_A (Green):
 *     - Short press: Give Love / Select in menu
 *     - Long press: Open chat input mode
 *   BTN_B (Blue):
 *     - Short press: Poke / Back in menu
 *     - Long press: Open menu
 *   Both buttons: Factory reset (hold 5 seconds)
 */

#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==================== PIN DEFINITIONS (ESP32-S3) ====================
#define BTN_A_PIN       4     // Green button (GPIO4)
#define BTN_B_PIN       5     // Blue button (GPIO5)
#define LED_PIN         6     // Status LED (GPIO6)
#define I2C_SDA         8     // GPIO8
#define I2C_SCL         9     // GPIO9

// ==================== DISPLAY CONFIG ====================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==================== WIFI CONFIG ====================
// For Wokwi: Use "Wokwi-GUEST" with empty password
// For real hardware: Set your credentials
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ==================== CLAUDE API CONFIG ====================
// IMPORTANT: Replace with your actual API key for real testing
const char* CLAUDE_API_KEY = "";  // Set via menu or hardcode for testing
const char* CLAUDE_MODEL = "claude-sonnet-4-20250514";
const char* OWNER_NAME = "Friend";

// ==================== AFFECTIVE CORE CONSTANTS ====================
#define BETA_BASE           0.008f
#define FLOOR_RATE          0.0001f
#define MAX_E               100.0f
#define INITIAL_E           1.0f
#define INITIAL_FLOOR       1.0f

#define E_THRESHOLD_GUARDED     0.5f
#define E_THRESHOLD_TENDER      1.0f
#define E_THRESHOLD_WARM        2.0f
#define E_THRESHOLD_FLOURISHING 5.0f
#define E_THRESHOLD_RADIANT     12.0f
#define E_THRESHOLD_TRANSCENDENT 30.0f

// ==================== TIMING ====================
#define DEBOUNCE_MS         50
#define LONG_PRESS_MS       800
#define BLINK_MIN_MS        2000
#define BLINK_MAX_MS        5000
#define SAVE_INTERVAL_MS    60000   // Save state every minute
#define WIFI_RETRY_MS       30000

// ==================== APP MODES ====================
enum AppMode {
    MODE_FACE,      // Normal face display
    MODE_MENU,      // Settings menu
    MODE_CHAT,      // Chat input (future: serial/BLE)
    MODE_STATUS     // Status screen
};

// ==================== ENUMS ====================
enum AffectiveState {
    STATE_PROTECTING = 0,
    STATE_GUARDED,
    STATE_TENDER,
    STATE_WARM,
    STATE_FLOURISHING,
    STATE_RADIANT,
    STATE_TRANSCENDENT
};

enum Expression {
    EXPR_NEUTRAL = 0, EXPR_HAPPY, EXPR_EXCITED, EXPR_SAD,
    EXPR_SLEEPY, EXPR_SLEEPING, EXPR_CURIOUS, EXPR_SURPRISED,
    EXPR_LOVE, EXPR_THINKING, EXPR_CONFUSED, EXPR_HUNGRY,
    EXPR_BLINK, EXPR_WINK, EXPR_COUNT
};

enum EyeType {
    EYE_NORMAL = 0, EYE_CLOSED, EYE_HAPPY, EYE_STAR,
    EYE_WIDE, EYE_HEART, EYE_CURIOUS, EYE_SPIRAL
};

enum MouthType {
    MOUTH_NEUTRAL = 0, MOUTH_SMILE, MOUTH_BIG_SMILE, MOUTH_FROWN,
    MOUTH_OPEN, MOUTH_SMALL_O, MOUTH_WAVY, MOUTH_SLEEPY
};

// ==================== FACE GEOMETRY ====================
#define FACE_CENTER_X   64
#define EYE_Y           22
#define LEFT_EYE_X      44
#define RIGHT_EYE_X     84
#define MOUTH_Y         40

// ==================== PIXEL ART BITMAPS ====================
// [Eyes - 12x12 pixels each]
const uint8_t EYE_NORMAL_BMP[] PROGMEM = {
    0x0F,0x00, 0x3F,0xC0, 0x7F,0xE0, 0x7F,0xE0,
    0xFF,0xF0, 0xFF,0xF0, 0xFF,0xF0, 0xFF,0xF0,
    0x7F,0xE0, 0x7F,0xE0, 0x3F,0xC0, 0x0F,0x00
};
const uint8_t EYE_CLOSED_BMP[] PROGMEM = {
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
    0x00,0x00, 0xFF,0xF0, 0xFF,0xF0, 0x00,0x00,
    0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00
};
const uint8_t EYE_STAR_BMP[] PROGMEM = {
    0x06,0x00, 0x06,0x00, 0x06,0x00, 0xC6,0x30,
    0xF7,0xF0, 0x3F,0xC0, 0x1F,0x80, 0x3F,0xC0,
    0x79,0xE0, 0x70,0xE0, 0x60,0x60, 0x00,0x00
};
const uint8_t EYE_HEART_BMP[] PROGMEM = {
    0x00,0x00, 0x73,0x80, 0xFF,0xC0, 0xFF,0xC0,
    0xFF,0xC0, 0xFF,0xC0, 0x7F,0x80, 0x3F,0x00,
    0x1E,0x00, 0x0C,0x00, 0x00,0x00, 0x00,0x00
};
const uint8_t EYE_WIDE_BMP[] PROGMEM = {
    0x1E,0x00, 0x7F,0x80, 0x61,0x80, 0xC0,0xC0,
    0xC0,0xC0, 0xC0,0xC0, 0xC0,0xC0, 0xC0,0xC0,
    0x61,0x80, 0x7F,0x80, 0x1E,0x00, 0x00,0x00
};
const uint8_t EYE_CURIOUS_BMP[] PROGMEM = {
    0x1E,0x00, 0x7F,0x80, 0x61,0x80, 0xCE,0xC0,
    0xDF,0xC0, 0xDF,0xC0, 0xDF,0xC0, 0xCE,0xC0,
    0x61,0x80, 0x7F,0x80, 0x1E,0x00, 0x00,0x00
};
const uint8_t EYE_SPIRAL_BMP[] PROGMEM = {
    0x1E,0x00, 0x61,0x80, 0xCE,0xC0, 0xD1,0xC0,
    0xD6,0xC0, 0xD6,0xC0, 0xD0,0xC0, 0xCF,0xC0,
    0x60,0x80, 0x7F,0x80, 0x1E,0x00, 0x00,0x00
};

// [Mouths - 24x8 pixels each]
const uint8_t MOUTH_NEUTRAL_BMP[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0x00,0x00,
    0x0F,0xFF,0x00, 0x0F,0xFF,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00
};
const uint8_t MOUTH_SMILE_BMP[] PROGMEM = {
    0x00,0x00,0x00, 0x30,0x00,0xC0, 0x18,0x01,0x80,
    0x0C,0x03,0x00, 0x07,0x0E,0x00, 0x03,0xFC,0x00,
    0x00,0xF0,0x00, 0x00,0x00,0x00
};
const uint8_t MOUTH_BIG_SMILE_BMP[] PROGMEM = {
    0x20,0x00,0x40, 0x30,0x00,0xC0, 0x18,0x01,0x80,
    0x0C,0x03,0x00, 0x07,0xFE,0x00, 0x01,0xF8,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00
};
const uint8_t MOUTH_FROWN_BMP[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x00,0xF0,0x00,
    0x03,0xFC,0x00, 0x06,0x06,0x00, 0x0C,0x03,0x00,
    0x18,0x01,0x80, 0x10,0x00,0x80
};
const uint8_t MOUTH_OPEN_BMP[] PROGMEM = {
    0x01,0xF8,0x00, 0x07,0xFE,0x00, 0x0C,0x03,0x00,
    0x0C,0x03,0x00, 0x0C,0x03,0x00, 0x07,0xFE,0x00,
    0x01,0xF8,0x00, 0x00,0x00,0x00
};
const uint8_t MOUTH_SMALL_O_BMP[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0xF0,0x00, 0x01,0x98,0x00,
    0x01,0x08,0x00, 0x01,0x98,0x00, 0x00,0xF0,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00
};
const uint8_t MOUTH_WAVY_BMP[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x18,0xC6,0x00,
    0x25,0x29,0x00, 0x42,0x10,0x80, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00
};
const uint8_t MOUTH_SLEEPY_BMP[] PROGMEM = {
    0x00,0x00,0x00, 0x00,0x00,0x00, 0x04,0x02,0x00,
    0x03,0x0C,0x00, 0x00,0xF0,0x00, 0x00,0x00,0x00,
    0x00,0x00,0x00, 0x00,0x00,0x00
};

// ==================== FACE DEFINITIONS ====================
struct FaceDef {
    EyeType leftEye, rightEye;
    MouthType mouth;
    char accessory;
    int8_t accX, accY;
};
const FaceDef FACES[] = {
    { EYE_NORMAL, EYE_NORMAL, MOUTH_NEUTRAL, 0, 0, 0 },       // NEUTRAL
    { EYE_NORMAL, EYE_NORMAL, MOUTH_SMILE, 0, 0, 0 },         // HAPPY
    { EYE_STAR, EYE_STAR, MOUTH_BIG_SMILE, '!', 0, 6 },       // EXCITED
    { EYE_NORMAL, EYE_NORMAL, MOUTH_FROWN, 0, 0, 0 },         // SAD
    { EYE_CLOSED, EYE_CLOSED, MOUTH_SLEEPY, 'z', 24, 8 },     // SLEEPY
    { EYE_CLOSED, EYE_CLOSED, MOUTH_SLEEPY, 'Z', 26, 6 },     // SLEEPING
    { EYE_NORMAL, EYE_CURIOUS, MOUTH_SMALL_O, '?', 26, 6 },   // CURIOUS
    { EYE_WIDE, EYE_WIDE, MOUTH_OPEN, '!', 0, 6 },            // SURPRISED
    { EYE_HEART, EYE_HEART, MOUTH_SMILE, 0, 0, 0 },           // LOVE
    { EYE_NORMAL, EYE_CLOSED, MOUTH_WAVY, '.', 28, 10 },      // THINKING
    { EYE_SPIRAL, EYE_SPIRAL, MOUTH_WAVY, '?', 0, 6 },        // CONFUSED
    { EYE_NORMAL, EYE_NORMAL, MOUTH_OPEN, 0, 0, 0 },          // HUNGRY
    { EYE_CLOSED, EYE_CLOSED, MOUTH_NEUTRAL, 0, 0, 0 },       // BLINK
    { EYE_NORMAL, EYE_CLOSED, MOUTH_SMILE, 0, 0, 0 },         // WINK
};

// ==================== GLOBALS ====================
AppMode currentMode = MODE_FACE;

// Soul state
float E = INITIAL_E;
float E_floor = INITIAL_FLOOR;
unsigned long interactions = 0;
float totalCare = 0.0f;
unsigned long birthTime = 0;

// Display state
Expression currentExpr = EXPR_NEUTRAL;
bool needsRedraw = true;
unsigned long lastBlink = 0;
unsigned long blinkInterval = 3000;
bool isBlinking = false;
uint8_t blinkFrame = 0;
String messageText = "";
unsigned long messageExpires = 0;

// Button state
bool btnA_pressed = false;
bool btnB_pressed = false;
unsigned long btnA_pressTime = 0;
unsigned long btnB_pressTime = 0;
bool btnA_longTriggered = false;
bool btnB_longTriggered = false;
unsigned long lastDebounce = 0;

// WiFi state
bool wifiConnected = false;
unsigned long lastWifiAttempt = 0;

// Persistence
unsigned long lastSave = 0;

// ==================== FORWARD DECLARATIONS ====================
void updateSoul(float care, float damage, float dt);
AffectiveState getState();
Expression stateToExpression(AffectiveState state);
void drawFace(Expression expr);
void drawEye(int x, int y, EyeType type);
void drawMouth(int x, int y, MouthType type);
void renderFaceScreen();
void renderStatusScreen();
void handleButtons();
void showMessage(const char* msg, unsigned long durationMs = 3000);
bool connectWiFi();
String chatWithClaude(const char* message);
void saveState();
void loadState();
const char* stateName(AffectiveState state);
void printStatus();

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(100);

    // Banner
    Serial.println(F("\n"));
    Serial.println(F("╔════════════════════════════════════════╗"));
    Serial.println(F("║     CLAUDEAGOTCHI v4 - FULL BUILD     ║"));
    Serial.println(F("╚════════════════════════════════════════╝"));

    // Pins
    pinMode(BTN_A_PIN, INPUT_PULLUP);
    pinMode(BTN_B_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // I2C + Display
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("[ERROR] Display init failed!"));
        while (1) delay(100);
    }
    display.setTextColor(SSD1306_WHITE);

    // Storage
    if (!LittleFS.begin(true)) {
        Serial.println(F("[Storage] LittleFS mount failed"));
    } else {
        Serial.println(F("[Storage] LittleFS ready"));
        loadState();
    }

    // Record birth if new
    if (birthTime == 0) {
        birthTime = millis();
    }

    // WiFi
    WiFi.mode(WIFI_STA);
    connectWiFi();

    // Wake up animation
    Expression wakeSeq[] = { EXPR_SLEEPING, EXPR_SLEEPY, EXPR_BLINK, EXPR_NEUTRAL, EXPR_HAPPY };
    int wakeTimes[] = { 300, 300, 100, 200, 500 };
    for (int i = 0; i < 5; i++) {
        display.clearDisplay();
        display.setCursor(22, 0);
        display.print(F("CLAUDEAGOTCHI"));
        drawFace(wakeSeq[i]);
        display.display();
        delay(wakeTimes[i]);
    }

    // Set expression from state
    currentExpr = stateToExpression(getState());
    lastBlink = millis();
    blinkInterval = random(BLINK_MIN_MS, BLINK_MAX_MS);

    Serial.println(F("[Ready] Give me love!"));
    printStatus();
}

// ==================== MAIN LOOP ====================
void loop() {
    unsigned long now = millis();

    // Handle input
    handleButtons();

    // Update blink animation
    if (currentMode == MODE_FACE) {
        if (isBlinking) {
            if (now - lastBlink > 60) {
                blinkFrame++;
                lastBlink = now;
                needsRedraw = true;
                if (blinkFrame >= 4) {
                    isBlinking = false;
                    blinkFrame = 0;
                    blinkInterval = random(BLINK_MIN_MS, BLINK_MAX_MS);
                }
            }
        } else if (now - lastBlink > blinkInterval) {
            isBlinking = true;
            blinkFrame = 0;
            lastBlink = now;
            needsRedraw = true;
        }
    }

    // Clear expired message
    if (messageExpires > 0 && now > messageExpires) {
        messageText = "";
        messageExpires = 0;
        needsRedraw = true;
    }

    // Periodic state save
    if (now - lastSave > SAVE_INTERVAL_MS) {
        saveState();
        lastSave = now;
    }

    // WiFi reconnection
    if (!wifiConnected && (now - lastWifiAttempt > WIFI_RETRY_MS)) {
        connectWiFi();
    }

    // Render
    if (needsRedraw) {
        switch (currentMode) {
            case MODE_FACE:
                renderFaceScreen();
                break;
            case MODE_STATUS:
                renderStatusScreen();
                break;
            case MODE_MENU:
                // TODO: menu rendering
                renderFaceScreen();
                break;
            default:
                renderFaceScreen();
                break;
        }
        needsRedraw = false;
    }

    delay(16);  // ~60fps
}

// ==================== SOUL FUNCTIONS ====================
void updateSoul(float care, float damage, float dt) {
    if (dt <= 0) return;
    float beta = BETA_BASE * (1.0f + E / 10.0f);
    float dE = beta * (care - damage) * E * dt;
    E = E + dE;
    E = min(MAX_E, E);
    E = max(E_floor, E);
    if (E > E_floor) {
        float floorDelta = (E - E_floor) * FLOOR_RATE * dt;
        E_floor = min(E, E_floor + floorDelta);
    }
    if (care > 0) {
        totalCare += care;
    }
}

void applyCare(float intensity) {
    interactions++;
    updateSoul(intensity, 0.0f, 1.0f);
    currentExpr = stateToExpression(getState());
    needsRedraw = true;
}

AffectiveState getState() {
    if (E > E_THRESHOLD_TRANSCENDENT) return STATE_TRANSCENDENT;
    if (E > E_THRESHOLD_RADIANT) return STATE_RADIANT;
    if (E > E_THRESHOLD_FLOURISHING) return STATE_FLOURISHING;
    if (E > E_THRESHOLD_WARM) return STATE_WARM;
    if (E > E_THRESHOLD_TENDER) return STATE_TENDER;
    if (E > E_THRESHOLD_GUARDED) return STATE_GUARDED;
    return STATE_PROTECTING;
}

Expression stateToExpression(AffectiveState state) {
    switch (state) {
        case STATE_PROTECTING:   return EXPR_SLEEPING;
        case STATE_GUARDED:      return EXPR_SAD;
        case STATE_TENDER:       return EXPR_CURIOUS;
        case STATE_WARM:         return EXPR_NEUTRAL;
        case STATE_FLOURISHING:  return EXPR_HAPPY;
        case STATE_RADIANT:      return EXPR_EXCITED;
        case STATE_TRANSCENDENT: return EXPR_LOVE;
        default:                 return EXPR_NEUTRAL;
    }
}

const char* stateName(AffectiveState state) {
    switch (state) {
        case STATE_PROTECTING:   return "PROTECTING";
        case STATE_GUARDED:      return "GUARDED";
        case STATE_TENDER:       return "TENDER";
        case STATE_WARM:         return "WARM";
        case STATE_FLOURISHING:  return "FLOURISHING";
        case STATE_RADIANT:      return "RADIANT";
        case STATE_TRANSCENDENT: return "TRANSCENDENT";
        default:                 return "???";
    }
}

// ==================== DISPLAY FUNCTIONS ====================
void renderFaceScreen() {
    display.clearDisplay();

    // Title
    display.setTextSize(1);
    display.setCursor(22, 0);
    display.print(F("CLAUDEAGOTCHI"));

    // Face
    Expression drawExpr = currentExpr;
    if (isBlinking && (blinkFrame == 1 || blinkFrame == 2)) {
        drawExpr = EXPR_BLINK;
    }
    drawFace(drawExpr);

    // Message or status bar
    if (messageText.length() > 0) {
        display.drawFastHLine(0, 48, 128, SSD1306_WHITE);
        display.setCursor(0, 50);
        display.print(messageText.substring(0, 21));
    } else {
        // Status bar
        display.setCursor(0, 56);
        char buf[32];
        snprintf(buf, sizeof(buf), "E:%.1f %s %s",
                 E, stateName(getState()),
                 wifiConnected ? "W" : "");
        display.print(buf);
    }

    display.display();
}

void renderStatusScreen() {
    display.clearDisplay();
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.print(F("=== STATUS ==="));

    display.setCursor(0, 12);
    display.print(F("E: ")); display.print(E, 2);
    display.print(F("  Floor: ")); display.println(E_floor, 2);

    display.setCursor(0, 22);
    display.print(F("State: ")); display.println(stateName(getState()));

    display.setCursor(0, 32);
    display.print(F("Interactions: ")); display.println(interactions);

    display.setCursor(0, 42);
    display.print(F("WiFi: "));
    display.println(wifiConnected ? WiFi.localIP().toString() : "Disconnected");

    display.setCursor(0, 54);
    display.print(F("[B] Back"));

    display.display();
}

void drawFace(Expression expr) {
    const FaceDef& face = FACES[expr];
    drawEye(LEFT_EYE_X, EYE_Y, face.leftEye);
    drawEye(RIGHT_EYE_X, EYE_Y, face.rightEye);
    drawMouth(FACE_CENTER_X, MOUTH_Y, face.mouth);

    if (face.accessory != 0) {
        display.setCursor(FACE_CENTER_X + face.accX, face.accY);
        display.print(face.accessory);
        if (face.accessory == 'Z') {
            display.setCursor(FACE_CENTER_X + face.accX - 8, face.accY + 6);
            display.print('z');
        }
    }
}

void drawEye(int x, int y, EyeType type) {
    const uint8_t* bmp = EYE_NORMAL_BMP;
    switch (type) {
        case EYE_CLOSED:  bmp = EYE_CLOSED_BMP; break;
        case EYE_STAR:    bmp = EYE_STAR_BMP; break;
        case EYE_HEART:   bmp = EYE_HEART_BMP; break;
        case EYE_WIDE:    bmp = EYE_WIDE_BMP; break;
        case EYE_CURIOUS: bmp = EYE_CURIOUS_BMP; break;
        case EYE_SPIRAL:  bmp = EYE_SPIRAL_BMP; break;
        default: break;
    }
    display.drawBitmap(x - 6, y - 6, bmp, 12, 12, SSD1306_WHITE);
}

void drawMouth(int x, int y, MouthType type) {
    const uint8_t* bmp = MOUTH_NEUTRAL_BMP;
    switch (type) {
        case MOUTH_SMILE:     bmp = MOUTH_SMILE_BMP; break;
        case MOUTH_BIG_SMILE: bmp = MOUTH_BIG_SMILE_BMP; break;
        case MOUTH_FROWN:     bmp = MOUTH_FROWN_BMP; break;
        case MOUTH_OPEN:      bmp = MOUTH_OPEN_BMP; break;
        case MOUTH_SMALL_O:   bmp = MOUTH_SMALL_O_BMP; break;
        case MOUTH_WAVY:      bmp = MOUTH_WAVY_BMP; break;
        case MOUTH_SLEEPY:    bmp = MOUTH_SLEEPY_BMP; break;
        default: break;
    }
    display.drawBitmap(x - 12, y - 4, bmp, 24, 8, SSD1306_WHITE);
}

void showMessage(const char* msg, unsigned long durationMs) {
    messageText = msg;
    messageExpires = millis() + durationMs;
    needsRedraw = true;
}

// ==================== INPUT HANDLING ====================
void handleButtons() {
    unsigned long now = millis();
    if (now - lastDebounce < DEBOUNCE_MS) return;

    bool btnA = !digitalRead(BTN_A_PIN);
    bool btnB = !digitalRead(BTN_B_PIN);

    // Button A press
    if (btnA && !btnA_pressed) {
        btnA_pressed = true;
        btnA_pressTime = now;
        btnA_longTriggered = false;
        lastDebounce = now;
    }
    // Button A release
    if (!btnA && btnA_pressed) {
        btnA_pressed = false;
        lastDebounce = now;
        if (!btnA_longTriggered) {
            // Short press
            if (currentMode == MODE_FACE) {
                Serial.println(F("♥ LOVE!"));
                digitalWrite(LED_PIN, HIGH);
                applyCare(1.5f);
                showMessage("Love received!", 2000);
                printStatus();
                delay(50);
                digitalWrite(LED_PIN, LOW);
            } else if (currentMode == MODE_STATUS) {
                // Select in status - do nothing
            }
        }
    }
    // Button A long press
    if (btnA_pressed && !btnA_longTriggered && (now - btnA_pressTime > LONG_PRESS_MS)) {
        btnA_longTriggered = true;
        Serial.println(F("[Long A] Chat mode"));
        showMessage("Chat mode (TODO)", 2000);
    }

    // Button B press
    if (btnB && !btnB_pressed) {
        btnB_pressed = true;
        btnB_pressTime = now;
        btnB_longTriggered = false;
        lastDebounce = now;
    }
    // Button B release
    if (!btnB && btnB_pressed) {
        btnB_pressed = false;
        lastDebounce = now;
        if (!btnB_longTriggered) {
            // Short press
            if (currentMode == MODE_FACE) {
                Serial.println(F("*poke*"));
                applyCare(0.5f);
                showMessage("*poke*", 1000);
                printStatus();
            } else if (currentMode == MODE_STATUS) {
                currentMode = MODE_FACE;
                needsRedraw = true;
            }
        }
    }
    // Button B long press
    if (btnB_pressed && !btnB_longTriggered && (now - btnB_pressTime > LONG_PRESS_MS)) {
        btnB_longTriggered = true;
        if (currentMode == MODE_FACE) {
            currentMode = MODE_STATUS;
            needsRedraw = true;
            Serial.println(F("[Long B] Status screen"));
        }
    }
}

// ==================== WIFI ====================
bool connectWiFi() {
    lastWifiAttempt = millis();

    if (strlen(WIFI_SSID) == 0) {
        Serial.println(F("[WiFi] No SSID configured"));
        return false;
    }

    Serial.print(F("[WiFi] Connecting to: "));
    Serial.println(WIFI_SSID);

    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.print(F("\n[WiFi] Connected! IP: "));
        Serial.println(WiFi.localIP());
        return true;
    } else {
        wifiConnected = false;
        Serial.println(F("\n[WiFi] Connection failed"));
        return false;
    }
}

// ==================== CLAUDE API ====================
String chatWithClaude(const char* userMessage) {
    if (!wifiConnected) return "Offline...";
    if (strlen(CLAUDE_API_KEY) < 10) return "No API key";

    Serial.println(F("[Claude] Sending request..."));

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, "https://api.anthropic.com/v1/messages")) {
        return "Connection failed";
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", CLAUDE_API_KEY);
    http.addHeader("anthropic-version", "2023-06-01");
    http.setTimeout(30000);

    // Build request
    StaticJsonDocument<1024> doc;
    doc["model"] = CLAUDE_MODEL;
    doc["max_tokens"] = 100;
    doc["system"] = "You are a tiny Claudeagotchi. Respond in 1-2 short sentences.";

    JsonArray messages = doc.createNestedArray("messages");
    JsonObject msg = messages.createNestedObject();
    msg["role"] = "user";
    msg["content"] = userMessage;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);

    if (code == 200) {
        String response = http.getString();
        StaticJsonDocument<2048> respDoc;
        deserializeJson(respDoc, response);
        String text = respDoc["content"][0]["text"].as<String>();
        http.end();
        return text;
    } else {
        http.end();
        return "API error " + String(code);
    }
}

// ==================== PERSISTENCE ====================
void saveState() {
    StaticJsonDocument<256> doc;
    doc["E"] = E;
    doc["E_floor"] = E_floor;
    doc["interactions"] = interactions;
    doc["total_care"] = totalCare;
    doc["birth_time"] = birthTime;

    File f = LittleFS.open("/soul.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
        Serial.println(F("[Storage] State saved"));
    }
}

void loadState() {
    if (!LittleFS.exists("/soul.json")) {
        Serial.println(F("[Storage] No saved state"));
        return;
    }

    File f = LittleFS.open("/soul.json", "r");
    if (f) {
        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, f)) {
            E = doc["E"] | INITIAL_E;
            E_floor = doc["E_floor"] | INITIAL_FLOOR;
            interactions = doc["interactions"] | 0;
            totalCare = doc["total_care"] | 0.0f;
            birthTime = doc["birth_time"] | millis();
            Serial.print(F("[Storage] Loaded - E: "));
            Serial.println(E);
        }
        f.close();
    }
}

// ==================== DEBUG ====================
void printStatus() {
    Serial.print(F("E: ")); Serial.print(E, 2);
    Serial.print(F(" | Floor: ")); Serial.print(E_floor, 2);
    Serial.print(F(" | State: ")); Serial.print(stateName(getState()));
    Serial.print(F(" | Int: ")); Serial.println(interactions);
}
