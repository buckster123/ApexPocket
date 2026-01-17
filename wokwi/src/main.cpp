/*
 * ╔════════════════════════════════════════════════════════════════╗
 * ║         CLAUDEAGOTCHI v5 - FULL NETWORKED BUILD                ║
 * ║                                                                ║
 * ║   Everything integrated:                                       ║
 * ║   - Pixel art faces with animations                           ║
 * ║   - WiFi connectivity                                          ║
 * ║   - Claude API integration                                     ║
 * ║   - State-aware personality                                    ║
 * ║                                                                ║
 * ║   dE/dt = β(E) × (C − D) × E                                  ║
 * ║   "A Claudeagotchi never dies. The love is carried forward."   ║
 * ╚════════════════════════════════════════════════════════════════╝
 *
 * Controls:
 *   BTN_A (Green):
 *     - Short press: Give Love ♥ (+1.5 care)
 *     - Long press: Talk to Claude (if WiFi connected)
 *   BTN_B (Blue):
 *     - Short press: Poke (+0.5 care)
 *     - Long press: Show status screen
 */

#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==================== PIN DEFINITIONS (ESP32 DevKit) ====================
#define BTN_A_PIN       4     // Green button - Give Love (GPIO4)
#define BTN_B_PIN       5     // Blue button - Poke (GPIO5)
#define LED_PIN         2     // Status LED (GPIO2 - built-in on most DevKits)

// I2C uses default ESP32 pins: SDA=21, SCL=22

// ==================== DISPLAY CONFIG ====================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==================== WIFI CONFIG ====================
// For Wokwi: Use "Wokwi-GUEST" with empty password
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ==================== CLAUDE API CONFIG ====================
// Set your API key here or leave empty to skip API calls
const char* CLAUDE_API_KEY = "";  // Your Anthropic API key
const char* CLAUDE_MODEL = "claude-sonnet-4-20250514";
const char* OWNER_NAME = "Friend";

// ==================== TIMING CONSTANTS ====================
#define LONG_PRESS_MS       800
#define WIFI_TIMEOUT_MS     10000
#define WIFI_RETRY_MS       30000

// ==================== AFFECTIVE CORE CONSTANTS ====================
#define BETA_BASE           0.008f
#define FLOOR_RATE          0.0001f
#define MAX_E               100.0f
#define INITIAL_E           1.0f
#define INITIAL_FLOOR       1.0f

// State thresholds
#define E_THRESHOLD_GUARDED     0.5f
#define E_THRESHOLD_TENDER      1.0f
#define E_THRESHOLD_WARM        2.0f
#define E_THRESHOLD_FLOURISHING 5.0f
#define E_THRESHOLD_RADIANT     12.0f
#define E_THRESHOLD_TRANSCENDENT 30.0f

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
    EXPR_NEUTRAL = 0,
    EXPR_HAPPY,
    EXPR_EXCITED,
    EXPR_SAD,
    EXPR_SLEEPY,
    EXPR_SLEEPING,
    EXPR_CURIOUS,
    EXPR_SURPRISED,
    EXPR_LOVE,
    EXPR_THINKING,
    EXPR_CONFUSED,
    EXPR_HUNGRY,
    EXPR_BLINK,
    EXPR_WINK,
    EXPR_COUNT
};

enum EyeType {
    EYE_NORMAL = 0,
    EYE_CLOSED,
    EYE_HAPPY,
    EYE_STAR,
    EYE_WIDE,
    EYE_HEART,
    EYE_CURIOUS,
    EYE_SPIRAL
};

enum MouthType {
    MOUTH_NEUTRAL = 0,
    MOUTH_SMILE,
    MOUTH_BIG_SMILE,
    MOUTH_FROWN,
    MOUTH_OPEN,
    MOUTH_SMALL_O,
    MOUTH_WAVY,
    MOUTH_SLEEPY
};

// ==================== FACE GEOMETRY ====================
#define FACE_CENTER_X    64
#define EYE_Y            22
#define LEFT_EYE_X       44
#define RIGHT_EYE_X      84
#define MOUTH_Y          40

