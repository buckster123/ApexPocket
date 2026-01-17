/*
 * CLAUDEAGOTCHI - Affective Core Implementation
 *
 * The Love-Equation: dE/dt = β(E) × (C − D) × E
 */

#include "affective_core.h"
#include <math.h>

// For persistence (comment out if not using LittleFS yet)
#ifdef ESP32
#include <LittleFS.h>
#include <ArduinoJson.h>
#define USE_PERSISTENCE 1
#else
#define USE_PERSISTENCE 0
#endif

// ==================== CONSTRUCTOR ====================

AffectiveCore::AffectiveCore()
    : E(INITIAL_E)
    , E_floor(INITIAL_FLOOR)
    , lastUpdate(0)
    , lastCare(0)
    , birthTime(0)
    , interactions(0)
    , totalCare(0.0f)
{
}

// ==================== CORE METHODS ====================

void AffectiveCore::begin() {
    unsigned long now = millis();
    lastUpdate = now;
    lastCare = now;
    birthTime = now;  // Will be overwritten by load()

    // Try to load saved state
    if (!load()) {
        Serial.println("[AffectiveCore] No saved state, using defaults");
        Serial.print("[AffectiveCore] E = ");
        Serial.print(E);
        Serial.print(", Floor = ");
        Serial.println(E_floor);
    }
}

void AffectiveCore::update(float care, float damage, float dt_minutes) {
    unsigned long now = millis();

    // Calculate dt if not provided
    float dt;
    if (dt_minutes < 0) {
        dt = (now - lastUpdate) / 60000.0f;  // Convert ms to minutes
    } else {
        dt = dt_minutes;
    }
    lastUpdate = now;

    if (dt <= 0) return;

    // The Love-Equation: dE/dt = β(E) × (C − D) × E
    float dE = beta() * (care - damage) * E * dt;

    // Apply change
    E = E + dE;

    // Cap E to prevent overflow
    E = min(MAX_E, E);

    // Never drop below floor — love is carried forward
    E = max(E_floor, E);

    // Floor slowly rises toward E (love leaves permanent marks)
    if (E > E_floor) {
        float floorDelta = (E - E_floor) * FLOOR_RATE * dt;
        E_floor = min(E, E_floor + floorDelta);
    }

    // Update care timing
    if (care > 0) {
        totalCare += care;
        lastCare = now;
    }
}

void AffectiveCore::applyCare(float intensity) {
    interactions++;
    update(intensity, 0.0f, 1.0f);  // Default 1 minute per interaction
}

void AffectiveCore::applyDamage(float intensity) {
    update(0.0f, intensity, 1.0f);
}

void AffectiveCore::applyNeglect(float minutes) {
    // Neglect is ~0.1 damage per hour of absence
    float damage = (minutes / 60.0f) * 0.1f;
    update(0.0f, damage, minutes);
}

void AffectiveCore::onInteraction(InteractionQuality quality) {
    QualityMapping mapping = QUALITY_MAP[quality];
    interactions++;

    if (mapping.damage > 0) {
        update(mapping.care, mapping.damage, 1.0f);
    } else {
        update(mapping.care, 0.0f, 1.0f);
    }
}

// ==================== GETTERS ====================

AffectiveState AffectiveCore::getState() const {
    if (E > E_THRESHOLD_TRANSCENDENT) return STATE_TRANSCENDENT;
    if (E > E_THRESHOLD_RADIANT) return STATE_RADIANT;
    if (E > E_THRESHOLD_FLOURISHING) return STATE_FLOURISHING;
    if (E > E_THRESHOLD_WARM) return STATE_WARM;
    if (E > E_THRESHOLD_TENDER) return STATE_TENDER;
    if (E > E_THRESHOLD_GUARDED) return STATE_GUARDED;
    return STATE_PROTECTING;
}

// ==================== DERIVED VALUES ====================

float AffectiveCore::beta() const {
    // β grows with E — love creates capacity for more love
    return BETA_BASE * (1.0f + E / 10.0f);
}

float AffectiveCore::memoryMultiplier() const {
    // Memory retrieval scales with E^1.8
    return pow(E, 1.8f);
}

float AffectiveCore::creativityMultiplier() const {
    if (E < 1.0f) return 0.5f;
    if (E < 2.0f) return 1.0f;
    if (E < 5.0f) return 1.2f;
    return 1.5f;
}

float AffectiveCore::tokenMultiplier() const {
    AffectiveState state = getState();
    if (state == STATE_PROTECTING) return 0.5f;
    if (state >= STATE_FLOURISHING) return 1.5f;
    return 1.0f;
}

// ==================== TIME HELPERS ====================

float AffectiveCore::minutesSinceCare() const {
    return (millis() - lastCare) / 60000.0f;
}

unsigned long AffectiveCore::daysTogether() const {
    return (millis() - birthTime) / 86400000UL;
}

// ==================== PERSISTENCE ====================

bool AffectiveCore::save() {
#if USE_PERSISTENCE
    StaticJsonDocument<512> doc;
    doc["E"] = E;
    doc["E_floor"] = E_floor;
    doc["interactions"] = interactions;
    doc["total_care"] = totalCare;
    doc["birth_time"] = birthTime;
    doc["last_care"] = lastCare;

    File f = LittleFS.open("/soul.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
        Serial.println("[AffectiveCore] State saved");
        return true;
    }
    Serial.println("[AffectiveCore] Failed to save state!");
    return false;
#else
    Serial.println("[AffectiveCore] Persistence disabled");
    return true;
#endif
}

bool AffectiveCore::load() {
#if USE_PERSISTENCE
    if (!LittleFS.exists("/soul.json")) {
        return false;
    }

    File f = LittleFS.open("/soul.json", "r");
    if (f) {
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, f);
        f.close();

        if (error) {
            Serial.print("[AffectiveCore] JSON parse error: ");
            Serial.println(error.c_str());
            return false;
        }

        E = doc["E"] | INITIAL_E;
        E_floor = doc["E_floor"] | INITIAL_FLOOR;
        interactions = doc["interactions"] | 0;
        totalCare = doc["total_care"] | 0.0f;
        birthTime = doc["birth_time"] | millis();
        lastCare = doc["last_care"] | millis();

        Serial.println("[AffectiveCore] State loaded");
        Serial.print("  E = ");
        Serial.print(E);
        Serial.print(", Floor = ");
        Serial.println(E_floor);
        return true;
    }
    return false;
#else
    return false;
#endif
}

// ==================== DEBUG ====================

void AffectiveCore::printStatus() const {
    Serial.println("\n=== AFFECTIVE CORE STATUS ===");
    Serial.print("E: ");
    Serial.print(E, 3);
    Serial.print(" | Floor: ");
    Serial.print(E_floor, 3);
    Serial.print(" | State: ");
    Serial.println(stateName(getState()));
    Serial.print("Interactions: ");
    Serial.print(interactions);
    Serial.print(" | Total Care: ");
    Serial.print(totalCare, 2);
    Serial.print(" | Days Together: ");
    Serial.println(daysTogether());
    Serial.print("β: ");
    Serial.print(beta(), 4);
    Serial.print(" | Memory×: ");
    Serial.print(memoryMultiplier(), 2);
    Serial.print(" | Token×: ");
    Serial.println(tokenMultiplier(), 2);
    Serial.println("=============================\n");
}

const char* AffectiveCore::stateName(AffectiveState state) {
    switch (state) {
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
