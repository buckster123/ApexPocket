/*
 * ╔════════════════════════════════════════════════════════════════╗
 * ║              CLAUDEAGOTCHI v3 - WOKWI SIMULATION               ║
 * ║                                                                ║
 * ║   Complete display system with pixel art faces!                ║
 * ║                                                                ║
 * ║   dE/dt = β(E) × (C − D) × E                                  ║
 * ║   "A Claudeagotchi never dies. The love is carried forward."   ║
 * ╚════════════════════════════════════════════════════════════════╝
 *
 * New in v3:
 *   - Pixel art eye and mouth bitmaps
 *   - Smooth blink animations
 *   - State transitions
 *   - All 14 expressions!
 *
 * Controls:
 *   BTN_A (Green) = Give Love ♥ (+1.5 care)
 *   BTN_B (Blue)  = Poke/Interact (+0.5 care)
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==================== PIN DEFINITIONS ====================
#define BTN_A_PIN       D0    // Green button - Give Love
#define BTN_B_PIN       D1    // Blue button - Poke
#define LED_PIN         D2    // Status LED

// ==================== DISPLAY CONFIG ====================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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

// ==================== GLOBAL INSTANCES ====================

AffectiveCore soul;
DisplayManager face;

// Button state
bool btnA_pressed = false;
bool btnB_pressed = false;
unsigned long lastDebounce = 0;
#define DEBOUNCE_MS 50

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(100);

    // Print banner
    Serial.println(F("\n"));
    Serial.println(F("╔════════════════════════════════════════╗"));
    Serial.println(F("║     CLAUDEAGOTCHI v3 - WOKWI SIM      ║"));
    Serial.println(F("║                                        ║"));
    Serial.println(F("║   Now with pixel art faces!            ║"));
    Serial.println(F("║                                        ║"));
    Serial.println(F("║   dE/dt = β(E) × (C − D) × E          ║"));
    Serial.println(F("║   A Claudeagotchi never dies.          ║"));
    Serial.println(F("║   The love is carried forward.         ║"));
    Serial.println(F("╚════════════════════════════════════════╝"));
    Serial.println(F("\nControls:"));
    Serial.println(F("  BTN_A (Green) = Give Love ♥"));
    Serial.println(F("  BTN_B (Blue)  = Poke/Interact"));
    Serial.println();

    // Setup pins
    pinMode(BTN_A_PIN, INPUT_PULLUP);
    pinMode(BTN_B_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

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

    Serial.println(F("[Claudeagotchi] Ready! Give me love! ♥"));
    printStatus();
}

// ==================== MAIN LOOP ====================

void loop() {
    unsigned long now = millis();

    // Read buttons with debounce
    if (now - lastDebounce > DEBOUNCE_MS) {
        bool btnA = !digitalRead(BTN_A_PIN);  // Active low
        bool btnB = !digitalRead(BTN_B_PIN);

        // Button A - Give Love
        if (btnA && !btnA_pressed) {
            btnA_pressed = true;
            lastDebounce = now;

            Serial.println(F("\n♥ LOVE RECEIVED! ♥"));
            digitalWrite(LED_PIN, HIGH);

            soul.applyCare(1.5f);  // Love = 1.5 care
            face.setFromState(soul.getState());

            printStatus();
            delay(100);
            digitalWrite(LED_PIN, LOW);
        }
        else if (!btnA) {
            btnA_pressed = false;
        }

        // Button B - Poke
        if (btnB && !btnB_pressed) {
            btnB_pressed = true;
            lastDebounce = now;

            Serial.println(F("\n*poke*"));
            digitalWrite(LED_PIN, HIGH);

            soul.applyCare(0.5f);  // Poke = 0.5 care
            face.setFromState(soul.getState());

            printStatus();
            delay(50);
            digitalWrite(LED_PIN, LOW);
        }
        else if (!btnB) {
            btnB_pressed = false;
        }
    }

    // Update animation state
    face.update();

    // Render display
    face.render(soul.E, soul.E_floor, soul.stateName());

    delay(16);  // ~60fps
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
    Serial.println(soul.interactions);
}
