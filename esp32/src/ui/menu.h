/*
 * CLAUDEAGOTCHI - Menu System
 *
 * Simple hierarchical menu for OLED display.
 * Navigate with buttons, select items, adjust values.
 */

#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

// ==================== CONFIGURATION ====================

#define MENU_MAX_ITEMS      8
#define MENU_MAX_DEPTH      3
#define MENU_LINE_HEIGHT    10
#define MENU_VISIBLE_ITEMS  5

// ==================== MENU ITEM TYPES ====================

enum MenuItemType {
    MENU_TYPE_ACTION = 0,   // Triggers a callback
    MENU_TYPE_SUBMENU,      // Opens another menu
    MENU_TYPE_TOGGLE,       // On/Off value
    MENU_TYPE_VALUE,        // Numeric value
    MENU_TYPE_INFO,         // Display only, no action
    MENU_TYPE_BACK          // Go back to parent
};

// ==================== MENU ITEM ====================

struct MenuItem {
    const char* label;
    MenuItemType type;
    void (*action)();           // For ACTION type
    struct Menu* submenu;       // For SUBMENU type
    bool* toggleValue;          // For TOGGLE type
    int* intValue;              // For VALUE type
    int minValue;               // For VALUE type
    int maxValue;               // For VALUE type
    const char* (*getInfo)();   // For INFO type - dynamic text
};

// ==================== MENU ====================

struct Menu {
    const char* title;
    MenuItem items[MENU_MAX_ITEMS];
    uint8_t itemCount;
    Menu* parent;
};

// ==================== MENU SYSTEM CLASS ====================

class MenuSystem {
private:
    Adafruit_SSD1306* _display;
    Menu* _rootMenu;
    Menu* _currentMenu;

    uint8_t _selectedIndex;
    uint8_t _scrollOffset;
    bool _isActive;
    bool _isEditing;  // For VALUE type editing

public:
    MenuSystem(Adafruit_SSD1306* display);

    // ==================== SETUP ====================

    void setRootMenu(Menu* menu);
    void begin();

    // ==================== NAVIGATION ====================

    void show();
    void hide();
    bool isActive() const { return _isActive; }

    void up();
    void down();
    void select();
    void back();

    // ==================== RENDERING ====================

    void render();

private:
    void drawMenu();
    void executeItem(MenuItem& item);
    void adjustValue(MenuItem& item, int delta);
};

// ==================== IMPLEMENTATION ====================

inline MenuSystem::MenuSystem(Adafruit_SSD1306* display)
    : _display(display)
    , _rootMenu(nullptr)
    , _currentMenu(nullptr)
    , _selectedIndex(0)
    , _scrollOffset(0)
    , _isActive(false)
    , _isEditing(false)
{
}

inline void MenuSystem::setRootMenu(Menu* menu) {
    _rootMenu = menu;
    _currentMenu = menu;
}

inline void MenuSystem::begin() {
    _currentMenu = _rootMenu;
    _selectedIndex = 0;
    _scrollOffset = 0;
}

inline void MenuSystem::show() {
    _isActive = true;
    _currentMenu = _rootMenu;
    _selectedIndex = 0;
    _scrollOffset = 0;
    Serial.println(F("[Menu] Opened"));
}

inline void MenuSystem::hide() {
    _isActive = false;
    _isEditing = false;
    Serial.println(F("[Menu] Closed"));
}

inline void MenuSystem::up() {
    if (!_isActive) return;

    if (_isEditing) {
        // Adjust value up
        MenuItem& item = _currentMenu->items[_selectedIndex];
        adjustValue(item, 1);
    } else {
        // Navigate up
        if (_selectedIndex > 0) {
            _selectedIndex--;
            if (_selectedIndex < _scrollOffset) {
                _scrollOffset = _selectedIndex;
            }
        }
    }
}

inline void MenuSystem::down() {
    if (!_isActive) return;

    if (_isEditing) {
        // Adjust value down
        MenuItem& item = _currentMenu->items[_selectedIndex];
        adjustValue(item, -1);
    } else {
        // Navigate down
        if (_selectedIndex < _currentMenu->itemCount - 1) {
            _selectedIndex++;
            if (_selectedIndex >= _scrollOffset + MENU_VISIBLE_ITEMS) {
                _scrollOffset = _selectedIndex - MENU_VISIBLE_ITEMS + 1;
            }
        }
    }
}

inline void MenuSystem::select() {
    if (!_isActive || !_currentMenu) return;

    MenuItem& item = _currentMenu->items[_selectedIndex];

    if (_isEditing) {
        // Exit editing mode
        _isEditing = false;
        Serial.println(F("[Menu] Value saved"));
    } else {
        executeItem(item);
    }
}