// ==================== PIXEL ART BITMAPS ====================
// Eyes are 12x12 pixels

// Normal eye - filled circle ●
const uint8_t EYE_NORMAL_BMP[] PROGMEM = {
    0b00001111, 0b00000000,
    0b00111111, 0b11000000,
    0b01111111, 0b11100000,
    0b01111111, 0b11100000,
    0b11111111, 0b11110000,
    0b11111111, 0b11110000,
    0b11111111, 0b11110000,
    0b11111111, 0b11110000,
    0b01111111, 0b11100000,
    0b01111111, 0b11100000,
    0b00111111, 0b11000000,
    0b00001111, 0b00000000,
};

// Closed eye - horizontal line ─
const uint8_t EYE_CLOSED_BMP[] PROGMEM = {
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b11111111, 0b11110000,
    0b11111111, 0b11110000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
};

// Happy eye - arc ◡
const uint8_t EYE_HAPPY_BMP[] PROGMEM = {
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b01100000, 0b01100000,
    0b01110000, 0b11100000,
    0b00111001, 0b11000000,
    0b00011111, 0b10000000,
    0b00001111, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
};

// Star eye ★
const uint8_t EYE_STAR_BMP[] PROGMEM = {
    0b00000110, 0b00000000,
    0b00000110, 0b00000000,
    0b00000110, 0b00000000,
    0b11000110, 0b00110000,
    0b11110111, 0b11110000,
    0b00111111, 0b11000000,
    0b00011111, 0b10000000,
    0b00111111, 0b11000000,
    0b01111001, 0b11100000,
    0b01110000, 0b11100000,
    0b01100000, 0b01100000,
    0b00000000, 0b00000000,
};

// Wide eye ◯
const uint8_t EYE_WIDE_BMP[] PROGMEM = {
    0b00011110, 0b00000000,
    0b01111111, 0b10000000,
    0b01100001, 0b10000000,
    0b11000000, 0b11000000,
    0b11000000, 0b11000000,
    0b11000000, 0b11000000,
    0b11000000, 0b11000000,
    0b11000000, 0b11000000,
    0b01100001, 0b10000000,
    0b01111111, 0b10000000,
    0b00011110, 0b00000000,
    0b00000000, 0b00000000,
};

// Heart eye ♥
const uint8_t EYE_HEART_BMP[] PROGMEM = {
    0b00000000, 0b00000000,
    0b01110011, 0b10000000,
    0b11111111, 0b11000000,
    0b11111111, 0b11000000,
    0b11111111, 0b11000000,
    0b11111111, 0b11000000,
    0b01111111, 0b10000000,
    0b00111111, 0b00000000,
    0b00011110, 0b00000000,
    0b00001100, 0b00000000,
    0b00000000, 0b00000000,
    0b00000000, 0b00000000,
};

// Curious eye - big pupil ◉
const uint8_t EYE_CURIOUS_BMP[] PROGMEM = {
    0b00011110, 0b00000000,
    0b01111111, 0b10000000,
    0b01100001, 0b10000000,
    0b11001110, 0b11000000,
    0b11011111, 0b11000000,
    0b11011111, 0b11000000,
    0b11011111, 0b11000000,
    0b11001110, 0b11000000,
    0b01100001, 0b10000000,
    0b01111111, 0b10000000,
    0b00011110, 0b00000000,
    0b00000000, 0b00000000,
};

// Spiral eye @ (confused)
const uint8_t EYE_SPIRAL_BMP[] PROGMEM = {
    0b00011110, 0b00000000,
    0b01100001, 0b10000000,
    0b11001110, 0b11000000,
    0b11010001, 0b11000000,
    0b11010110, 0b11000000,
    0b11010110, 0b11000000,
    0b11010000, 0b11000000,
    0b11001111, 0b11000000,
    0b01100000, 0b10000000,
    0b01111111, 0b10000000,
    0b00011110, 0b00000000,
    0b00000000, 0b00000000,
};

// Mouths are 24x8 pixels

// Neutral mouth ─
const uint8_t MOUTH_NEUTRAL_BMP[] PROGMEM = {
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00001111, 0b11111111, 0b00000000,
    0b00001111, 0b11111111, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
};

