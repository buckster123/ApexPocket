"""
Claudeagotchi Personality Engine

Manages mood, energy, and personality traits.
This is the emotional core of the Claudeagotchi.
"""

import time
import random
from dataclasses import dataclass, field, asdict
from typing import Optional
import json


# ==================== TUNING CONSTANTS ====================

# Decay rates (per minute)
MOOD_DECAY_RATE = 0.001          # Slow drift toward baseline
ENERGY_DECAY_RATE = 0.002        # Gradual energy loss
HUNGER_INCREASE_RATE = 0.003     # Builds up over time

# Interaction effects
INTERACTION_MOOD_BOOST = 0.05    # Mood boost from chatting
INTERACTION_ENERGY_COST = 0.02   # Energy cost of conversation
INTERACTION_HUNGER_RELIEF = 0.1  # Satisfies need for interaction

# Baselines
MOOD_BASELINE = 0.6              # Neutral-positive default mood
ENERGY_BASELINE = 0.7            # Well-rested default

# Thresholds
LOW_ENERGY_THRESHOLD = 0.3
LOW_MOOD_THRESHOLD = 0.3
HIGH_HUNGER_THRESHOLD = 0.7


@dataclass
class PersonalityTraits:
    """
    Semi-permanent traits that evolve slowly over time.
    These shape HOW the Claudeagotchi behaves.
    """
    curiosity: float = 0.7       # How often asks questions (0-1)
    chattiness: float = 0.5      # How often initiates conversation (0-1)
    playfulness: float = 0.6     # Tendency toward humor/jokes (0-1)
    attachment: float = 0.3      # Bond strength with owner (0-1, grows over time)
    
    def to_dict(self) -> dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: dict) -> 'PersonalityTraits':
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})


@dataclass
class PersonalityState:
    """
    Dynamic state that changes frequently.
    This is HOW the Claudeagotchi feels right now.
    """
    mood: float = 0.6            # 0=sad, 1=happy
    energy: float = 0.8          # 0=exhausted, 1=energetic  
    hunger: float = 0.2          # 0=satisfied, 1=needs interaction
    
    def to_dict(self) -> dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: dict) -> 'PersonalityState':
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})


@dataclass 
class PersonalityStats:
    """
    Cumulative statistics about the relationship.
    """
    total_interactions: int = 0
    days_together: int = 0
    created_timestamp: float = field(default_factory=time.time)
    last_interaction_timestamp: float = field(default_factory=time.time)
    
    def to_dict(self) -> dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: dict) -> 'PersonalityStats':
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})


