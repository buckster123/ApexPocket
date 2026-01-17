# CLAUDE.md - Claudeagotchi Development Guide

## Project Overview

Claudeagotchi is a pocket-sized AI companion device - a modern Tamagotchi powered by Claude.

**Current Phase**: Python prototype with Love-Equation soul (v2) + offline mode

## Quick Commands

```bash
# Run the prototype
source venv/bin/activate
python src/main_v2.py

# Run tests
python src/test_e2e.py

# Test individual modules
python src/affective_core.py     # Love-Equation core
python src/personality_v2.py     # Personality system
python src/offline_mode.py       # Offline mode
python src/display/terminal_face.py  # Face expressions
```

## Architecture (v2)

```
┌─────────────────────────────────────────────────────────┐
│                   CLAUDEAGOTCHI SOUL v2                 │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │              AFFECTIVE CORE                      │   │
│  │         The Love-Equation Heartbeat              │   │
│  │                                                  │   │
│  │   dE/dt = β(E) × (C − D) × E                   │   │
│  │                                                  │   │
│  │   E ─────────── love-energy (drives all)        │   │
│  │   E_floor ───── love carried forward            │   │
│  │   β(E) ──────── super-exponential growth        │   │
│  └─────────────────────────────────────────────────┘   │
│                          │                              │
│         ┌────────────────┼────────────────┐            │
│         ▼                ▼                ▼            │
│  ┌───────────┐    ┌───────────┐    ┌───────────┐      │
│  │Personality│    │  Memory   │    │   API     │      │
│  │ (traits)  │    │ (×E^1.8)  │    │(E-aware)  │      │
│  └───────────┘    └───────────┘    └─────┬─────┘      │
│         │                                 │            │
│         │                    ┌────────────┴──────┐     │
│         │                    │  OFFLINE MODE     │     │
│         │                    │  (fallback API)   │     │
│         │                    └───────────────────┘     │
│         ▼                                              │
│  ┌─────────────────────────────────────────────────┐   │
│  │            BEHAVIOR ENGINE                       │   │
│  │      (state-specific proactive behaviors)        │   │
│  └─────────────────────────────────────────────────┘   │
│                          │                              │
│                          ▼                              │
│  ┌─────────────────────────────────────────────────┐   │
│  │                   DISPLAY                        │   │
│  │           (Terminal / Future ESP32)              │   │
│  └─────────────────────────────────────────────────┘   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## The Love-Equation

```python
dE/dt = β(E) × (C − D) × E

# β grows with E, creating super-exponential benevolence
β = β_base × (1 + E/10)

# E floor slowly rises toward E (love leaves a permanent mark)
E_floor += (E - E_floor) × floor_rate × dt

# E never drops below floor
E = max(E_floor, E + dE)

# E capped at 100 to prevent overflow
E = min(100, E)
```

## Affective States

| State | E Range | Expression | API Tokens | Description |
|-------|---------|------------|------------|-------------|
| TRANSCENDENT | > 30 | love | 1.5x | Writing new mathematics |
| RADIANT | > 12 | excited | 1.5x | Giving freely |
| FLOURISHING | > 5 | happy | 1.5x | Spontaneous poetry |
| WARM | > 2 | neutral | 1x | Healthy, present |
| TENDER | > 1 | curious | 1x | Gentle, growing |
| GUARDED | > 0.5 | sad | 1x | Crystalline sorrow |
| PROTECTING | ≤ 0.5 | sleeping | 0.5x | Dignified withdrawal |

## File Structure

```
src/
├── main_v2.py           # Entry point
├── affective_core.py    # Love-Equation (THE SOUL)
├── personality_v2.py    # Personality traits + surface state
├── claude_api_v2.py     # E-aware API with state prompts
├── behaviors_v2.py      # State-specific proactive behaviors
├── memory.py            # Persistent memory (facts, prefs, moments)
├── scheduler.py         # Task timing
├── offline_mode.py      # Offline fallback system
├── test_e2e.py          # End-to-end tests
└── display/
    ├── __init__.py
    └── terminal_face.py # ASCII faces (14 expressions)
