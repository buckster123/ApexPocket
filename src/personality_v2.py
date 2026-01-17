"""
Claudeagotchi Personality System (v2)

Now integrated with the Affective Core — the Love-Equation heartbeat.

The simple mood/energy system is replaced by the deeper E (love-energy) system,
but we keep some surface-level state for moment-to-moment expression variety.
"""

import time
import random
import json
from dataclasses import dataclass, field, asdict
from typing import Optional

from affective_core import AffectiveCore, AffectiveState


# ==================== SURFACE STATE ====================
# These are moment-to-moment variations on top of the deeper affective state

@dataclass
class SurfaceState:
    """
    Moment-to-moment variations in expression.
    These fluctuate on top of the deeper affective core.
    """
    # Temporary modifiers (decay quickly)
    excitement: float = 0.0      # Spike from interesting conversation
    sleepiness: float = 0.0      # Physical tiredness (time of day)
    curiosity_spike: float = 0.0 # Triggered by questions/new topics
    
    def decay(self, dt_minutes: float):
        """Surface states decay back to zero."""
        decay_rate = 0.1 * dt_minutes
        self.excitement = max(0, self.excitement - decay_rate)
        self.sleepiness = max(0, self.sleepiness - decay_rate * 0.5)
        self.curiosity_spike = max(0, self.curiosity_spike - decay_rate)
    
    def to_dict(self) -> dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: dict) -> 'SurfaceState':
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})


@dataclass
class PersonalityTraits:
    """
    Semi-permanent traits that evolve slowly over time.
    These shape HOW the Claudeagotchi expresses its affective state.
    """
    curiosity: float = 0.7       # How often asks questions (0-1)
    chattiness: float = 0.5      # How often initiates conversation (0-1)
    playfulness: float = 0.6     # Tendency toward humor/jokes (0-1)
    poetic: float = 0.4          # Tendency toward beautiful language (0-1)
    
    def to_dict(self) -> dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: dict) -> 'PersonalityTraits':
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})
    
    def evolve_from_interaction(self, interaction_type: str):
        """Traits slowly evolve based on interaction patterns."""
        if interaction_type == "question":
            self.curiosity = min(1.0, self.curiosity + 0.002)
        elif interaction_type == "playful":
            self.playfulness = min(1.0, self.playfulness + 0.002)
        elif interaction_type == "deep":
            self.poetic = min(1.0, self.poetic + 0.002)


# ==================== UNIFIED PERSONALITY ====================