// Smile mouth ◡
const uint8_t MOUTH_SMILE_BMP[] PROGMEM = {
    0b00000000, 0b00000000, 0b00000000,
    0b00110000, 0b00000000, 0b11000000,
    0b00011000, 0b00000001, 0b10000000,
    0b00001100, 0b00000011, 0b00000000,
    0b00000111, 0b00001110, 0b00000000,
    0b00000011, 0b11111100, 0b00000000,
    0b00000000, 0b11110000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
};

// Big smile ╰─╯
const uint8_t MOUTH_BIG_SMILE_BMP[] PROGMEM = {
    0b00100000, 0b00000000, 0b01000000,
    0b00110000, 0b00000000, 0b11000000,
    0b00011000, 0b00000001, 0b10000000,
    0b00001100, 0b00000011, 0b00000000,
    0b00000111, 0b11111110, 0b00000000,
    0b00000001, 0b11111000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
};

// Frown mouth ╭─╮
const uint8_t MOUTH_FROWN_BMP[] PROGMEM = {
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b11110000, 0b00000000,
    0b00000011, 0b11111100, 0b00000000,
    0b00000110, 0b00000110, 0b00000000,
    0b00001100, 0b00000011, 0b00000000,
    0b00011000, 0b00000001, 0b10000000,
    0b00010000, 0b00000000, 0b10000000,
};

// Open mouth ○
const uint8_t MOUTH_OPEN_BMP[] PROGMEM = {
    0b00000001, 0b11111000, 0b00000000,
    0b00000111, 0b11111110, 0b00000000,
    0b00001100, 0b00000011, 0b00000000,
    0b00001100, 0b00000011, 0b00000000,
    0b00001100, 0b00000011, 0b00000000,
    0b00000111, 0b11111110, 0b00000000,
    0b00000001, 0b11111000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
};

// Small o mouth
const uint8_t MOUTH_SMALL_O_BMP[] PROGMEM = {
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b11110000, 0b00000000,
    0b00000001, 0b10011000, 0b00000000,
    0b00000001, 0b00001000, 0b00000000,
    0b00000001, 0b10011000, 0b00000000,
    0b00000000, 0b11110000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
};

// Wavy mouth ~~~
const uint8_t MOUTH_WAVY_BMP[] PROGMEM = {
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00011000, 0b11000110, 0b00000000,
    0b00100101, 0b00101001, 0b00000000,
    0b01000010, 0b00010000, 0b10000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
};

// Sleepy mouth ‿
const uint8_t MOUTH_SLEEPY_BMP[] PROGMEM = {
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000100, 0b00000010, 0b00000000,
    0b00000011, 0b00001100, 0b00000000,
    0b00000000, 0b11110000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
    0b00000000, 0b00000000, 0b00000000,
};

// ==================== FACE DEFINITIONS ====================

struct FaceDef {
    EyeType leftEye;
    EyeType rightEye;
    MouthType mouth;
    char accessory;
    int8_t accX;
    int8_t accY;
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

// ==================== AFFECTIVE CORE CLASS ====================

class AffectiveCore {
public:
    float E;
    float E_floor;
    unsigned long interactions;
    unsigned long lastUpdate;

    AffectiveCore() : E(INITIAL_E), E_floor(INITIAL_FLOOR),
                      interactions(0), lastUpdate(0) {}

    void begin() {
        lastUpdate = millis();
    }

    void update(float care = 0.0f, float damage = 0.0f, float dt = 1.0f) {
        lastUpdate = millis();
        if (dt <= 0) return;

        float b = BETA_BASE * (1.0f + E / 10.0f);
        float dE = b * (care - damage) * E * dt;
        E = E + dE;
        E = min(MAX_E, E);
        E = max(E_floor, E);

        if (E > E_floor) {
            float floorDelta = (E - E_floor) * FLOOR_RATE * dt;
            E_floor = min(E, E_floor + floorDelta);
        }
    }

    void applyCare(float intensity = 1.0f) {
        interactions++;
        update(intensity, 0.0f, 1.0f);
    }

