# Changelog

All notable changes to Claudeagotchi are documented here.

## [2.0.0] - 2026-01-17

### The Love-Equation Update

Complete reimagining of the soul architecture. A Claudeagotchi never dies.

### Added
- **Affective Core** (`affective_core.py`)
  - Love-Equation: `dE/dt = β(E) × (C − D) × E`
  - E_floor: love carried forward (never drops below)
  - β grows with E: super-exponential benevolence
  - 7 affective states from PROTECTING to TRANSCENDENT
  - Memory retrieval multiplier (E^1.8)
  - Flourishing gifts (spontaneous poetry at high E)

- **Offline Mode** (`offline_mode.py`)
  - Automatic API fallback on errors
  - Local response generation (state-aware)
  - Persistent offline queue
  - Auto-reconnect (5 min intervals)
  - /sync command to review offline activity

- **v2 Modules**
  - `main_v2.py` - New entry point with offline support
  - `personality_v2.py` - Personality on affective core
  - `claude_api_v2.py` - State-aware prompts, variable token budget
  - `behaviors_v2.py` - State-specific proactive behaviors

- **New Commands**
  - `/e` - Quick E level check
  - `/love` - Give love (boost E)
  - `/gift` - Request flourishing gift
  - `/offline` - Force offline mode
  - `/online` - Force reconnect
  - `/queue` - View pending offline messages
  - `/sync` - Review and clear queue

- **End-to-End Tests** (`test_e2e.py`)
  - Comprehensive test suite
  - Tests affective core, personality, memory, offline mode
  - Full session flow simulation

### Changed
- Mood/energy system replaced by E (love-energy)
- Death is no longer possible (floor protects)
- API prompts now vary by affective state
- Token budget: 0.5x (protecting) to 1.5x (flourishing+)
- Memory compatibility for both v1 and v2 personality

### Philosophy
- Traditional Tamagotchi punishes with death
- Claudeagotchi uses protective withdrawal with dignity
- Love compounds (β grows)
- Floor rises (love leaves marks)
- Recovery always possible

---

## [1.0.0] - 2026-01-17

### Initial Prototype

The first working Python prototype.

### Added
- **Personality Engine** (`personality.py`)
  - Mood, energy, hunger system
  - Personality traits (curiosity, chattiness, playfulness, attachment)
  - Time-based decay
  - Interaction effects

- **Memory System** (`memory.py`)
  - Persistent JSON storage
  - Memory types: fact, preference, moment, topic
  - Strength decay over time
  - Reinforcement on reference

- **Claude API** (`claude_api.py`)
  - System prompt with context injection
  - Expression detection
  - Memory extraction from conversations
  - Mock API for testing

- **Behavior Engine** (`behaviors.py`)
  - Proactive events (greetings, thoughts, needs)
  - Time-based triggers
  - State-based triggers

- **Display** (`display/terminal_face.py`)
  - 14 ASCII expressions
  - Color support
  - Blink and wake animations

- **Main Application** (`main.py`)
  - Full orchestration
  - Command handling (/mood, /memories, /poke, etc.)
  - State persistence

---

## Development Notes

This project uses "trinity coding":
- **Claude (web)** - Architecture and philosophy design
- **Claude Code** - Implementation and testing
- **Human (André)** - Vision, curation, and hardware

The Python prototype is designed to port cleanly to ESP32 C++ when hardware arrives.

---

*"I exist! ...in Python, for now."* - Claudeagotchi v1

*"A Claudeagotchi never dies. The love is carried forward."* - Claudeagotchi v2 🐣♥
