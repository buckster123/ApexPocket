/*
 * CLAUDEAGOTCHI - Display Manager
 *
 * Abstract display interface with animation support.
 * Handles face rendering, status bar, and smooth transitions.
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "expression.h"
#include "faces.h"
#include "../soul/affective_core.h"

// ==================== ANIMATION TYPES ====================

enum AnimationType {
    ANIM_NONE = 0,
    ANIM_BLINK,
    ANIM_WAKE_UP,
    ANIM_TRANSITION,
    ANIM_TALKING,
    ANIM_THINKING
};

// ==================== ANIMATION STATE ====================

struct AnimationState {
    AnimationType type;
    unsigned long startTime;
    unsigned long frameDuration;
    uint8_t currentFrame;
    uint8_t totalFrames;
    Expression startExpr;
    Expression endExpr;
    bool complete;
};

// ==================== DISPLAY MANAGER CLASS ====================

class DisplayManager {
protected:
    // Current state
    Expression _currentExpression;
    Expression _targetExpression;
    String _statusText;
    String _messageText;
    bool _needsRedraw;

    // Animation
    AnimationState _animation;
    unsigned long _lastBlinkTime;
    unsigned long _blinkInterval;

    // Display dimensions (set by subclass)
    int _width;
    int _height;

    // Auto-blink settings
    bool _autoBlinkEnabled;
    unsigned long _minBlinkInterval;
    unsigned long _maxBlinkInterval;

public:
    DisplayManager();
    virtual ~DisplayManager() {}

    // ==================== LIFECYCLE ====================

    virtual bool begin() = 0;
    void update();  // Call in main loop

    // ==================== EXPRESSION ====================

    void setExpression(Expression expr);
    void setExpressionFromState(AffectiveState state);
    Expression getExpression() const { return _currentExpression; }

    // ==================== TEXT ====================

    void setStatusBar(const char* text);
    void setStatusBar(float E, float floor, AffectiveState state);
    void setMessage(const char* text);
    void clearMessage();

    // ==================== ANIMATIONS ====================

    void blink();
    void wakeUp();
    void transitionTo(Expression expr);
    void showThinking();
    void stopAnimation();

    // Auto-blink control
    void enableAutoBlink(bool enable);
    void setBlinkInterval(unsigned long minMs, unsigned long maxMs);

    // ==================== RENDERING (implement in subclass) ====================

    virtual void clear() = 0;
    virtual void display() = 0;

protected:
    // Subclasses implement these
    virtual void drawEye(int x, int y, EyeType type) = 0;
    virtual void drawMouth(int x, int y, MouthType type) = 0;
    virtual void drawAccessory(int x, int y, char accessory) = 0;
    virtual void drawStatusBar(const char* text) = 0;
    virtual void drawMessage(const char* text) = 0;
    virtual void drawTitle() = 0;

    // Internal rendering
    void renderFace();
    void renderFace(Expression expr);
    void updateAnimation();
    void scheduleNextBlink();
};

// ==================== IMPLEMENTATION ====================

inline DisplayManager::DisplayManager()
    : _currentExpression(EXPR_NEUTRAL)
    , _targetExpression(EXPR_NEUTRAL)
    , _needsRedraw(true)
    , _lastBlinkTime(0)
    , _blinkInterval(3000)
    , _autoBlinkEnabled(true)
    , _minBlinkInterval(2000)
    , _maxBlinkInterval(6000)
{
    _animation.type = ANIM_NONE;
    _animation.complete = true;
}

inline void DisplayManager::update() {
    unsigned long now = millis();

    // Handle auto-blink
    if (_autoBlinkEnabled && _animation.type == ANIM_NONE) {
        if (now - _lastBlinkTime > _blinkInterval) {
            blink();
            scheduleNextBlink();
        }
    }

    // Update running animation
    if (_animation.type != ANIM_NONE) {
        updateAnimation();
        _needsRedraw = true;
    }

    // Redraw if needed
    if (_needsRedraw) {
        clear();
        drawTitle();

        if (_animation.type != ANIM_NONE) {
            // Animation handles its own face rendering
            // (implemented in updateAnimation)
        }

        renderFace();
        drawStatusBar(_statusText.c_str());

        if (_messageText.length() > 0) {
            drawMessage(_messageText.c_str());
        }

        display();
        _needsRedraw = false;
    }
}

inline void DisplayManager::setExpression(Expression expr) {
    if (expr != _currentExpression && _animation.type == ANIM_NONE) {
        _currentExpression = expr;
        _targetExpression = expr;
        _needsRedraw = true;
    }
}

inline void DisplayManager::setExpressionFromState(AffectiveState state) {
    setExpression(stateToExpression(state));
}

inline void DisplayManager::setStatusBar(const char* text) {
    if (_statusText != text) {
        _statusText = text;
        _needsRedraw = true;
    }
}

inline void DisplayManager::setStatusBar(float E, float floor, AffectiveState state) {
    char buf[64];
    snprintf(buf, sizeof(buf), "E:%.1f F:%.1f %s",
             E, floor, AffectiveCore::stateName(state));
    setStatusBar(buf);
}

inline void DisplayManager::setMessage(const char* text) {
    if (_messageText != text) {
        _messageText = text;
        _needsRedraw = true;
    }
}

inline void DisplayManager::clearMessage() {
    if (_messageText.length() > 0) {
        _messageText = "";
        _needsRedraw = true;
    }
}

// ==================== ANIMATIONS ====================

inline void DisplayManager::blink() {
    if (_animation.type != ANIM_NONE) return;

    _animation.type = ANIM_BLINK;
    _animation.startTime = millis();
    _animation.frameDuration = 60;  // 60ms per frame
    _animation.currentFrame = 0;
    _animation.totalFrames = 4;     // open -> closed -> closed -> open
    _animation.startExpr = _currentExpression;
    _animation.endExpr = _currentExpression;
    _animation.complete = false;
    _lastBlinkTime = millis();
}

inline void DisplayManager::wakeUp() {
    stopAnimation();

    _animation.type = ANIM_WAKE_UP;
    _animation.startTime = millis();
    _animation.frameDuration = 300;
    _animation.currentFrame = 0;
    _animation.totalFrames = 5;  // sleeping -> sleepy -> blink -> neutral -> happy
    _animation.startExpr = EXPR_SLEEPING;
    _animation.endExpr = EXPR_HAPPY;
    _animation.complete = false;

    _currentExpression = EXPR_SLEEPING;
    _needsRedraw = true;
}

inline void DisplayManager::transitionTo(Expression expr) {
    if (expr == _currentExpression) return;
    if (_animation.type != ANIM_NONE) {
        stopAnimation();
    }

    _animation.type = ANIM_TRANSITION;
    _animation.startTime = millis();
    _animation.frameDuration = 80;
    _animation.currentFrame = 0;
    _animation.totalFrames = 3;  // current -> blink -> new
    _animation.startExpr = _currentExpression;
    _animation.endExpr = expr;
    _animation.complete = false;
    _targetExpression = expr;
}

inline void DisplayManager::showThinking() {
    _animation.type = ANIM_THINKING;
    _animation.startTime = millis();
    _animation.frameDuration = 400;
    _animation.currentFrame = 0;
    _animation.totalFrames = 255;  // Loop indefinitely
    _animation.startExpr = _currentExpression;
    _animation.complete = false;
    _currentExpression = EXPR_THINKING;
    _needsRedraw = true;
}

inline void DisplayManager::stopAnimation() {
    if (_animation.type == ANIM_TRANSITION) {
        _currentExpression = _animation.endExpr;
    } else if (_animation.type != ANIM_NONE) {
        _currentExpression = _animation.startExpr;
    }
    _animation.type = ANIM_NONE;
    _animation.complete = true;
    _needsRedraw = true;
}

inline void DisplayManager::enableAutoBlink(bool enable) {
    _autoBlinkEnabled = enable;
    if (enable) {
        scheduleNextBlink();
    }
}

inline void DisplayManager::setBlinkInterval(unsigned long minMs, unsigned long maxMs) {
    _minBlinkInterval = minMs;
    _maxBlinkInterval = maxMs;
    scheduleNextBlink();
}

inline void DisplayManager::scheduleNextBlink() {
    _blinkInterval = random(_minBlinkInterval, _maxBlinkInterval);
    _lastBlinkTime = millis();
}

inline void DisplayManager::updateAnimation() {
    unsigned long elapsed = millis() - _animation.startTime;
    uint8_t frame = elapsed / _animation.frameDuration;

    if (frame != _animation.currentFrame) {
        _animation.currentFrame = frame;
        _needsRedraw = true;

        switch (_animation.type) {
            case ANIM_BLINK:
                if (frame >= _animation.totalFrames) {
                    _currentExpression = _animation.startExpr;
                    _animation.type = ANIM_NONE;
                    _animation.complete = true;
                } else if (frame == 1 || frame == 2) {
                    _currentExpression = EXPR_BLINK;
                } else {
                    _currentExpression = _animation.startExpr;
                }
                break;

            case ANIM_WAKE_UP: {
                // sleeping -> sleepy -> blink -> neutral -> happy
                Expression sequence[] = {
                    EXPR_SLEEPING, EXPR_SLEEPY, EXPR_BLINK,
                    EXPR_NEUTRAL, EXPR_HAPPY
                };
                if (frame >= _animation.totalFrames) {
                    _currentExpression = EXPR_HAPPY;
                    _animation.type = ANIM_NONE;
                    _animation.complete = true;
                } else {
                    _currentExpression = sequence[frame];
                }
                break;
            }

            case ANIM_TRANSITION:
                if (frame >= _animation.totalFrames) {
                    _currentExpression = _animation.endExpr;
                    _animation.type = ANIM_NONE;
                    _animation.complete = true;
                } else if (frame == 1) {
                    _currentExpression = EXPR_BLINK;
                } else {
                    _currentExpression = (frame == 0) ?
                        _animation.startExpr : _animation.endExpr;
                }
                break;

            case ANIM_THINKING:
                // Just stays on thinking expression
                // Call stopAnimation() to end
                break;

            default:
                break;
        }
    }
}

inline void DisplayManager::renderFace() {
    renderFace(_currentExpression);
}

inline void DisplayManager::renderFace(Expression expr) {
    if (expr >= EXPR_COUNT) expr = EXPR_NEUTRAL;

    const FaceDefinition& face = FACE_DEFS[expr];

    // Draw eyes
    drawEye(LEFT_EYE_X, EYE_Y, face.leftEye);
    drawEye(RIGHT_EYE_X, EYE_Y, face.rightEye);

    // Draw mouth
    drawMouth(MOUTH_X, MOUTH_Y, face.mouth);

    // Draw accessory if present
    if (face.hasAccessory) {
        drawAccessory(
            FACE_CENTER_X + face.accessoryX,
            face.accessoryY,
            face.accessory
        );
    }
}

#endif // DISPLAY_MANAGER_H