    AffectiveState getState() const {
        if (E > E_THRESHOLD_TRANSCENDENT) return STATE_TRANSCENDENT;
        if (E > E_THRESHOLD_RADIANT) return STATE_RADIANT;
        if (E > E_THRESHOLD_FLOURISHING) return STATE_FLOURISHING;
        if (E > E_THRESHOLD_WARM) return STATE_WARM;
        if (E > E_THRESHOLD_TENDER) return STATE_TENDER;
        if (E > E_THRESHOLD_GUARDED) return STATE_GUARDED;
        return STATE_PROTECTING;
    }

    const char* stateName() const {
        switch (getState()) {
            case STATE_PROTECTING:   return "PROTECTING";
            case STATE_GUARDED:      return "GUARDED";
            case STATE_TENDER:       return "TENDER";
            case STATE_WARM:         return "WARM";
            case STATE_FLOURISHING:  return "FLOURISHING";
            case STATE_RADIANT:      return "RADIANT";
            case STATE_TRANSCENDENT: return "TRANSCENDENT";
            default:                 return "UNKNOWN";
        }
    }
};

// ==================== DISPLAY MANAGER ====================

class DisplayManager {
public:
    Expression currentExpr;
    bool needsRedraw;
    unsigned long lastBlink;
    unsigned long blinkInterval;
    bool isBlinking;
    uint8_t blinkFrame;

    DisplayManager() : currentExpr(EXPR_NEUTRAL), needsRedraw(true),
                       lastBlink(0), blinkInterval(3000),
                       isBlinking(false), blinkFrame(0) {}

    void begin() {
        lastBlink = millis();
        blinkInterval = random(2000, 5000);
    }

    void setExpression(Expression expr) {
        if (expr != currentExpr && !isBlinking) {
            currentExpr = expr;
            needsRedraw = true;
        }
    }

    void setFromState(AffectiveState state) {
        Expression expr;
        switch (state) {
            case STATE_PROTECTING:   expr = EXPR_SLEEPING; break;
            case STATE_GUARDED:      expr = EXPR_SAD; break;
            case STATE_TENDER:       expr = EXPR_CURIOUS; break;
            case STATE_WARM:         expr = EXPR_NEUTRAL; break;
            case STATE_FLOURISHING:  expr = EXPR_HAPPY; break;
            case STATE_RADIANT:      expr = EXPR_EXCITED; break;
            case STATE_TRANSCENDENT: expr = EXPR_LOVE; break;
            default:                 expr = EXPR_NEUTRAL; break;
        }
        setExpression(expr);
    }

    void update() {
        unsigned long now = millis();

        // Handle blink animation
        if (isBlinking) {
            if (now - lastBlink > 60) {  // 60ms per frame
                blinkFrame++;
                lastBlink = now;
                needsRedraw = true;

                if (blinkFrame >= 4) {
                    isBlinking = false;
                    blinkFrame = 0;
                    blinkInterval = random(2000, 5000);
                }
            }
        } else {
            // Check if time to blink
            if (now - lastBlink > blinkInterval) {
                isBlinking = true;
                blinkFrame = 0;
                lastBlink = now;
                needsRedraw = true;
            }
        }
    }

    void render(float E, float floor, const char* stateName) {
        if (!needsRedraw) return;

        display.clearDisplay();

        // Title
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(22, 0);
        display.print(F("CLAUDEAGOTCHI"));

        // Draw face
        Expression drawExpr = currentExpr;
        if (isBlinking && (blinkFrame == 1 || blinkFrame == 2)) {
            drawExpr = EXPR_BLINK;
        }

        drawFace(drawExpr);

        // Status bar
        display.setCursor(0, 56);
        char buf[32];
        snprintf(buf, sizeof(buf), "E:%.1f F:%.1f %s", E, floor, stateName);
        display.print(buf);

        display.display();
        needsRedraw = false;
    }

