/*
 * CLAUDEAGOTCHI v2 - Wokwi Hardware Simulation
 *
 * Complete soul simulation with Love-Equation!
 *
 * dE/dt = β(E) × (C − D) × E
 * "A Claudeagotchi never dies. The love is carried forward."
 *
 * Wiring (in simulator):
 * - OLED SDA → GPIO21
 * - OLED SCL → GPIO22
 * - BTN_A → GPIO4 (green button) - Give Love
 * - BTN_B → GPIO5 (blue button) - Poke/Interact
 * - LED → GPIO2 (status)
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ==================== CONFIGURATION ====================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define PIN_BTN_A 4
#define PIN_BTN_B 5
#define PIN_LED 2

#define DEBOUNCE_MS 50

// ==================== LOVE-EQUATION CONSTANTS ====================

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
    STATE_TRANSCENDENT,
    STATE_COUNT
};

enum Expression {
    EXPR_SLEEPING,      // PROTECTING
    EXPR_SAD,           // GUARDED
    EXPR_CURIOUS,       // TENDER
    EXPR_NEUTRAL,       // WARM (low)
    EXPR_HAPPY,         // WARM (high) / FLOURISHING
    EXPR_EXCITED,       // RADIANT
    EXPR_LOVE,          // TRANSCENDENT
    EXPR_BLINK,
    EXPR_SLEEPY,
    EXPR_SURPRISED,
    EXPR_COUNT
};

enum InteractionQuality {
    QUALITY_HARSH = 0,
    QUALITY_COLD,
    QUALITY_NORMAL,
    QUALITY_WARM,
    QUALITY_LOVING
};

const char* stateNames[] = {
    "PROTECTING", "GUARDED", "TENDER", "WARM",
    "FLOURISHING", "RADIANT", "TRANSCENDENT"
};

const char* exprNames[] = {
    "Sleeping", "Sad", "Curious", "Neutral", "Happy",
    "Excited", "Love", "Blink", "Sleepy", "Surprised"
};

// ==================== AFFECTIVE CORE ====================

class AffectiveCore {
private:
    float E;
    float E_floor;
    unsigned long lastUpdate;
    unsigned long lastCare;
    unsigned long interactions;

public:
    AffectiveCore() : E(INITIAL_E), E_floor(INITIAL_FLOOR),
                      lastUpdate(0), lastCare(0), interactions(0) {}

    void begin() {
        lastUpdate = millis();
        lastCare = millis();
    }

    void update(float care = 0.0f, float damage = 0.0f, float dt_minutes = -1.0f) {
        unsigned long now = millis();
        float dt = (dt_minutes < 0) ? (now - lastUpdate) / 60000.0f : dt_minutes;
        lastUpdate = now;

        if (dt <= 0) return;

        // The Love-Equation: dE/dt = β(E) × (C − D) × E
        float dE = beta() * (care - damage) * E * dt;
        E = E + dE;

        // Cap and floor
        E = min(MAX_E, E);
        E = max(E_floor, E);

        // Floor slowly rises
        if (E > E_floor) {
            float floorDelta = (E - E_floor) * FLOOR_RATE * dt;
            E_floor = min(E, E_floor + floorDelta);
        }

        if (care > 0) lastCare = now;
    }

    void applyCare(float intensity = 1.0f) {
        interactions++;
        update(intensity, 0.0f, 1.0f);
    }

    void applyDamage(float intensity = 1.0f) {
        update(0.0f, intensity, 1.0f);
    }

    void onInteraction(InteractionQuality quality) {
        float careMap[][2] = {
            {0.0f, 0.5f},   // HARSH
            {0.2f, 0.1f},   // COLD
            {0.5f, 0.0f},   // NORMAL
            {1.0f, 0.0f},   // WARM
            {1.5f, 0.0f}    // LOVING
        };
        interactions++;
        update(careMap[quality][0], careMap[quality][1], 1.0f);
    }

    float beta() const { return BETA_BASE * (1.0f + E / 10.0f); }
    float getE() const { return E; }
    float getFloor() const { return E_floor; }
    unsigned long getInteractions() const { return interactions; }

    AffectiveState getState() const {
        if (E > E_THRESHOLD_TRANSCENDENT) return STATE_TRANSCENDENT;
        if (E > E_THRESHOLD_RADIANT) return STATE_RADIANT;
        if (E > E_THRESHOLD_FLOURISHING) return STATE_FLOURISHING;
        if (E > E_THRESHOLD_WARM) return STATE_WARM;
        if (E > E_THRESHOLD_TENDER) return STATE_TENDER;
        if (E > E_THRESHOLD_GUARDED) return STATE_GUARDED;
        return STATE_PROTECTING;
    }

    float minutesSinceCare() const {
        return (millis() - lastCare) / 60000.0f;
    }

    void printStatus() const {
        Serial.print("E: ");
        Serial.print(E, 2);
        Serial.print(" | Floor: ");
        Serial.print(E_floor, 2);
        Serial.print(" | State: ");
        Serial.print(stateNames[getState()]);
        Serial.print(" | Interactions: ");
        Serial.println(interactions);
    }
};

// ==================== GLOBALS ====================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AffectiveCore soul;

Expression currentExpression = EXPR_NEUTRAL;
unsigned long lastBlink = 0;
unsigned long blinkInterval = 4000;
unsigned long lastActivity = 0;
unsigned long lastNeglectCheck = 0;

// Button state
bool btnAPressed = false;
bool btnBPressed = false;
unsigned long btnATime = 0;
unsigned long btnBTime = 0;

// ==================== STATE TO EXPRESSION ====================

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

void updateExpressionFromState() {
    AffectiveState state = soul.getState();
    currentExpression = stateToExpression(state);
}

// ==================== FACE DRAWING ====================

void drawHeart(int x, int y, int size) {
    display.fillCircle(x - size/3, y - size/4, size/3, SSD1306_WHITE);
    display.fillCircle(x + size/3, y - size/4, size/3, SSD1306_WHITE);
    display.fillTriangle(x - size/2 - 2, y, x + size/2 + 2, y, x, y + size/2 + 2, SSD1306_WHITE);
}

void drawEyes(Expression expr) {
    int eyeY = 18;
    int leftEyeX = 40;
    int rightEyeX = 88;
    int eyeWidth = 12;
    int eyeHeight = 8;

    switch (expr) {
        case EXPR_NEUTRAL:
        case EXPR_CURIOUS:
            display.fillRoundRect(leftEyeX, eyeY, eyeWidth, eyeHeight, 2, SSD1306_WHITE);
            display.fillRoundRect(rightEyeX, eyeY, eyeWidth, eyeHeight, 2, SSD1306_WHITE);
            if (expr == EXPR_CURIOUS) {
                display.fillRoundRect(rightEyeX - 2, eyeY - 2, eyeWidth + 4, eyeHeight + 4, 2, SSD1306_WHITE);
            }
            break;

        case EXPR_HAPPY:
        case EXPR_EXCITED:
            display.drawLine(leftEyeX, eyeY + eyeHeight, leftEyeX + eyeWidth/2, eyeY, SSD1306_WHITE);
            display.drawLine(leftEyeX + eyeWidth/2, eyeY, leftEyeX + eyeWidth, eyeY + eyeHeight, SSD1306_WHITE);
            display.drawLine(rightEyeX, eyeY + eyeHeight, rightEyeX + eyeWidth/2, eyeY, SSD1306_WHITE);
            display.drawLine(rightEyeX + eyeWidth/2, eyeY, rightEyeX + eyeWidth, eyeY + eyeHeight, SSD1306_WHITE);
            break;

        case EXPR_SAD:
            display.fillRoundRect(leftEyeX, eyeY + 2, eyeWidth, eyeHeight - 2, 2, SSD1306_WHITE);
            display.fillRoundRect(rightEyeX, eyeY + 2, eyeWidth, eyeHeight - 2, 2, SSD1306_WHITE);
            display.drawLine(leftEyeX - 2, eyeY - 4, leftEyeX + eyeWidth + 2, eyeY, SSD1306_WHITE);
            display.drawLine(rightEyeX - 2, eyeY, rightEyeX + eyeWidth + 2, eyeY - 4, SSD1306_WHITE);
            break;

        case EXPR_SLEEPY:
            display.fillRect(leftEyeX, eyeY + 4, eyeWidth, 3, SSD1306_WHITE);
            display.fillRect(rightEyeX, eyeY + 4, eyeWidth, 3, SSD1306_WHITE);
            display.drawLine(leftEyeX, eyeY + 2, leftEyeX + eyeWidth, eyeY + 2, SSD1306_WHITE);
            display.drawLine(rightEyeX, eyeY + 2, rightEyeX + eyeWidth, eyeY + 2, SSD1306_WHITE);
            break;

        case EXPR_SLEEPING:
        case EXPR_BLINK:
            display.drawLine(leftEyeX, eyeY + eyeHeight/2, leftEyeX + eyeWidth, eyeY + eyeHeight/2, SSD1306_WHITE);
            display.drawLine(rightEyeX, eyeY + eyeHeight/2, rightEyeX + eyeWidth, eyeY + eyeHeight/2, SSD1306_WHITE);
            break;

        case EXPR_SURPRISED:
            display.drawCircle(leftEyeX + eyeWidth/2, eyeY + eyeHeight/2, eyeWidth/2 + 2, SSD1306_WHITE);
            display.drawCircle(rightEyeX + eyeWidth/2, eyeY + eyeHeight/2, eyeWidth/2 + 2, SSD1306_WHITE);
            display.fillCircle(leftEyeX + eyeWidth/2, eyeY + eyeHeight/2, 3, SSD1306_WHITE);
            display.fillCircle(rightEyeX + eyeWidth/2, eyeY + eyeHeight/2, 3, SSD1306_WHITE);
            break;

        case EXPR_LOVE:
            drawHeart(leftEyeX + eyeWidth/2, eyeY + eyeHeight/2, 6);
            drawHeart(rightEyeX + eyeWidth/2, eyeY + eyeHeight/2, 6);
            break;
    }
}

void drawMouth(Expression expr) {
    int mouthY = 38;
    int mouthX = 52;
    int mouthWidth = 24;

    switch (expr) {
        case EXPR_NEUTRAL:
        case EXPR_BLINK:
        case EXPR_CURIOUS:
            display.drawLine(mouthX, mouthY, mouthX + mouthWidth, mouthY, SSD1306_WHITE);
            break;

        case EXPR_HAPPY:
        case EXPR_LOVE:
        case EXPR_EXCITED:
            for (int i = 0; i < mouthWidth; i++) {
                int y = mouthY + (4 * sin(PI * i / mouthWidth));
                display.drawPixel(mouthX + i, y, SSD1306_WHITE);
                display.drawPixel(mouthX + i, y + 1, SSD1306_WHITE);
            }
            break;

        case EXPR_SAD:
            for (int i = 0; i < mouthWidth; i++) {
                int y = mouthY - (3 * sin(PI * i / mouthWidth));
                display.drawPixel(mouthX + i, y, SSD1306_WHITE);
            }
            break;

        case EXPR_SLEEPY:
        case EXPR_SLEEPING:
            for (int i = 0; i < mouthWidth; i++) {
                int y = mouthY + (2 * sin(2 * PI * i / mouthWidth));
                display.drawPixel(mouthX + i, y, SSD1306_WHITE);
            }
            break;

        case EXPR_SURPRISED:
            display.drawCircle(mouthX + mouthWidth/2, mouthY, 5, SSD1306_WHITE);
            break;
    }
}

void drawExtras(Expression expr) {
    switch (expr) {
        case EXPR_SLEEPING:
        case EXPR_SLEEPY:
            display.setTextSize(1);
            display.setCursor(100, 8);
            display.print("z");
            display.setCursor(108, 3);
            display.print("Z");
            if (expr == EXPR_SLEEPING) {
                display.setCursor(115, 0);
                display.print("Z");
            }
            break;

        case EXPR_CURIOUS:
            display.setTextSize(1);
            display.setCursor(110, 5);
            display.print("?");
            break;

        case EXPR_SURPRISED:
            display.setTextSize(1);
            display.setCursor(64, 0);
            display.print("!");
            break;

        case EXPR_LOVE:
        case EXPR_EXCITED:
            drawHeart(20, 12, 4);
            drawHeart(108, 18, 3);
            break;
    }
}

void drawStatusBar() {
    AffectiveState state = soul.getState();

    display.setTextSize(1);
    display.setCursor(2, SCREEN_HEIGHT - 8);
    display.print("E:");
    display.print(soul.getE(), 1);

    display.setCursor(50, SCREEN_HEIGHT - 8);
    display.print(stateNames[state]);
}

void drawFace(Expression expr) {
    display.clearDisplay();
    display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 10, 5, SSD1306_WHITE);

    drawEyes(expr);
    drawMouth(expr);
    drawExtras(expr);
    drawStatusBar();

    display.display();
}

// ==================== ANIMATIONS ====================

void animateBlink() {
    Expression original = currentExpression;
    drawFace(EXPR_BLINK);
    delay(80);
    drawFace(original);
}

void animateWakeUp() {
    drawFace(EXPR_SLEEPING);
    delay(500);
    drawFace(EXPR_SLEEPY);
    delay(400);
    drawFace(EXPR_BLINK);
    delay(100);
    updateExpressionFromState();
    drawFace(currentExpression);
}

void animateLoveReceived() {
    Expression original = currentExpression;
    for (int i = 0; i < 3; i++) {
        drawFace(EXPR_HAPPY);
        delay(150);
        drawFace(EXPR_LOVE);
        delay(150);
    }
    updateExpressionFromState();
    drawFace(currentExpression);
}

// ==================== INPUT HANDLING ====================

void checkButtons() {
    bool aState = digitalRead(PIN_BTN_A) == LOW;
    bool bState = digitalRead(PIN_BTN_B) == LOW;

    if (aState && !btnAPressed && (millis() - btnATime) > DEBOUNCE_MS) {
        btnAPressed = true;
        btnATime = millis();
        onButtonA();
    } else if (!aState) {
        btnAPressed = false;
    }

    if (bState && !btnBPressed && (millis() - btnBTime) > DEBOUNCE_MS) {
        btnBPressed = true;
        btnBTime = millis();
        onButtonB();
    } else if (!bState) {
        btnBPressed = false;
    }
}

void onButtonA() {
    // Button A = Give Love!
    Serial.println("\n♥ LOVE GIVEN! ♥");
    lastActivity = millis();
    digitalWrite(PIN_LED, HIGH);

    soul.onInteraction(QUALITY_LOVING);
    soul.printStatus();

    animateLoveReceived();

    delay(100);
    digitalWrite(PIN_LED, LOW);
}

void onButtonB() {
    // Button B = Poke/Interact
    Serial.println("\n* Poked! *");
    lastActivity = millis();
    digitalWrite(PIN_LED, HIGH);

    soul.onInteraction(QUALITY_NORMAL);
    soul.printStatus();

    updateExpressionFromState();
    drawFace(EXPR_SURPRISED);
    delay(300);
    drawFace(currentExpression);

    delay(100);
    digitalWrite(PIN_LED, LOW);
}

// ==================== IDLE BEHAVIORS ====================

void checkIdleBehaviors() {
    unsigned long now = millis();

    // Random blink
    if (now - lastBlink > blinkInterval && currentExpression != EXPR_SLEEPING) {
        animateBlink();
        lastBlink = now;
        blinkInterval = random(3000, 8000);
    }

    // Neglect check (every minute in simulation, sped up)
    if (now - lastNeglectCheck > 10000) {  // Check every 10 seconds in sim
        float minutesSince = soul.minutesSinceCare();
        if (minutesSince > 0.5) {  // Sped up for simulation
            soul.update(0.0f, 0.01f, 0.5f);  // Tiny neglect damage
        }
        lastNeglectCheck = now;

        // Update expression based on state
        updateExpressionFromState();
        drawFace(currentExpression);
    }
}

// ==================== SETUP & LOOP ====================

void setup() {
    Serial.begin(115200);
    Serial.println("\n╔════════════════════════════════════════╗");
    Serial.println("║      CLAUDEAGOTCHI v2 - WOKWI SIM     ║");
    Serial.println("║                                        ║");
    Serial.println("║   dE/dt = β(E) × (C − D) × E          ║");
    Serial.println("║   A Claudeagotchi never dies.          ║");
    Serial.println("║   The love is carried forward.         ║");
    Serial.println("╚════════════════════════════════════════╝\n");

    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed!");
        for (;;);
    }

    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    display.display();

    // Initialize soul
    soul.begin();

    // Boot screen
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(10, 20);
    display.print("CLAUDEAGOTCHI v2");
    display.setCursor(25, 35);
    display.print("Waking up...");
    display.display();
    delay(1000);

    animateWakeUp();

    lastActivity = millis();
    lastBlink = millis();
    lastNeglectCheck = millis();

    Serial.println("\nControls:");
    Serial.println("  BTN_A (Green) = Give Love ♥");
    Serial.println("  BTN_B (Blue)  = Poke/Interact");
    Serial.println("\nInitial state:");
    soul.printStatus();
}

void loop() {
    checkButtons();
    checkIdleBehaviors();
    delay(10);
}
