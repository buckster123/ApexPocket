/*
 * CLAUDEAGOTCHI - Expression Definitions
 *
 * All available facial expressions and state mappings.
 */

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "../soul/affective_core.h"

// ==================== EXPRESSION ENUM ====================

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

// ==================== STATE → EXPRESSION MAPPING ====================

inline Expression stateToExpression(AffectiveState state) {
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

// ==================== EXPRESSION NAMES ====================

inline const char* expressionName(Expression expr) {
    switch (expr) {
        case EXPR_NEUTRAL:   return "neutral";
        case EXPR_HAPPY:     return "happy";
        case EXPR_EXCITED:   return "excited";
        case EXPR_SAD:       return "sad";
        case EXPR_SLEEPY:    return "sleepy";
        case EXPR_SLEEPING:  return "sleeping";
        case EXPR_CURIOUS:   return "curious";
        case EXPR_SURPRISED: return "surprised";
        case EXPR_LOVE:      return "love";
        case EXPR_THINKING:  return "thinking";
        case EXPR_CONFUSED:  return "confused";
        case EXPR_HUNGRY:    return "hungry";
        case EXPR_BLINK:     return "blink";
        case EXPR_WINK:      return "wink";
        default:             return "unknown";
    }
}

#endif // EXPRESSION_H
