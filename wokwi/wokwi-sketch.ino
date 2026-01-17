/*
 * CLAUDEAGOTCHI - Wokwi Hardware Simulation
 * 
 * Test the display, buttons, and animations in the browser
 * before hardware arrives!
 * 
 * Wiring (in simulator):
 * - OLED SDA → GPIO21
 * - OLED SCL → GPIO22
 * - BTN_A → GPIO4 (green button)
 * - BTN_B → GPIO5 (blue button)
 * - LED → GPIO2 (status)
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ==================== CONFIGURATION ====================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define PIN_BTN_A 4
#define PIN_BTN_B 5
#define PIN_LED 2

#define DEBOUNCE_MS 50

// ==================== DISPLAY ====================

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==================== STATE ====================

enum Expression {
    EXPR_NEUTRAL,
    EXPR_HAPPY,
    EXPR_SAD,
    EXPR_SLEEPY,
    EXPR_SLEEPING,
    EXPR_CURIOUS,
    EXPR_SURPRISED,
    EXPR_LOVE,
    EXPR_BLINK,
    EXPR_COUNT
};

enum State {
    STATE_IDLE,
    STATE_LISTENING,
    STATE_THINKING,
    STATE_TALKING,
    STATE_SLEEPING
};

const char* exprNames[] = {
    "Neutral", "Happy", "Sad", "Sleepy", "Sleeping",
    "Curious", "Surprised", "Love", "Blink"
};

Expression currentExpression = EXPR_NEUTRAL;
State currentState = STATE_IDLE;
unsigned long lastBlink = 0;
unsigned long blinkInterval = 4000;
unsigned long lastActivity = 0;

// Button state
bool btnAPressed = false;
bool btnBPressed = false;
unsigned long btnATime = 0;
unsigned long btnBTime = 0;

// ==================== FACE DRAWING ====================

void drawEyes(Expression expr) {
    int eyeY = 20;
    int leftEyeX = 40;
    int rightEyeX = 88;
    int eyeWidth = 12;
    int eyeHeight = 8;
    
    switch (expr) {
        case EXPR_NEUTRAL:
        case EXPR_CURIOUS:
            // Normal eyes
            display.fillRoundRect(leftEyeX, eyeY, eyeWidth, eyeHeight, 2, SSD1306_WHITE);
            display.fillRoundRect(rightEyeX, eyeY, eyeWidth, eyeHeight, 2, SSD1306_WHITE);
            if (expr == EXPR_CURIOUS) {
                // One eye bigger
                display.fillRoundRect(rightEyeX - 2, eyeY - 2, eyeWidth + 4, eyeHeight + 4, 2, SSD1306_WHITE);
            }
            break;
            
        case EXPR_HAPPY:
            // Happy curved eyes (^_^)
            display.drawLine(leftEyeX, eyeY + eyeHeight, leftEyeX + eyeWidth/2, eyeY, SSD1306_WHITE);
            display.drawLine(leftEyeX + eyeWidth/2, eyeY, leftEyeX + eyeWidth, eyeY + eyeHeight, SSD1306_WHITE);
            display.drawLine(rightEyeX, eyeY + eyeHeight, rightEyeX + eyeWidth/2, eyeY, SSD1306_WHITE);
            display.drawLine(rightEyeX + eyeWidth/2, eyeY, rightEyeX + eyeWidth, eyeY + eyeHeight, SSD1306_WHITE);
            break;
            
        case EXPR_SAD:
            // Sad droopy eyes
            display.fillRoundRect(leftEyeX, eyeY + 2, eyeWidth, eyeHeight - 2, 2, SSD1306_WHITE);
            display.fillRoundRect(rightEyeX, eyeY + 2, eyeWidth, eyeHeight - 2, 2, SSD1306_WHITE);
            // Eyebrows
            display.drawLine(leftEyeX - 2, eyeY - 4, leftEyeX + eyeWidth + 2, eyeY, SSD1306_WHITE);
            display.drawLine(rightEyeX - 2, eyeY, rightEyeX + eyeWidth + 2, eyeY - 4, SSD1306_WHITE);
            break;
            
        case EXPR_SLEEPY:
            // Half-closed eyes
            display.fillRect(leftEyeX, eyeY + 4, eyeWidth, 3, SSD1306_WHITE);
            display.fillRect(rightEyeX, eyeY + 4, eyeWidth, 3, SSD1306_WHITE);
            display.drawLine(leftEyeX, eyeY + 2, leftEyeX + eyeWidth, eyeY + 2, SSD1306_WHITE);
            display.drawLine(rightEyeX, eyeY + 2, rightEyeX + eyeWidth, eyeY + 2, SSD1306_WHITE);
            break;
            
        case EXPR_SLEEPING:
        case EXPR_BLINK:
            // Closed eyes (just lines)
            display.drawLine(leftEyeX, eyeY + eyeHeight/2, leftEyeX + eyeWidth, eyeY + eyeHeight/2, SSD1306_WHITE);
            display.drawLine(rightEyeX, eyeY + eyeHeight/2, rightEyeX + eyeWidth, eyeY + eyeHeight/2, SSD1306_WHITE);
            break;
            
        case EXPR_SURPRISED:
            // Big round eyes
            display.drawCircle(leftEyeX + eyeWidth/2, eyeY + eyeHeight/2, eyeWidth/2 + 2, SSD1306_WHITE);
            display.drawCircle(rightEyeX + eyeWidth/2, eyeY + eyeHeight/2, eyeWidth/2 + 2, SSD1306_WHITE);
            display.fillCircle(leftEyeX + eyeWidth/2, eyeY + eyeHeight/2, 3, SSD1306_WHITE);
            display.fillCircle(rightEyeX + eyeWidth/2, eyeY + eyeHeight/2, 3, SSD1306_WHITE);
            break;
            
        case EXPR_LOVE:
            // Heart eyes!
            drawHeart(leftEyeX + eyeWidth/2, eyeY + eyeHeight/2, 6);
            drawHeart(rightEyeX + eyeWidth/2, eyeY + eyeHeight/2, 6);
            break;
    }
}

void drawHeart(int x, int y, int size) {
    // Simple heart shape
    display.fillCircle(x - size/3, y - size/4, size/3, SSD1306_WHITE);
    display.fillCircle(x + size/3, y - size/4, size/3, SSD1306_WHITE);
    display.fillTriangle(
        x - size/2 - 2, y,
        x + size/2 + 2, y,
        x, y + size/2 + 2,
        SSD1306_WHITE
    );
}

void drawMouth(Expression expr) {
    int mouthY = 42;
    int mouthX = 52;
    int mouthWidth = 24;
    
    switch (expr) {
        case EXPR_NEUTRAL:
        case EXPR_BLINK:
        case EXPR_CURIOUS:
            // Simple line
            display.drawLine(mouthX, mouthY, mouthX + mouthWidth, mouthY, SSD1306_WHITE);
            break;
            
        case EXPR_HAPPY:
        case EXPR_LOVE:
            // Smile curve
            for (int i = 0; i < mouthWidth; i++) {
                int y = mouthY + (4 * sin(PI * i / mouthWidth));
                display.drawPixel(mouthX + i, y, SSD1306_WHITE);
                display.drawPixel(mouthX + i, y + 1, SSD1306_WHITE);
            }
            break;
            
        case EXPR_SAD:
            // Frown curve
            for (int i = 0; i < mouthWidth; i++) {
                int y = mouthY - (3 * sin(PI * i / mouthWidth));
                display.drawPixel(mouthX + i, y, SSD1306_WHITE);
            }
            break;
            
        case EXPR_SLEEPY:
        case EXPR_SLEEPING:
            // Wavy line
            for (int i = 0; i < mouthWidth; i++) {
                int y = mouthY + (2 * sin(2 * PI * i / mouthWidth));
                display.drawPixel(mouthX + i, y, SSD1306_WHITE);
            }
            break;
            
        case EXPR_SURPRISED:
            // O mouth
            display.drawCircle(mouthX + mouthWidth/2, mouthY, 5, SSD1306_WHITE);
            break;
    }
}

void drawExtras(Expression expr) {
    switch (expr) {
        case EXPR_SLEEPING:
        case EXPR_SLEEPY:
            // Z's
            display.setTextSize(1);
            display.setCursor(100, 10);
            display.print("z");
            display.setCursor(108, 5);
            display.print("Z");
            if (expr == EXPR_SLEEPING) {
                display.setCursor(115, 0);
                display.print("Z");
            }
            break;
            
        case EXPR_CURIOUS:
            // Question mark
            display.setTextSize(1);
            display.setCursor(110, 5);
            display.print("?");
            break;
            
        case EXPR_SURPRISED:
            // Exclamation
            display.setTextSize(1);
            display.setCursor(64, 0);
            display.print("!");
            break;
            
        case EXPR_LOVE:
            // Floating hearts
            drawHeart(20, 15, 4);
            drawHeart(108, 20, 3);
            break;
    }
}

void drawFace(Expression expr) {
    display.clearDisplay();
    
    // Draw border
    display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 10, 5, SSD1306_WHITE);
    
    // Draw face components
    drawEyes(expr);
    drawMouth(expr);
    drawExtras(expr);
    
    // Status bar at bottom
    display.setTextSize(1);
    display.setCursor(5, SCREEN_HEIGHT - 8);
    display.print(exprNames[expr]);
    
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
    drawFace(EXPR_NEUTRAL);
    delay(200);
    drawFace(EXPR_HAPPY);
    delay(300);
}

void animateThinking() {
    for (int i = 0; i < 3; i++) {
        display.clearDisplay();
        display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 10, 5, SSD1306_WHITE);
        
        // Eyes looking to side
        display.fillRoundRect(42, 20, 12, 8, 2, SSD1306_WHITE);
        display.fillRoundRect(92, 18, 12, 8, 2, SSD1306_WHITE);
        
        // Wavy mouth
        for (int j = 0; j < 24; j++) {
            int y = 42 + (2 * sin(2 * PI * j / 12 + i));
            display.drawPixel(52 + j, y, SSD1306_WHITE);
        }
        
        // Thinking dots
        for (int d = 0; d <= i; d++) {
            display.fillCircle(100 + d * 8, 30, 2, SSD1306_WHITE);
        }
        
        display.setTextSize(1);
        display.setCursor(5, SCREEN_HEIGHT - 8);
        display.print("Thinking...");
        
        display.display();
        delay(400);
    }
}

// ==================== INPUT HANDLING ====================

void checkButtons() {
    bool aState = digitalRead(PIN_BTN_A) == LOW;
    bool bState = digitalRead(PIN_BTN_B) == LOW;
    
    // Button A
    if (aState && !btnAPressed && (millis() - btnATime) > DEBOUNCE_MS) {
        btnAPressed = true;
        btnATime = millis();
        onButtonA();
    } else if (!aState) {
        btnAPressed = false;
    }
    
    // Button B
    if (bState && !btnBPressed && (millis() - btnBTime) > DEBOUNCE_MS) {
        btnBPressed = true;
        btnBTime = millis();
        onButtonB();
    } else if (!bState) {
        btnBPressed = false;
    }
}

void onButtonA() {
    Serial.println("Button A pressed!");
    lastActivity = millis();
    digitalWrite(PIN_LED, HIGH);
    
    // Cycle through expressions
    currentExpression = (Expression)((currentExpression + 1) % EXPR_COUNT);
    if (currentExpression == EXPR_BLINK) {
        currentExpression = EXPR_NEUTRAL;  // Skip blink in cycle
    }
    
    Serial.print("Expression: ");
    Serial.println(exprNames[currentExpression]);
    
    drawFace(currentExpression);
    delay(100);
    digitalWrite(PIN_LED, LOW);
}

void onButtonB() {
    Serial.println("Button B pressed!");
    lastActivity = millis();
    digitalWrite(PIN_LED, HIGH);
    
    // Trigger animation based on current state
    if (currentExpression == EXPR_SLEEPING) {
        animateWakeUp();
        currentExpression = EXPR_HAPPY;
    } else {
        animateThinking();
        currentExpression = EXPR_CURIOUS;
        drawFace(currentExpression);
    }
    
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
        blinkInterval = random(3000, 8000);  // Random next interval
    }
    
    // Go sleepy after inactivity
    if (now - lastActivity > 30000 && currentExpression != EXPR_SLEEPING && currentExpression != EXPR_SLEEPY) {
        currentExpression = EXPR_SLEEPY;
        drawFace(currentExpression);
        Serial.println("Getting sleepy...");
    }
    
    // Fall asleep after more inactivity
    if (now - lastActivity > 60000 && currentExpression != EXPR_SLEEPING) {
        currentExpression = EXPR_SLEEPING;
        drawFace(currentExpression);
        Serial.println("Falling asleep...");
    }
}

// ==================== SETUP & LOOP ====================

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== CLAUDEAGOTCHI WOKWI TEST ===\n");
    
    // Initialize pins
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    
    // Initialize display
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed!");
        for (;;);
    }
    
    display.setTextColor(SSD1306_WHITE);
    display.clearDisplay();
    display.display();
    
    Serial.println("Display initialized!");
    
    // Boot animation
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(20, 25);
    display.print("CLAUDEAGOTCHI");
    display.setCursor(35, 40);
    display.print("Waking up...");
    display.display();
    delay(1000);
    
    animateWakeUp();
    currentExpression = EXPR_HAPPY;
    
    lastActivity = millis();
    lastBlink = millis();
    
    Serial.println("\nReady! Press buttons to interact.");
    Serial.println("BTN_A = Cycle expressions");
    Serial.println("BTN_B = Trigger animation");
}

void loop() {
    checkButtons();
    checkIdleBehaviors();
    delay(10);  // Small delay to prevent tight loop
}