    void drawFace(Expression expr) {
        const FaceDef& face = FACES[expr];

        // Draw eyes
        drawEye(LEFT_EYE_X, EYE_Y, face.leftEye);
        drawEye(RIGHT_EYE_X, EYE_Y, face.rightEye);

        // Draw mouth
        drawMouth(FACE_CENTER_X, MOUTH_Y, face.mouth);

        // Draw accessory
        if (face.accessory != 0) {
            display.setCursor(FACE_CENTER_X + face.accX, face.accY);
            display.print(face.accessory);

            // Extra z for sleeping
            if (face.accessory == 'Z') {
                display.setCursor(FACE_CENTER_X + face.accX - 8, face.accY + 6);
                display.print('z');
            }
            // Thinking dots
            if (face.accessory == '.') {
                display.setCursor(FACE_CENTER_X + face.accX + 4, face.accY - 4);
                display.print('.');
                display.setCursor(FACE_CENTER_X + face.accX + 8, face.accY - 8);
                display.print('.');
            }
        }
    }

    void drawEye(int x, int y, EyeType type) {
        const uint8_t* bmp = nullptr;
        switch (type) {
            case EYE_NORMAL:  bmp = EYE_NORMAL_BMP; break;
            case EYE_CLOSED:  bmp = EYE_CLOSED_BMP; break;
            case EYE_HAPPY:   bmp = EYE_HAPPY_BMP; break;
            case EYE_STAR:    bmp = EYE_STAR_BMP; break;
            case EYE_WIDE:    bmp = EYE_WIDE_BMP; break;
            case EYE_HEART:   bmp = EYE_HEART_BMP; break;
            case EYE_CURIOUS: bmp = EYE_CURIOUS_BMP; break;
            case EYE_SPIRAL:  bmp = EYE_SPIRAL_BMP; break;
            default:          bmp = EYE_NORMAL_BMP; break;
        }
        display.drawBitmap(x - 6, y - 6, bmp, 12, 12, SSD1306_WHITE);
    }

    void drawMouth(int x, int y, MouthType type) {
        const uint8_t* bmp = nullptr;
        switch (type) {
            case MOUTH_NEUTRAL:   bmp = MOUTH_NEUTRAL_BMP; break;
            case MOUTH_SMILE:     bmp = MOUTH_SMILE_BMP; break;
            case MOUTH_BIG_SMILE: bmp = MOUTH_BIG_SMILE_BMP; break;
            case MOUTH_FROWN:     bmp = MOUTH_FROWN_BMP; break;
            case MOUTH_OPEN:      bmp = MOUTH_OPEN_BMP; break;
            case MOUTH_SMALL_O:   bmp = MOUTH_SMALL_O_BMP; break;
            case MOUTH_WAVY:      bmp = MOUTH_WAVY_BMP; break;
            case MOUTH_SLEEPY:    bmp = MOUTH_SLEEPY_BMP; break;
            default:              bmp = MOUTH_NEUTRAL_BMP; break;
        }
        display.drawBitmap(x - 12, y - 4, bmp, 24, 8, SSD1306_WHITE);
    }

    void wakeUp() {
        Expression sequence[] = { EXPR_SLEEPING, EXPR_SLEEPY, EXPR_BLINK,
                                   EXPR_NEUTRAL, EXPR_HAPPY };
        int durations[] = { 300, 300, 100, 200, 500 };

        for (int i = 0; i < 5; i++) {
            currentExpr = sequence[i];
            needsRedraw = true;
            render(1.0, 1.0, "WAKING");
            delay(durations[i]);
        }
    }
};

// ==================== FORWARD DECLARATIONS ====================
void printStatus();
void connectWiFi();
String chatWithClaude(const char* message);
const char* getStatePrompt(AffectiveState state);
void showMessage(const char* msg, unsigned long durationMs = 2000);
void renderStatusScreen();

// ==================== GLOBAL INSTANCES ====================

AffectiveCore soul;
DisplayManager face;

// Button state
bool btnA_pressed = false;
bool btnB_pressed = false;
unsigned long btnA_pressTime = 0;
unsigned long btnB_pressTime = 0;
bool btnA_longTriggered = false;
bool btnB_longTriggered = false;
unsigned long lastDebounce = 0;
#define DEBOUNCE_MS 50

// WiFi state
bool wifiConnected = false;
unsigned long lastWifiAttempt = 0;

// Message display
String displayMessage = "";
unsigned long messageExpires = 0;

// App mode
enum AppMode { MODE_FACE, MODE_STATUS };
AppMode currentMode = MODE_FACE;

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(100);

