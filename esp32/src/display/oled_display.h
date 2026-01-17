/*
 * CLAUDEAGOTCHI - OLED Display Implementation
 *
 * SSD1306 128x64 OLED driver using Adafruit library.
 * Implements DisplayManager interface with bitmap rendering.
 */

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "display_manager.h"
#include "faces.h"

// ==================== OLED CONFIGURATION ====================

#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_RESET       -1     // No reset pin
#define OLED_I2C_ADDR    0x3C   // Common address (try 0x3D if doesn't work)

// Layout
#define TITLE_Y          0
#define FACE_AREA_TOP    10
#define STATUS_Y         56
#define MESSAGE_Y        48

// ==================== OLED DISPLAY CLASS ====================

class OLEDDisplay : public DisplayManager {
private:
    Adafruit_SSD1306* _display;
    bool _externalDisplay;  // If true, don't delete in destructor

public:
    OLEDDisplay();
    OLEDDisplay(Adafruit_SSD1306* existingDisplay);
    ~OLEDDisplay();

    // Lifecycle
    bool begin() override;
    bool begin(uint8_t i2cAddr);

    // Raw display access (for custom drawing)
    Adafruit_SSD1306* getDisplay() { return _display; }

    // DisplayManager interface
    void clear() override;
    void display() override;

protected:
    void drawEye(int x, int y, EyeType type) override;
    void drawMouth(int x, int y, MouthType type) override;
    void drawAccessory(int x, int y, char accessory) override;
    void drawStatusBar(const char* text) override;
    void drawMessage(const char* text) override;
    void drawTitle() override;

private:
    void drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h);
};

// ==================== IMPLEMENTATION ====================

inline OLEDDisplay::OLEDDisplay()
    : DisplayManager()
    , _display(nullptr)
    , _externalDisplay(false)
{
    _width = OLED_WIDTH;
    _height = OLED_HEIGHT;
}

inline OLEDDisplay::OLEDDisplay(Adafruit_SSD1306* existingDisplay)
    : DisplayManager()
    , _display(existingDisplay)
    , _externalDisplay(true)
{
    _width = OLED_WIDTH;
    _height = OLED_HEIGHT;
}

inline OLEDDisplay::~OLEDDisplay() {
    if (_display && !_externalDisplay) {
        delete _display;
    }
}

inline bool OLEDDisplay::begin() {
    return begin(OLED_I2C_ADDR);
}

inline bool OLEDDisplay::begin(uint8_t i2cAddr) {
    if (!_display) {
        _display = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
    }

    if (!_display->begin(SSD1306_SWITCHCAPVCC, i2cAddr)) {
        Serial.println("[OLEDDisplay] SSD1306 init failed!");
        return false;
    }

    _display->clearDisplay();
    _display->setTextColor(SSD1306_WHITE);
    _display->setTextSize(1);
    _display->cp437(true);  // Use correct 437 codepage

    Serial.println("[OLEDDisplay] Initialized");
    return true;
}

inline void OLEDDisplay::clear() {
    _display->clearDisplay();
}

inline void OLEDDisplay::display() {
    _display->display();
}

// ==================== DRAWING HELPERS ====================

inline void OLEDDisplay::drawBitmap(int x, int y, const uint8_t* bitmap, int w, int h) {
    // Draw 1-bit bitmap from PROGMEM
    _display->drawBitmap(x - w/2, y - h/2, bitmap, w, h, SSD1306_WHITE);
}

inline void OLEDDisplay::drawEye(int x, int y, EyeType type) {
    const uint8_t* bitmap = nullptr;

    switch (type) {
        case EYE_NORMAL:
            bitmap = EYE_NORMAL_BITMAP;
            break;
        case EYE_CLOSED:
        case EYE_WINK:  // Same as closed for single eye
            bitmap = EYE_CLOSED_BITMAP;
            break;
        case EYE_HAPPY:
            bitmap = EYE_HAPPY_BITMAP;
            break;
        case EYE_STAR:
            bitmap = EYE_STAR_BITMAP;
            break;
        case EYE_WIDE:
            bitmap = EYE_WIDE_BITMAP;
            break;
        case EYE_HEART:
            bitmap = EYE_HEART_BITMAP;
            break;
        case EYE_SLEEPY:
            // Draw closed line on top, normal below
            bitmap = EYE_CLOSED_BITMAP;
            drawBitmap(x, y - 2, bitmap, EYE_WIDTH, EYE_HEIGHT);
            bitmap = EYE_NORMAL_BITMAP;
            y += 2;
            break;
        case EYE_CURIOUS:
            bitmap = EYE_CURIOUS_BITMAP;
            break;
        case EYE_SPIRAL:
            bitmap = EYE_SPIRAL_BITMAP;
            break;
        case EYE_HALF_LEFT:
            bitmap = EYE_HALF_LEFT_BITMAP;
            break;
        default:
            bitmap = EYE_NORMAL_BITMAP;
            break;
    }

    if (bitmap) {
        drawBitmap(x, y, bitmap, EYE_WIDTH, EYE_HEIGHT);
    }
}

