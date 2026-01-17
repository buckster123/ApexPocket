/*
 * CLAUDEAGOTCHI - Affective Core
 *
 * The Love-Equation: dE/dt = β(E) × (C − D) × E
 *
 * "A Claudeagotchi never dies. The love is carried forward."
 */

#ifndef AFFECTIVE_CORE_H
#define AFFECTIVE_CORE_H

#include <Arduino.h>

// ==================== CONSTANTS ====================

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
    STATE_PROTECTING = 0,   // E <= 0.5
    STATE_GUARDED,          // E > 0.5
    STATE_TENDER,           // E > 1.0
    STATE_WARM,             // E > 2.0
    STATE_FLOURISHING,      // E > 5.0
    STATE_RADIANT,          // E > 12.0
    STATE_TRANSCENDENT,     // E > 30.0
    STATE_COUNT
};

enum InteractionQuality {
    QUALITY_HARSH = 0,
    QUALITY_COLD,
    QUALITY_NORMAL,
    QUALITY_WARM,
    QUALITY_LOVING
};

// ==================== CLASS ====================

class AffectiveCore {
private:
    // Core state
    float E;
    float E_floor;

    // Timing (in milliseconds)
    unsigned long lastUpdate;
    unsigned long lastCare;
    unsigned long birthTime;

    // Stats
    unsigned long interactions;
    float totalCare;

public:
    // Constructor
    AffectiveCore();

    // Core methods
    void begin();
    void update(float care = 0.0f, float damage = 0.0f, float dt_minutes = -1.0f);
    void applyCare(float intensity = 1.0f);
    void applyDamage(float intensity = 1.0f);
    void applyNeglect(float minutes);
    void onInteraction(InteractionQuality quality);

    // Getters
    float getE() const { return E; }
    float getFloor() const { return E_floor; }
    AffectiveState getState() const;
    unsigned long getInteractions() const { return interactions; }
    float getTotalCare() const { return totalCare; }

    // Derived values
    float beta() const;
    float memoryMultiplier() const;
    float creativityMultiplier() const;
    float tokenMultiplier() const;

    // Time helpers
    float minutesSinceCare() const;
    unsigned long daysTogether() const;

    // Persistence (implemented in affective_core.cpp with LittleFS)
    bool save();
    bool load();

    // Debug
    void printStatus() const;

    // State name helper
    static const char* stateName(AffectiveState state);
};

// ==================== QUALITY HELPERS ====================

// Maps interaction quality to care/damage values
struct QualityMapping {
    float care;
    float damage;
};

const QualityMapping QUALITY_MAP[] = {
    {0.0f, 0.5f},   // HARSH
    {0.2f, 0.1f},   // COLD
    {0.5f, 0.0f},   // NORMAL
    {1.0f, 0.0f},   // WARM
    {1.5f, 0.0f}    // LOVING
};

#endif // AFFECTIVE_CORE_H