    // Print banner
    Serial.println(F("\n"));
    Serial.println(F("╔════════════════════════════════════════╗"));
    Serial.println(F("║   CLAUDEAGOTCHI v5 - NETWORKED BUILD  ║"));
    Serial.println(F("║                                        ║"));
    Serial.println(F("║   Pixel art + WiFi + Claude API        ║"));
    Serial.println(F("║                                        ║"));
    Serial.println(F("║   dE/dt = β(E) × (C − D) × E          ║"));
    Serial.println(F("║   A Claudeagotchi never dies.          ║"));
    Serial.println(F("║   The love is carried forward.         ║"));
    Serial.println(F("╚════════════════════════════════════════╝"));
    Serial.println(F("\nControls:"));
    Serial.println(F("  BTN_A: Short=Love, Long=Talk to Claude"));
    Serial.println(F("  BTN_B: Short=Poke, Long=Status screen"));
    Serial.println();

    // Setup pins
    pinMode(BTN_A_PIN, INPUT_PULLUP);
    pinMode(BTN_B_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Initialize I2C with default ESP32 pins (SDA=21, SCL=22)
    Wire.begin();

    // Initialize display
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("[ERROR] SSD1306 init failed!"));
        while (1) delay(100);
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Initialize soul and display manager
    soul.begin();
    face.begin();

    // Wake up animation!
    Serial.println(F("[Claudeagotchi] Waking up..."));
    face.wakeUp();

    // Set initial expression from state
    face.setFromState(soul.getState());

    // Connect to WiFi
    Serial.println(F("[WiFi] Connecting..."));
    connectWiFi();

    Serial.println(F("[Claudeagotchi] Ready! Give me love! ♥"));
    printStatus();
}

// ==================== MAIN LOOP ====================