class Personality:
    """
    The complete personality system.
    Combines traits, state, and stats into a living personality.
    """
    
    def __init__(self):
        self.traits = PersonalityTraits()
        self.state = PersonalityState()
        self.stats = PersonalityStats()
        self._last_update = time.time()
    
    # ==================== STATE UPDATES ====================
    
    def update(self):
        """
        Called regularly to update time-based changes.
        Should be called at least once per minute.
        """
        now = time.time()
        elapsed_minutes = (now - self._last_update) / 60.0
        self._last_update = now
        
        if elapsed_minutes <= 0:
            return
        
        # Mood drifts toward baseline
        mood_diff = MOOD_BASELINE - self.state.mood
        self.state.mood += mood_diff * MOOD_DECAY_RATE * elapsed_minutes
        
        # Energy slowly depletes
        self.state.energy -= ENERGY_DECAY_RATE * elapsed_minutes
        self.state.energy = max(0.0, self.state.energy)
        
        # Hunger for interaction builds up
        self.state.hunger += HUNGER_INCREASE_RATE * elapsed_minutes
        self.state.hunger = min(1.0, self.state.hunger)
        
        # Update days together
        days = (now - self.stats.created_timestamp) / 86400.0
        self.stats.days_together = int(days)
    
    def on_interaction(self, positive: bool = True):
        """
        Called when an interaction (conversation) occurs.
        """
        # Update stats
        self.stats.total_interactions += 1
        self.stats.last_interaction_timestamp = time.time()
        
        # Mood boost (or decrease if negative)
        if positive:
            self.state.mood = min(1.0, self.state.mood + INTERACTION_MOOD_BOOST)
            # Attachment grows slowly with positive interactions
            self.traits.attachment = min(1.0, self.traits.attachment + 0.005)
        else:
            self.state.mood = max(0.0, self.state.mood - INTERACTION_MOOD_BOOST)
        
        # Energy cost
        self.state.energy = max(0.0, self.state.energy - INTERACTION_ENERGY_COST)
        
        # Satisfies hunger for interaction
        self.state.hunger = max(0.0, self.state.hunger - INTERACTION_HUNGER_RELIEF)
    
    def rest(self, duration_minutes: float = 30):
        """
        Simulate resting/sleeping to restore energy.
        """
        recovery = duration_minutes * 0.01  # 1% per minute
        self.state.energy = min(1.0, self.state.energy + recovery)
    
    # ==================== STATE QUERIES ====================
    
    def get_expression(self) -> str:
        """
        Determine the current expression based on state.
        Returns an expression name for the display system.
        """
        # Priority-based expression selection
        
        if self.state.energy < 0.2:
            return "sleeping"
        
        if self.state.energy < LOW_ENERGY_THRESHOLD:
            return "sleepy"
        
        if self.state.mood > 0.8:
            if random.random() < 0.3:
                return "excited"
            return "happy"
        
        if self.state.mood < LOW_MOOD_THRESHOLD:
            return "sad"
        
        if self.state.hunger > HIGH_HUNGER_THRESHOLD:
            return "hungry"
        
        if random.random() < self.traits.curiosity * 0.1:
            return "curious"
        
        return "neutral"
    
    def get_mood_string(self) -> str:
        """Human-readable mood description."""
        if self.state.mood > 0.8:
            return "very happy"
        elif self.state.mood > 0.6:
            return "happy"
        elif self.state.mood > 0.4:
            return "okay"
        elif self.state.mood > 0.2:
            return "a bit down"
        else:
            return "sad"
    
    def get_energy_string(self) -> str:
        """Human-readable energy description."""
        if self.state.energy > 0.8:
            return "energetic"
        elif self.state.energy > 0.6:
            return "good"
        elif self.state.energy > 0.4:
            return "a bit tired"
        elif self.state.energy > 0.2:
            return "tired"
        else:
            return "exhausted"
    
    def is_sleepy(self) -> bool:
        return self.state.energy < LOW_ENERGY_THRESHOLD
    
    def is_sad(self) -> bool:
        return self.state.mood < LOW_MOOD_THRESHOLD
    
    def is_hungry_for_interaction(self) -> bool:
        return self.state.hunger > HIGH_HUNGER_THRESHOLD
    
    def time_since_last_interaction(self) -> float:
        """Returns seconds since last interaction."""
        return time.time() - self.stats.last_interaction_timestamp
    
    # ==================== SERIALIZATION ====================
    
    def to_dict(self) -> dict:
        """Serialize entire personality to dictionary."""
        return {
            "traits": self.traits.to_dict(),
            "state": self.state.to_dict(),
            "stats": self.stats.to_dict()
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'Personality':
        """Deserialize personality from dictionary."""
        p = cls()
        if "traits" in data:
            p.traits = PersonalityTraits.from_dict(data["traits"])
        if "state" in data:
            p.state = PersonalityState.from_dict(data["state"])
        if "stats" in data:
            p.stats = PersonalityStats.from_dict(data["stats"])
        return p
    
    def to_json(self) -> str:
        """Serialize to JSON string."""
        return json.dumps(self.to_dict(), indent=2)
    
    @classmethod
    def from_json(cls, json_str: str) -> 'Personality':
        """Deserialize from JSON string."""
        return cls.from_dict(json.loads(json_str))
    
    # ==================== DISPLAY ====================
    
    def get_status_display(self) -> str:
        """Get a formatted status string for display."""
        bars = lambda v: "█" * int(v * 10) + "░" * (10 - int(v * 10))
        
        return f"""
╭─────────────────────────────────────╮
│       CLAUDEAGOTCHI STATUS          │
├─────────────────────────────────────┤
│  Mood:   [{bars(self.state.mood)}] {self.get_mood_string():>12} │
│  Energy: [{bars(self.state.energy)}] {self.get_energy_string():>12} │
│  Hunger: [{bars(self.state.hunger)}] {"needs chat" if self.is_hungry_for_interaction() else "satisfied":>12} │
├─────────────────────────────────────┤
│  Days together: {self.stats.days_together:>4}               │
│  Total chats:   {self.stats.total_interactions:>4}               │
│  Attachment:    {self.traits.attachment*100:>5.1f}%             │
╰─────────────────────────────────────╯
"""


# ==================== TESTING ====================

if __name__ == "__main__":
    # Quick test
    p = Personality()
    print("Initial state:")
    print(p.get_status_display())
    
    # Simulate some interactions
    for i in range(5):
        p.on_interaction(positive=True)
    
    print("\nAfter 5 positive interactions:")
    print(p.get_status_display())
    
    # Simulate time passing
    p._last_update -= 60 * 60  # 1 hour ago
    p.update()
    
    print("\nAfter 1 hour of no interaction:")
    print(p.get_status_display())
    
    # Test serialization
    json_str = p.to_json()
    p2 = Personality.from_json(json_str)
    print("\nSerialization test passed:", p.state.mood == p2.state.mood)
