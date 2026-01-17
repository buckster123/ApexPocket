/*
 * CLAUDEAGOTCHI - Input Handler
 *
 * Button handling with debounce, long press, and combo detection.
 * Supports up to 4 buttons with various interaction patterns.
 */

#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <Arduino.h>

// ==================== CONFIGURATION ====================

#define MAX_BUTTONS         4
#define DEBOUNCE_MS         50      // Debounce time
#define LONG_PRESS_MS       800     // Long press threshold
#define COMBO_WINDOW_MS     200     // Window for detecting combos

// ==================== BUTTON EVENTS ====================

enum ButtonEvent {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_PRESS,        // Just pressed
    BTN_EVENT_RELEASE,      // Just released (short press)
    BTN_EVENT_LONG_PRESS,   // Held for LONG_PRESS_MS
    BTN_EVENT_LONG_RELEASE, // Released after long press
    BTN_EVENT_REPEAT        // Repeating while held (optional)
};

// ==================== BUTTON STATE ====================

struct ButtonState {
    uint8_t pin;
    bool activeLow;         // true = pressed when LOW
    bool enabled;

    // Current state
    bool isPressed;
    bool wasPressed;
    bool longPressTriggered;

    // Timing
    unsigned long pressTime;
    unsigned long lastDebounceTime;

    // Last raw reading
    bool lastReading;
};

// ==================== INPUT HANDLER CLASS ====================

class InputHandler {
private:
    ButtonState _buttons[MAX_BUTTONS];
    uint8_t _buttonCount;

    // Combo detection
    uint8_t _lastButtonPressed;
    unsigned long _lastPressTime;
    bool _comboDetected;
    uint8_t _comboButtons[2];

    // Callbacks
    void (*_onPress)(uint8_t buttonIndex);
    void (*_onRelease)(uint8_t buttonIndex);
    void (*_onLongPress)(uint8_t buttonIndex);
    void (*_onCombo)(uint8_t btn1, uint8_t btn2);

public:
    InputHandler();

    // ==================== SETUP ====================

    void begin();
    uint8_t addButton(uint8_t pin, bool activeLow = true);
    void removeButton(uint8_t index);
    void setEnabled(uint8_t index, bool enabled);

    // ==================== UPDATE ====================

    void update();  // Call in main loop

    // ==================== STATE QUERIES ====================

    bool isPressed(uint8_t index) const;
    bool justPressed(uint8_t index) const;
    bool justReleased(uint8_t index) const;
    bool isLongPress(uint8_t index) const;
    unsigned long pressDuration(uint8_t index) const;

    // Get last event for a button
    ButtonEvent getEvent(uint8_t index) const;

    // Combo detection
    bool isCombo(uint8_t btn1, uint8_t btn2) const;
    bool anyCombo() const { return _comboDetected; }

    // ==================== CALLBACKS ====================

    void onPress(void (*callback)(uint8_t)) { _onPress = callback; }
    void onRelease(void (*callback)(uint8_t)) { _onRelease = callback; }
    void onLongPress(void (*callback)(uint8_t)) { _onLongPress = callback; }
    void onCombo(void (*callback)(uint8_t, uint8_t)) { _onCombo = callback; }

    // ==================== DEBUG ====================

    void printState();

private:
    void updateButton(uint8_t index);
    bool readButton(uint8_t index);
};

// ==================== IMPLEMENTATION ====================

inline InputHandler::InputHandler()
    : _buttonCount(0)
    , _lastButtonPressed(255)
    , _lastPressTime(0)
    , _comboDetected(false)
    , _onPress(nullptr)
    , _onRelease(nullptr)
    , _onLongPress(nullptr)
    , _onCombo(nullptr)
{
    memset(_buttons, 0, sizeof(_buttons));
    memset(_comboButtons, 255, sizeof(_comboButtons));
}

inline void InputHandler::begin() {
    // Configure all added button pins
    for (uint8_t i = 0; i < _buttonCount; i++) {
        if (_buttons[i].enabled) {
            if (_buttons[i].activeLow) {
                pinMode(_buttons[i].pin, INPUT_PULLUP);
            } else {
                pinMode(_buttons[i].pin, INPUT_PULLDOWN);
            }
        }
    }
    Serial.print(F("[Input] Initialized "));
    Serial.print(_buttonCount);
    Serial.println(F(" buttons"));
}

inline uint8_t InputHandler::addButton(uint8_t pin, bool activeLow) {
    if (_buttonCount >= MAX_BUTTONS) {
        Serial.println(F("[Input] Max buttons reached!"));
        return 255;
    }

    uint8_t index = _buttonCount++;

    _buttons[index].pin = pin;
    _buttons[index].activeLow = activeLow;
    _buttons[index].enabled = true;
    _buttons[index].isPressed = false;
    _buttons[index].wasPressed = false;
    _buttons[index].longPressTriggered = false;
    _buttons[index].pressTime = 0;
    _buttons[index].lastDebounceTime = 0;
    _buttons[index].lastReading = false;

    Serial.print(F("[Input] Button "));
    Serial.print(index);
    Serial.print(F(" on pin "));
    Serial.println(pin);

    return index;
}