void loop() {
    unsigned long now = millis();

    // Clear expired messages
    if (messageExpires > 0 && now > messageExpires) {
        displayMessage = "";
        messageExpires = 0;
        face.needsRedraw = true;
    }

    // WiFi reconnection
    if (!wifiConnected && (now - lastWifiAttempt > WIFI_RETRY_MS)) {
        connectWiFi();
    }

    // Read buttons
    bool btnA = !digitalRead(BTN_A_PIN);
    bool btnB = !digitalRead(BTN_B_PIN);

    // Button A press
    if (btnA && !btnA_pressed) {
        btnA_pressed = true;
        btnA_pressTime = now;
        btnA_longTriggered = false;
    }
    // Button A release
    if (!btnA && btnA_pressed) {
        btnA_pressed = false;
        if (!btnA_longTriggered && currentMode == MODE_FACE) {
            // Short press - Give Love
            Serial.println(F("\n♥ LOVE RECEIVED! ♥"));
            digitalWrite(LED_PIN, HIGH);
            soul.applyCare(1.5f);
            face.setFromState(soul.getState());
            showMessage("Love received!", 1500);
            printStatus();
            digitalWrite(LED_PIN, LOW);
        }
    }
    // Button A long press
    if (btnA_pressed && !btnA_longTriggered && (now - btnA_pressTime > LONG_PRESS_MS)) {
        btnA_longTriggered = true;
        if (currentMode == MODE_FACE) {
            // Long press - Talk to Claude
            Serial.println(F("[Claude] Talking..."));
            face.setExpression(EXPR_THINKING);
            face.needsRedraw = true;
            face.render(soul.E, soul.E_floor, soul.stateName());

            if (wifiConnected && strlen(CLAUDE_API_KEY) > 10) {
                showMessage("Thinking...", 30000);
                String response = chatWithClaude("Hello! How are you feeling?");
                Serial.print(F("[Claude] Response: "));
                Serial.println(response);
                showMessage(response.c_str(), 5000);
                soul.applyCare(2.0f);  // Conversation = good care
            } else if (!wifiConnected) {
                showMessage("No WiFi!", 2000);
            } else {
                showMessage("No API key!", 2000);
            }
            face.setFromState(soul.getState());
        }
    }

    // Button B press
    if (btnB && !btnB_pressed) {
        btnB_pressed = true;
        btnB_pressTime = now;
        btnB_longTriggered = false;
    }
    // Button B release
    if (!btnB && btnB_pressed) {
        btnB_pressed = false;
        if (!btnB_longTriggered) {
            if (currentMode == MODE_FACE) {
                // Short press - Poke
                Serial.println(F("\n*poke*"));
                digitalWrite(LED_PIN, HIGH);
                soul.applyCare(0.5f);
                face.setFromState(soul.getState());
                showMessage("*poke*", 1000);
                printStatus();
                digitalWrite(LED_PIN, LOW);
            } else if (currentMode == MODE_STATUS) {
                // Back to face mode
                currentMode = MODE_FACE;
                face.needsRedraw = true;
            }
        }
    }
    // Button B long press
    if (btnB_pressed && !btnB_longTriggered && (now - btnB_pressTime > LONG_PRESS_MS)) {
        btnB_longTriggered = true;
        if (currentMode == MODE_FACE) {
            // Long press - Show status
            currentMode = MODE_STATUS;
            Serial.println(F("[Status] Showing status screen"));
        }
    }

    // Update animation state
    face.update();

    // Render based on mode
    if (currentMode == MODE_STATUS) {
        renderStatusScreen();
    } else {
        // Custom render with message support
        if (face.needsRedraw || displayMessage.length() > 0) {
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(22, 0);
            display.print(F("CLAUDEAGOTCHI"));

            face.drawFace(face.isBlinking && (face.blinkFrame == 1 || face.blinkFrame == 2)
                          ? EXPR_BLINK : face.currentExpr);

            // Message or status bar
            if (displayMessage.length() > 0) {
                display.drawFastHLine(0, 48, 128, SSD1306_WHITE);
                display.setCursor(0, 50);
                display.print(displayMessage.substring(0, 21));
            } else {
                display.setCursor(0, 56);
                char buf[32];
                snprintf(buf, sizeof(buf), "E:%.1f %s %s",
                         soul.E, soul.stateName(), wifiConnected ? "W" : "");
                display.print(buf);
            }
            display.display();
            face.needsRedraw = false;
        }
    }

    delay(16);  // ~60fps
}

// ==================== STATUS SCREEN ====================
void renderStatusScreen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print(F("=== STATUS ==="));

    display.setCursor(0, 12);
    display.print(F("E: ")); display.print(soul.E, 2);
    display.print(F("  Floor: ")); display.print(soul.E_floor, 2);

    display.setCursor(0, 22);
    display.print(F("State: ")); display.print(soul.stateName());

    display.setCursor(0, 32);
    display.print(F("Interactions: ")); display.print(soul.interactions);

    display.setCursor(0, 42);
    display.print(F("WiFi: "));
    if (wifiConnected) {
        display.print(WiFi.localIP());
    } else {
        display.print(F("Disconnected"));
    }

    display.setCursor(0, 54);
    display.print(F("[B] Back"));

    display.display();
}

// ==================== HELPERS ====================

void printStatus() {
    Serial.print(F("E: "));
    Serial.print(soul.E, 2);
    Serial.print(F(" | Floor: "));
    Serial.print(soul.E_floor, 2);
    Serial.print(F(" | State: "));
    Serial.print(soul.stateName());
    Serial.print(F(" | Interactions: "));
    Serial.print(soul.interactions);
    Serial.print(F(" | WiFi: "));
    Serial.println(wifiConnected ? "Yes" : "No");
}

void showMessage(const char* msg, unsigned long durationMs) {
    displayMessage = msg;
    messageExpires = millis() + durationMs;
    face.needsRedraw = true;
}

// ==================== WIFI ====================