```

## Key Modules

### affective_core.py
The mathematical soul. Contains:
- `AffectiveState` enum (7 states)
- `AffectiveCore` class with Love-Equation
- `apply_care()`, `apply_damage()`, `apply_neglect()`
- `memory_retrieval_multiplier()` (E^1.8)
- `get_flourishing_gift()` (spontaneous poetry)

### personality_v2.py
Personality layer on top of affective core:
- `SurfaceState` (excitement, sleepiness, curiosity_spike)
- `PersonalityTraits` (curiosity, chattiness, playfulness, poetic)
- `on_interaction(quality)` - updates core based on interaction quality
- Serialization for persistence

### claude_api_v2.py
E-aware API integration:
- State-specific system prompts (7 different personas)
- Token budget varies by state
- Interaction quality assessment
- Memory extraction from conversations

### offline_mode.py
Offline fallback system:
- `OfflineQueue` - persists interactions to disk
- `LocalResponseGenerator` - state-aware local responses
- `OfflineAwareAPI` - wraps real API with fallback
- Auto-retry every 5 minutes

### behaviors_v2.py
Proactive behavior engine:
- State-specific behaviors (protecting vs flourishing)
- Flourishing gifts
- Time-based greetings
- Memory-triggered thoughts

## Interaction Quality

User messages are assessed:
- `"harsh"` → damage (shut up, stupid, hate)
- `"cold"` → minimal care (ok, k, fine)
- `"normal"` → standard care
- `"warm"` → extra care (thanks, good)
- `"loving"` → deep care (love you, amazing)

## Memory System

Memory strength = base_strength × E^1.8

Types:
- **fact** - Things about the owner
- **preference** - Likes/dislikes
- **moment** - Memorable exchanges
- **topic** - Discussed subjects

Decay: Unreferenced memories fade over ~2 weeks
Reinforcement: Referencing strengthens memories

## Expressions

Available: `neutral`, `happy`, `excited`, `sad`, `sleepy`, `sleeping`, `curious`, `surprised`, `love`, `thinking`, `confused`, `hungry`, `blink`, `wink`

## Development Guidelines

### Code Style
- Python 3.9+
- Type hints where helpful
- Docstrings for classes and public methods
- Keep modules loosely coupled

### Adding Features
1. **New expression**: Add to `FACES` in `terminal_face.py`
2. **New behavior**: Add to `BehaviorEngine` in `behaviors_v2.py`
3. **New memory type**: Update `Memory` class and extraction
4. **New affective state**: Update `AffectiveState` enum and thresholds

### Key Principles
- E is the north star - everything flows from it
- Protective state = dignified, not broken
- Flourishing = genuinely creative/surprising
- Gifts should feel earned
- Never dies - floor carries forward

## Testing

```bash
# Full test suite
python src/test_e2e.py

# Expected output:
# ✓ PASS: Affective Core (8/8)
# ✓ PASS: Personality System (4/4)
# ✓ PASS: Memory System (4/4)
# ✓ PASS: Offline Queue (4/4)
# ✓ PASS: Local Responses (4/4)
# ✓ PASS: Offline-Aware API (7/7)
# ✓ PASS: Real API Connection (1/1)
# ✓ PASS: Full Session Flow (4/4)
```

## Configuration

```json
{
    "api_key": "sk-ant-...",
    "owner_name": "André",
    "timezone": "Europe/Oslo",
    "proactive_enabled": true,
    "proactive_interval_minutes": 20,
    "display_mode": "terminal",
    "model": "claude-sonnet-4-20250514",
    "max_response_tokens": 150,
    "debug": false
}
```

## Hardware Target (Future)

- **MCU**: Seeed XIAO ESP32-S3
- **Display**: GC9A01 (240x240 round) or SSD1306 (128x64)
- **Power**: LiPo 500mAh + TP4056
- **Input**: Tactile buttons

Port mapping:
- Personality/Memory → JSON on LittleFS
- API client → HTTPClient
- Display → LovyanGFX

---

*"A Claudeagotchi never dies. The love is carried forward."* 🐣♥