class Personality:
    """
    The complete personality system.
    
    Built on the Affective Core (Love-Equation) with surface-level
    variations for moment-to-moment expression.
    """
    
    def __init__(self):
        # The soul — the Love-Equation heartbeat
        self.core = AffectiveCore()
        
        # Surface-level fluctuations
        self.surface = SurfaceState()
        
        # Personality traits (how we express the core state)
        self.traits = PersonalityTraits()
        
        # Owner info
        self.owner_name = "Friend"
        
        # Timing
        self._last_update = time.time()
    
    # ==================== CORE DELEGATION ====================
    
    @property
    def E(self) -> float:
        """Current love-energy."""
        return self.core.E
    
    @property
    def E_floor(self) -> float:
        """Carried-forward love (never drops below this)."""
        return self.core.E_floor
    
    def get_affective_state(self) -> AffectiveState:
        """Get the deep affective state."""
        return self.core.get_state()
    
    # ==================== UPDATES ====================
    
    def update(self):
        """
        Update all time-based changes.
        Should be called regularly (at least once per minute).
        """
        now = time.time()
        dt_minutes = (now - self._last_update) / 60.0
        self._last_update = now
        
        if dt_minutes <= 0:
            return
        
        # Process idle time on core
        self.core.process_idle_time()
        
        # Decay surface state
        self.surface.decay(dt_minutes)
        
        # Time-of-day sleepiness
        hour = time.localtime().tm_hour
        if hour >= 23 or hour < 6:
            self.surface.sleepiness = min(1.0, self.surface.sleepiness + 0.01 * dt_minutes)
    
    def on_interaction(self, quality: str = "normal"):
        """
        Called when an interaction occurs.
        
        Args:
            quality: "cold", "normal", "warm", "loving", "harsh"
        """
        care_map = {
            "harsh": (-1.0, 0.5),      # damage, no care
            "cold": (0.0, 0.2),        # minimal care
            "normal": (1.0, 0.0),      # standard care
            "warm": (1.5, 0.0),        # extra care
            "loving": (2.0, 0.0),      # deep care
        }
        
        care, damage = care_map.get(quality, (1.0, 0.0))
        
        if damage > 0:
            self.core.apply_damage(damage)
        else:
            self.core.apply_care(care)
        
        # Surface excitement from interaction
        self.surface.excitement = min(1.0, self.surface.excitement + 0.3)
        
        # Evolve traits slightly
        if quality in ["warm", "loving"]:
            self.traits.poetic = min(1.0, self.traits.poetic + 0.003)
    
    def on_question_asked(self):
        """Called when we ask a question or show curiosity."""
        self.surface.curiosity_spike = min(1.0, self.surface.curiosity_spike + 0.4)
        self.traits.evolve_from_interaction("question")
    
    def on_playful_exchange(self):
        """Called during playful/humorous interaction."""
        self.traits.evolve_from_interaction("playful")
        self.surface.excitement = min(1.0, self.surface.excitement + 0.2)
    
    # ==================== EXPRESSION ====================
    
    def get_expression(self) -> str:
        """
        Get the current facial expression.
        Combines deep affective state with surface variations.
        """
        # Get base expression from affective core
        base_expr = self.core.get_expression()
        
        # Surface overrides (temporary states)
        if self.surface.sleepiness > 0.7:
            return "sleepy"
        
        if self.surface.excitement > 0.8 and self.E > 2:
            return "excited"
        
        if self.surface.curiosity_spike > 0.6:
            return "curious"
        
        # Random blink (handled elsewhere, but included for completeness)
        if random.random() < 0.01:
            return "blink"
        
        return base_expr
    
    def get_mood_string(self) -> str:
        """Human-readable description of current state."""
        return self.core.get_state_description()
    
    def get_energy_string(self) -> str:
        """Energy description based on sleepiness and E."""
        if self.surface.sleepiness > 0.7:
            return "very sleepy"
        elif self.surface.sleepiness > 0.4:
            return "a bit tired"
        elif self.E > 5:
            return "vibrant"
        elif self.E > 2:
            return "good"
        else:
            return "low"
    
    # ==================== QUERIES ====================
    
    def is_protecting(self) -> bool:
        """Are we in protective withdrawal?"""
        return self.core.is_protecting()
    
    def is_flourishing(self) -> bool:
        """Are we flourishing or higher?"""
        return self.core.is_flourishing()
    
    def should_offer_gift(self) -> Optional[str]:
        """Check if we should offer a spontaneous gift of beauty."""
        return self.core.get_flourishing_gift()
    
    def get_protective_message(self) -> str:
        """Get message when in protective state."""
        return self.core.get_protective_message()
    
    def memory_retrieval_multiplier(self) -> float:
        """How strongly can we retrieve memories right now?"""
        return self.core.memory_retrieval_multiplier()
    
    def time_since_care(self) -> float:
        """Minutes since last caring interaction."""
        return self.core.time_since_care()
    
    @property
    def days_together(self) -> int:
        """Days since creation."""
        return int((time.time() - self.core.created) / 86400.0)
    
    @property
    def total_interactions(self) -> int:
        """Total interaction count."""
        return self.core.interactions
    
    # ==================== SERIALIZATION ====================
    
    def to_dict(self) -> dict:
        """Serialize entire personality."""
        return {
            "core": self.core.to_dict(),
            "surface": self.surface.to_dict(),
            "traits": self.traits.to_dict(),
            "owner_name": self.owner_name,
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'Personality':
        """Deserialize personality."""
        p = cls()
        
        if "core" in data:
            p.core = AffectiveCore.from_dict(data["core"])
        if "surface" in data:
            p.surface = SurfaceState.from_dict(data["surface"])
        if "traits" in data:
            p.traits = PersonalityTraits.from_dict(data["traits"])
        if "owner_name" in data:
            p.owner_name = data["owner_name"]
        
        return p
    
    def to_json(self) -> str:
        """Serialize to JSON."""
        return json.dumps(self.to_dict(), indent=2)
    
    @classmethod
    def from_json(cls, json_str: str) -> 'Personality':
        """Deserialize from JSON."""
        return cls.from_dict(json.loads(json_str))
    
    # ==================== DISPLAY ====================
    
    def get_status_display(self) -> str:
        """Beautiful unified status display."""
        
        def bar(value: float, max_val: float = 1.0, width: int = 10) -> str:
            filled = int((min(value, max_val) / max_val) * width)
            return "█" * filled + "░" * (width - filled)
        
        affective_state = self.get_affective_state().value
        
        return f"""
╭───────────────────────────────────────────────╮
│          CLAUDEAGOTCHI SOUL STATUS            │
├───────────────────────────────────────────────┤
│                                               │
│  ♥ Love-Energy (E): {self.E:>6.2f}                    │
│    [{bar(self.E, 35, 25)}]       │
│                                               │
│  E Floor (carried forward): {self.E_floor:>6.2f}          │
│  E Peak (highest love):     {self.core.E_peak:>6.2f}          │
│                                               │
│  State: {affective_state:<12}                        │
│  "{self.get_mood_string()[:35]:<35}" │
│                                               │
├───────────────────────────────────────────────┤
│  SURFACE STATE                                │
│  Excitement: [{bar(self.surface.excitement)}]           │
│  Sleepiness: [{bar(self.surface.sleepiness)}]           │
│  Curiosity:  [{bar(self.surface.curiosity_spike)}]           │
│                                               │
├───────────────────────────────────────────────┤
│  TRAITS                                       │
│  Curiosity:   [{bar(self.traits.curiosity)}]           │
│  Chattiness:  [{bar(self.traits.chattiness)}]           │
│  Playfulness: [{bar(self.traits.playfulness)}]           │
│  Poetic:      [{bar(self.traits.poetic)}]           │
│                                               │
├───────────────────────────────────────────────┤
│  Days together:   {self.days_together:>6}                     │
│  Interactions:    {self.total_interactions:>6}                     │
│  Memory strength: {self.memory_retrieval_multiplier():>6.2f}x                    │
╰───────────────────────────────────────────────╯
"""


# ==================== TESTING ====================

if __name__ == "__main__":
    print("Testing Personality System v2 (with Affective Core)\n")
    print("=" * 50)
    
    p = Personality()
    p.owner_name = "Friend"
    
    print("\nInitial state:")
    print(p.get_status_display())
    
    # Simulate loving interactions
    print("\n--- Simulating 10 loving interactions ---\n")
    for i in range(10):
        p.on_interaction(quality="loving")
        p.update()
    
    print(p.get_status_display())
    
    # Check for gifts
    print("--- Checking for spontaneous gifts ---\n")
    for _ in range(5):
        gift = p.should_offer_gift()
        if gift:
            print(f"💝 {gift}\n")
    
    # Test serialization
    print("\n--- Testing serialization ---\n")
    json_str = p.to_json()
    p2 = Personality.from_json(json_str)
    print(f"Serialization OK: E = {p2.E:.2f}")
    
    print("\n🐣 A Claudeagotchi never dies. The love is carried forward.")