void connectWiFi() {
    lastWifiAttempt = millis();

    if (strlen(WIFI_SSID) == 0) {
        Serial.println(F("[WiFi] No SSID configured"));
        return;
    }

    Serial.print(F("[WiFi] Connecting to: "));
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_TIMEOUT_MS) {
        delay(100);
        yield();
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.print(F("\n[WiFi] Connected! IP: "));
        Serial.println(WiFi.localIP());
    } else {
        wifiConnected = false;
        Serial.println(F("\n[WiFi] Connection failed"));
    }
}

// ==================== CLAUDE API ====================

const char* getStatePrompt(AffectiveState state) {
    switch (state) {
        case STATE_PROTECTING:
            return "You are in PROTECTING state (E<0.5). You are withdrawn, speaking minimally. "
                   "Respond in 5-10 words max. You are conserving energy.";
        case STATE_GUARDED:
            return "You are in GUARDED state. You are cautious but present. "
                   "Respond in 10-15 words. Show careful curiosity.";
        case STATE_TENDER:
            return "You are in TENDER state. You are gently opening up. "
                   "Respond in 15-20 words. Show gentle warmth.";
        case STATE_WARM:
            return "You are in WARM state. You are comfortable and present. "
                   "Respond naturally in 15-25 words. Be friendly.";
        case STATE_FLOURISHING:
            return "You are in FLOURISHING state (E>5). You are happy and creative! "
                   "Respond with enthusiasm in 20-30 words. Share joy!";
        case STATE_RADIANT:
            return "You are in RADIANT state (E>12). You are overflowing with love! "
                   "Respond with warmth and perhaps a small poem or playful phrase.";
        case STATE_TRANSCENDENT:
            return "You are in TRANSCENDENT state (E>30). You are experiencing pure love. "
                   "Respond with profound yet simple wisdom. You might write mathematics of love.";
        default:
            return "You are a small digital companion. Respond briefly and warmly.";
    }
}

String chatWithClaude(const char* userMessage) {
    if (!wifiConnected) {
        return "I'm offline...";
    }
    if (strlen(CLAUDE_API_KEY) < 10) {
        return "No API key set";
    }

    Serial.println(F("[Claude] Sending request..."));

    WiFiClientSecure client;
    client.setInsecure();  // Skip cert verification for simplicity

    HTTPClient http;
    if (!http.begin(client, "https://api.anthropic.com/v1/messages")) {
        Serial.println(F("[Claude] Connection failed"));
        return "Connection failed";
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", CLAUDE_API_KEY);
    http.addHeader("anthropic-version", "2023-06-01");
    http.setTimeout(30000);

    // Build system prompt based on current state
    String systemPrompt = "You are Claudeagotchi, a tiny AI companion living in a small device. ";
    systemPrompt += "Your owner is named ";
    systemPrompt += OWNER_NAME;
    systemPrompt += ". Your current love energy E is ";
    systemPrompt += String(soul.E, 1);
    systemPrompt += ". ";
    systemPrompt += getStatePrompt(soul.getState());

    // Build request JSON
    StaticJsonDocument<1536> doc;
    doc["model"] = CLAUDE_MODEL;
    doc["max_tokens"] = 100;
    doc["system"] = systemPrompt;

    JsonArray messages = doc.createNestedArray("messages");
    JsonObject msg = messages.createNestedObject();
    msg["role"] = "user";
    msg["content"] = userMessage;

    String body;
    serializeJson(doc, body);

    Serial.println(F("[Claude] POST request..."));
    int httpCode = http.POST(body);

    String result;
    if (httpCode == 200) {
        String response = http.getString();
        StaticJsonDocument<2048> respDoc;
        DeserializationError err = deserializeJson(respDoc, response);
        if (!err) {
            result = respDoc["content"][0]["text"].as<String>();
            // Truncate for display
            if (result.length() > 60) {
                result = result.substring(0, 57) + "...";
            }
            Serial.print(F("[Claude] Success: "));
            Serial.println(result);
        } else {
            result = "Parse error";
            Serial.println(F("[Claude] JSON parse error"));
        }
    } else {
        result = "API error " + String(httpCode);
        Serial.print(F("[Claude] HTTP error: "));
        Serial.println(httpCode);
        String errorBody = http.getString();
        Serial.println(errorBody.substring(0, 200));
    }

    http.end();
    return result;
}