inline void InputHandler::removeButton(uint8_t index) {
    if (index < _buttonCount) {
        _buttons[index].enabled = false;
    }
}

inline void InputHandler::setEnabled(uint8_t index, bool enabled) {
    if (index < _buttonCount) {
        _buttons[index].enabled = enabled;
    }
}

inline void InputHandler::update() {
    _comboDetected = false;

    for (uint8_t i = 0; i < _buttonCount; i++) {
        if (_buttons[i].enabled) {
            updateButton(i);
        }
    }
}

inline void InputHandler::updateButton(uint8_t index) {
    ButtonState& btn = _buttons[index];
    unsigned long now = millis();

    // Store previous state
    btn.wasPressed = btn.isPressed;

    // Read with debounce
    bool reading = readButton(index);

    if (reading != btn.lastReading) {
        btn.lastDebounceTime = now;
    }
    btn.lastReading = reading;

    if ((now - btn.lastDebounceTime) > DEBOUNCE_MS) {
        // Reading is stable

        if (reading && !btn.isPressed) {
            // Button just pressed
            btn.isPressed = true;
            btn.pressTime = now;
            btn.longPressTriggered = false;

            // Combo detection
            if (_lastButtonPressed != 255 &&
                _lastButtonPressed != index &&
                (now - _lastPressTime) < COMBO_WINDOW_MS) {
                _comboDetected = true;
                _comboButtons[0] = _lastButtonPressed;
                _comboButtons[1] = index;

                if (_onCombo) {
                    _onCombo(_comboButtons[0], _comboButtons[1]);
                }
            }

            _lastButtonPressed = index;
            _lastPressTime = now;

            if (_onPress) {
                _onPress(index);
            }
        }
        else if (!reading && btn.isPressed) {
            // Button just released
            btn.isPressed = false;

            if (_onRelease && !btn.longPressTriggered) {
                _onRelease(index);
            }
        }
    }

    // Long press detection (while still held)
    if (btn.isPressed && !btn.longPressTriggered) {
        if ((now - btn.pressTime) >= LONG_PRESS_MS) {
            btn.longPressTriggered = true;

            if (_onLongPress) {
                _onLongPress(index);
            }
        }
    }
}

inline bool InputHandler::readButton(uint8_t index) {
    bool raw = digitalRead(_buttons[index].pin);
    return _buttons[index].activeLow ? !raw : raw;
}

inline bool InputHandler::isPressed(uint8_t index) const {
    if (index >= _buttonCount) return false;
    return _buttons[index].isPressed;
}

inline bool InputHandler::justPressed(uint8_t index) const {
    if (index >= _buttonCount) return false;
    return _buttons[index].isPressed && !_buttons[index].wasPressed;
}

inline bool InputHandler::justReleased(uint8_t index) const {
    if (index >= _buttonCount) return false;
    return !_buttons[index].isPressed && _buttons[index].wasPressed;
}

inline bool InputHandler::isLongPress(uint8_t index) const {
    if (index >= _buttonCount) return false;
    return _buttons[index].longPressTriggered;
}

inline unsigned long InputHandler::pressDuration(uint8_t index) const {
    if (index >= _buttonCount || !_buttons[index].isPressed) return 0;
    return millis() - _buttons[index].pressTime;
}

inline ButtonEvent InputHandler::getEvent(uint8_t index) const {
    if (index >= _buttonCount) return BTN_EVENT_NONE;

    const ButtonState& btn = _buttons[index];

    if (btn.isPressed && !btn.wasPressed) {
        return BTN_EVENT_PRESS;
    }
    if (!btn.isPressed && btn.wasPressed) {
        return btn.longPressTriggered ? BTN_EVENT_LONG_RELEASE : BTN_EVENT_RELEASE;
    }
    if (btn.isPressed && btn.longPressTriggered && btn.wasPressed) {
        return BTN_EVENT_LONG_PRESS;
    }

    return BTN_EVENT_NONE;
}

inline bool InputHandler::isCombo(uint8_t btn1, uint8_t btn2) const {
    if (!_comboDetected) return false;
    return (_comboButtons[0] == btn1 && _comboButtons[1] == btn2) ||
           (_comboButtons[0] == btn2 && _comboButtons[1] == btn1);
}

inline void InputHandler::printState() {
    Serial.print(F("[Input] Buttons: "));
    for (uint8_t i = 0; i < _buttonCount; i++) {
        Serial.print(_buttons[i].isPressed ? "1" : "0");
    }
    Serial.println();
}

#endif // INPUT_HANDLER_H