inline void OLEDDisplay::drawMouth(int x, int y, MouthType type) {
    const uint8_t* bitmap = nullptr;

    switch (type) {
        case MOUTH_NEUTRAL:
            bitmap = MOUTH_NEUTRAL_BITMAP;
            break;
        case MOUTH_SMILE:
            bitmap = MOUTH_SMILE_BITMAP;
            break;
        case MOUTH_BIG_SMILE:
            bitmap = MOUTH_BIG_SMILE_BITMAP;
            break;
        case MOUTH_FROWN:
            bitmap = MOUTH_FROWN_BITMAP;
            break;
        case MOUTH_OPEN:
            bitmap = MOUTH_OPEN_BITMAP;
            break;
        case MOUTH_SMALL_O:
            bitmap = MOUTH_SMALL_O_BITMAP;
            break;
        case MOUTH_WAVY:
            bitmap = MOUTH_WAVY_BITMAP;
            break;
        case MOUTH_SLEEPY:
            bitmap = MOUTH_SLEEPY_BITMAP;
            break;
        case MOUTH_HUNGRY:
            bitmap = MOUTH_HUNGRY_BITMAP;
            break;
        case MOUTH_KISS:
            // Draw small heart
            _display->drawBitmap(x - 4, y - 3, SMALL_HEART, 8, 6, SSD1306_WHITE);
            return;
        default:
            bitmap = MOUTH_NEUTRAL_BITMAP;
            break;
    }

    if (bitmap) {
        drawBitmap(x, y, bitmap, MOUTH_WIDTH, MOUTH_HEIGHT);
    }
}

inline void OLEDDisplay::drawAccessory(int x, int y, char accessory) {
    _display->setTextSize(1);
    _display->setCursor(x - 3, y);

    switch (accessory) {
        case '!':
            _display->print(F("!"));
            break;
        case '?':
            _display->print(F("?"));
            break;
        case 'z':
            _display->print(F("z"));
            break;
        case 'Z':
            _display->print(F("Z"));
            // Add extra z
            _display->setCursor(x - 10, y + 6);
            _display->print(F("z"));
            break;
        case '.':
            // Thinking dots
            _display->setCursor(x - 3, y);
            _display->print(F("."));
            _display->setCursor(x + 2, y - 4);
            _display->print(F("."));
            _display->setCursor(x + 6, y - 8);
            _display->print(F("."));
            break;
        default:
            _display->print(accessory);
            break;
    }
}

inline void OLEDDisplay::drawTitle() {
    _display->setTextSize(1);
    _display->setCursor(22, TITLE_Y);
    _display->print(F("CLAUDEAGOTCHI"));
}

inline void OLEDDisplay::drawStatusBar(const char* text) {
    if (!text || strlen(text) == 0) return;

    _display->setTextSize(1);
    _display->setCursor(0, STATUS_Y);
    _display->print(text);
}

inline void OLEDDisplay::drawMessage(const char* text) {
    if (!text || strlen(text) == 0) return;

    // Draw separator line
    _display->drawFastHLine(0, MESSAGE_Y - 2, OLED_WIDTH, SSD1306_WHITE);

    // Draw message (truncate if too long)
    _display->setTextSize(1);
    _display->setCursor(0, MESSAGE_Y);

    // Max ~21 chars per line at size 1
    char buf[22];
    strncpy(buf, text, 21);
    buf[21] = '\0';
    _display->print(buf);
}

#endif // OLED_DISPLAY_H