inline void MenuSystem::back() {
    if (!_isActive) return;

    if (_isEditing) {
        _isEditing = false;
        return;
    }

    if (_currentMenu->parent) {
        _currentMenu = _currentMenu->parent;
        _selectedIndex = 0;
        _scrollOffset = 0;
        Serial.println(F("[Menu] Back"));
    } else {
        hide();
    }
}

inline void MenuSystem::executeItem(MenuItem& item) {
    switch (item.type) {
        case MENU_TYPE_ACTION:
            if (item.action) {
                Serial.print(F("[Menu] Action: "));
                Serial.println(item.label);
                item.action();
            }
            break;

        case MENU_TYPE_SUBMENU:
            if (item.submenu) {
                item.submenu->parent = _currentMenu;
                _currentMenu = item.submenu;
                _selectedIndex = 0;
                _scrollOffset = 0;
                Serial.print(F("[Menu] Enter: "));
                Serial.println(item.submenu->title);
            }
            break;

        case MENU_TYPE_TOGGLE:
            if (item.toggleValue) {
                *item.toggleValue = !(*item.toggleValue);
                Serial.print(F("[Menu] Toggle: "));
                Serial.println(*item.toggleValue ? "ON" : "OFF");
            }
            break;

        case MENU_TYPE_VALUE:
            _isEditing = true;
            Serial.print(F("[Menu] Editing: "));
            Serial.println(item.label);
            break;

        case MENU_TYPE_BACK:
            back();
            break;

        case MENU_TYPE_INFO:
            // No action
            break;
    }
}

inline void MenuSystem::adjustValue(MenuItem& item, int delta) {
    if (item.type != MENU_TYPE_VALUE || !item.intValue) return;

    *item.intValue += delta;

    // Clamp to range
    if (*item.intValue < item.minValue) *item.intValue = item.minValue;
    if (*item.intValue > item.maxValue) *item.intValue = item.maxValue;
}

inline void MenuSystem::render() {
    if (!_isActive || !_display || !_currentMenu) return;

    _display->clearDisplay();

    // Title bar
    _display->setTextSize(1);
    _display->setTextColor(SSD1306_WHITE);
    _display->setCursor(0, 0);
    _display->print(_currentMenu->title);
    _display->drawFastHLine(0, 9, 128, SSD1306_WHITE);

    // Menu items
    uint8_t y = 12;
    for (uint8_t i = _scrollOffset;
         i < min((uint8_t)(_scrollOffset + MENU_VISIBLE_ITEMS), _currentMenu->itemCount);
         i++) {

        MenuItem& item = _currentMenu->items[i];

        // Selection indicator
        if (i == _selectedIndex) {
            _display->fillRect(0, y - 1, 128, MENU_LINE_HEIGHT, SSD1306_WHITE);
            _display->setTextColor(SSD1306_BLACK);
        } else {
            _display->setTextColor(SSD1306_WHITE);
        }

        _display->setCursor(4, y);
        _display->print(item.label);

        // Show values/states on right side
        switch (item.type) {
            case MENU_TYPE_TOGGLE:
                _display->setCursor(100, y);
                if (item.toggleValue) {
                    _display->print(*item.toggleValue ? F("ON") : F("OFF"));
                }
                break;

            case MENU_TYPE_VALUE:
                _display->setCursor(90, y);
                if (item.intValue) {
                    if (_isEditing && i == _selectedIndex) {
                        _display->print(F("<"));
                        _display->print(*item.intValue);
                        _display->print(F(">"));
                    } else {
                        _display->print(*item.intValue);
                    }
                }
                break;

            case MENU_TYPE_SUBMENU:
                _display->setCursor(118, y);
                _display->print(F(">"));
                break;

            case MENU_TYPE_INFO:
                if (item.getInfo) {
                    _display->setCursor(60, y);
                    _display->print(item.getInfo());
                }
                break;

            default:
                break;
        }

        y += MENU_LINE_HEIGHT;
    }

    // Scroll indicators
    if (_scrollOffset > 0) {
        _display->setCursor(120, 12);
        _display->setTextColor(SSD1306_WHITE);
        _display->print(F("^"));
    }
    if (_scrollOffset + MENU_VISIBLE_ITEMS < _currentMenu->itemCount) {
        _display->setCursor(120, 54);
        _display->setTextColor(SSD1306_WHITE);
        _display->print(F("v"));
    }

    _display->display();
}

#endif // MENU_H
