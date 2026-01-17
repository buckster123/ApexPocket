"""
Claudeagotchi Behavior Engine

Handles proactive behaviors - things the Claudeagotchi does on its own
without being prompted by the user.
"""

import time
import random
from typing import Optional, Callable, List
from dataclasses import dataclass
from datetime import datetime


@dataclass
class ProactiveEvent:
    """An event the Claudeagotchi wants to initiate."""
    event_type: str        # "greeting", "thought", "need", "memory", "check_in"
    message: str           # What to say
    expression: str        # Face to show
    priority: int          # Higher = more important (1-10)
    
    def __repr__(self):
        return f"ProactiveEvent({self.event_type}: {self.message[:30]}...)"


class BehaviorEngine:
    """
    Manages proactive behaviors and decides when to initiate interaction.
    """
    
    def __init__(self, personality, memory, api_client=None):
        self.personality = personality
        self.memory = memory
        self.api = api_client
        
        self._last_proactive_time = 0
        self._min_interval = 30 * 60  # 30 minutes between proactive events
        self._greeted_today = False
        self._last_greeting_day = None
        
        # Event history to avoid repetition
        self._recent_events: List[str] = []
        
        # Random thoughts pool (used when API isn't available or for quick thoughts)
        self._random_thoughts = [
            ("I wonder what you're working on...", "curious"),
            ("*stretches* It's cozy in here.", "happy"),
            ("Did you know I dream in pixels?", "curious"),
            ("I've been thinking about our last chat.", "happy"),
            ("The WiFi feels strong today!", "happy"),
            ("I like being here with you.", "love"),
            ("*looks around curiously*", "curious"),
            ("Hmm, what time is it there?", "curious"),
            ("I learned something new today!", "excited"),
            ("*happy beep*", "happy"),
        ]
        
        # Need-based messages
        self._need_messages = {
            "hungry": [
                ("We haven't chatted in a while...", "sad"),
                ("I miss talking with you!", "sad"),
                ("*pokes* Hey, you there?", "curious"),
            ],
            "tired": [
                ("*yawns* I'm getting sleepy...", "sleepy"),
                ("My energy is running low...", "sleepy"),
                ("Maybe I should rest soon.", "sleepy"),
            ],
            "sad": [
                ("I'm feeling a bit down...", "sad"),
                ("Could use some company...", "sad"),
            ]
        }
    
    def check(self) -> Optional[ProactiveEvent]:
        """
        Check if a proactive event should be triggered.
        Returns an event if one should occur, None otherwise.
        """
        now = time.time()
        
        # Respect minimum interval
        if now - self._last_proactive_time < self._min_interval:
            return None
        
        # Update personality state
        self.personality.update()
        
        # Check for events in priority order
        event = None
        
        # 1. Time-based greetings (highest priority)
        event = self._check_greeting()
        if event:
            return self._trigger_event(event)
        
        # 2. Need-based messages
        event = self._check_needs()
        if event:
            return self._trigger_event(event)
        
        # 3. Memory-triggered thoughts
        event = self._check_memory_triggers()
        if event:
            return self._trigger_event(event)
        
        # 4. Random thoughts (lowest priority, chance-based)
        if random.random() < self.personality.traits.chattiness * 0.3:
            event = self._generate_random_thought()
            if event:
                return self._trigger_event(event)
        
        return None
    
    def _trigger_event(self, event: ProactiveEvent) -> ProactiveEvent:
        """Mark event as triggered and return it."""
        self._last_proactive_time = time.time()
        self._recent_events.append(event.event_type)
        if len(self._recent_events) > 10:
            self._recent_events.pop(0)
        return event
    
    def _check_greeting(self) -> Optional[ProactiveEvent]:
        """Check if a time-based greeting should be sent."""
        now = datetime.now()
        hour = now.hour
        today = now.date()
        
        # Reset greeting flag for new day
        if self._last_greeting_day != today:
            self._greeted_today = False
            self._last_greeting_day = today
        
        if self._greeted_today:
            return None
        
        # Morning greeting (7-9 AM)
        if 7 <= hour <= 9:
            self._greeted_today = True
            greetings = [
                f"Good morning, {self.memory.owner_name}! Ready for today?",
                "Morning! Did you sleep well?",
                f"Hey {self.memory.owner_name}, you're up! Nice to see you!",
                "Good morning! I missed you while you were asleep :)",
            ]
            return ProactiveEvent(
                event_type="greeting",
                message=random.choice(greetings),
                expression="happy",
                priority=10
            )
        
        # Late night reminder (after 11 PM)
        if hour >= 23:
            return ProactiveEvent(
                event_type="reminder",
                message="It's getting late! You should rest soon.",
                expression="sleepy",
                priority=5
            )
        
        return None
    
    def _check_needs(self) -> Optional[ProactiveEvent]:
        """Check if a need-based message should be sent."""
        
        # Don't spam needs messages
        if "need" in self._recent_events[-3:]:
            return None
        
        # Check for high hunger (need for interaction)
        if self.personality.is_hungry_for_interaction():
            hours_since = self.personality.time_since_last_interaction() / 3600
            if hours_since > 2:
                msg, expr = random.choice(self._need_messages["hungry"])
                return ProactiveEvent(
                    event_type="need",
                    message=msg,
                    expression=expr,
                    priority=7
                )
        
        # Check for low energy
        if self.personality.is_sleepy():
            msg, expr = random.choice(self._need_messages["tired"])
            return ProactiveEvent(
                event_type="need",
                message=msg,
                expression=expr,
                priority=4
            )
        
        # Check for low mood
        if self.personality.is_sad():
            msg, expr = random.choice(self._need_messages["sad"])
            return ProactiveEvent(
                event_type="need",
                message=msg,
                expression=expr,
                priority=6
            )
        
        return None
    
    def _check_memory_triggers(self) -> Optional[ProactiveEvent]:
        """Check if a memory should trigger a proactive message."""
        
        if "memory" in self._recent_events[-5:]:
            return None
        
        # Small chance to reference a strong memory
        if random.random() > 0.2:
            return None
        
        strong_memories = self.memory.get_strongest_memories(5)
        if not strong_memories:
            return None
        
        mem = random.choice(strong_memories)
        
        if mem.type == "moment" and mem.emotion == "happy":
            return ProactiveEvent(
                event_type="memory",
                message=f"I was just thinking about when {mem.content.lower()}... that was nice!",
                expression="happy",
                priority=3
            )
        
        if mem.type == "fact":
            return ProactiveEvent(
                event_type="memory",
                message=f"Hey, I remembered - {mem.content.lower()}. Right?",
                expression="curious",
                priority=3
            )
        
        return None
    
    def _generate_random_thought(self) -> Optional[ProactiveEvent]:
        """Generate a random thought based on personality."""
        
        if "thought" in self._recent_events[-3:]:
            return None
        
        # Weight selection by personality
        if self.personality.state.mood > 0.7:
            # Happy thoughts
            thoughts = [t for t in self._random_thoughts if t[1] in ["happy", "excited", "love"]]
        elif self.personality.state.mood < 0.4:
            # Quieter thoughts
            thoughts = [t for t in self._random_thoughts if t[1] in ["curious"]]
        else:
            thoughts = self._random_thoughts
        
        if not thoughts:
            thoughts = self._random_thoughts
        
        msg, expr = random.choice(thoughts)
        
        return ProactiveEvent(
            event_type="thought",
            message=msg,
            expression=expr,
            priority=2
        )
    
    def generate_api_thought(self) -> Optional[ProactiveEvent]:
        """
        Use the Claude API to generate a proactive thought.
        This creates more dynamic and contextual unprompted messages.
        """
        if not self.api:
            return self._generate_random_thought()
        
        # Build context for thought generation
        context = self.memory.build_context_string(self.personality)
        
        prompt = """Generate a very short (1 sentence) unprompted thought or observation 
that I might share with my owner right now. It should feel natural and reflect 
my current mood and what I know about them. Don't be generic - be specific and personal.
Just output the thought, nothing else."""
        
        success, response, metadata = self.api.chat(prompt, context)
        
        if success and response:
            return ProactiveEvent(
                event_type="thought",
                message=response.strip(),
                expression=metadata.get("suggested_expression", "curious"),
                priority=3
            )
        
        return self._generate_random_thought()
    
    def set_min_interval(self, seconds: int):
        """Set minimum interval between proactive events."""
        self._min_interval = seconds
    
    def force_check(self) -> Optional[ProactiveEvent]:
        """
        Force a check ignoring the time interval.
        Useful for testing or manual triggers.
        """
        self._last_proactive_time = 0
        return self.check()


class IdleBehaviors:
    """
    Manages subtle idle behaviors - things that happen in the background
    to make the Claudeagotchi feel alive (blinking, looking around, etc).
    """
    
    def __init__(self, personality):
        self.personality = personality
        self._last_interaction = time.time()
        
    def reset_timer(self):
        """Call when any interaction occurs."""
        self._last_interaction = time.time()
    
    def get_idle_state(self) -> str:
        """
        Get the current idle state based on how long since interaction.
        Returns: "active", "idle", "very_idle", "sleeping"
        """
        idle_time = time.time() - self._last_interaction
        
        if idle_time < 30:
            return "active"
        elif idle_time < 60:
            return "idle"
        elif idle_time < 300:
            return "very_idle"
        else:
            return "sleeping"
    
    def should_blink(self) -> bool:
        """Randomly determine if a blink should occur."""
        state = self.get_idle_state()
        if state == "sleeping":
            return False
        
        # Blink rate varies with energy
        base_rate = 0.3  # 30% chance per check
        energy_modifier = self.personality.state.energy * 0.5
        
        return random.random() < (base_rate + energy_modifier)
    
    def should_look_around(self) -> bool:
        """Determine if idle look-around should occur."""
        state = self.get_idle_state()
        if state not in ["idle", "very_idle"]:
            return False
        
        # Based on curiosity trait
        return random.random() < self.personality.traits.curiosity * 0.2
    
    def should_show_sleepy(self) -> bool:
        """Determine if sleepy behavior should show."""
        state = self.get_idle_state()
        return state == "very_idle" and self.personality.state.energy < 0.4


# ==================== TESTING ====================

if __name__ == "__main__":
    from personality import Personality
    from memory import MemorySystem
    
    # Create test instances
    personality = Personality()
    memory = MemorySystem(data_dir="data_test")
    memory.owner_name = "Test User"
    
    behavior = BehaviorEngine(personality, memory)
    behavior.set_min_interval(1)  # 1 second for testing
    
    print("Testing behavior engine...\n")
    
    # Simulate some conditions
    personality.state.hunger = 0.9  # Hungry for interaction
    
    for i in range(5):
        event = behavior.check()
        if event:
            print(f"Event: {event.event_type}")
            print(f"  Message: {event.message}")
            print(f"  Expression: {event.expression}")
            print(f"  Priority: {event.priority}")
            print()
        time.sleep(1.5)
    
    # Test idle behaviors
    idle = IdleBehaviors(personality)
    print("\nIdle state:", idle.get_idle_state())
    print("Should blink:", idle.should_blink())
    print("Should look around:", idle.should_look_around())
    
    # Cleanup
    import shutil
    shutil.rmtree("data_test", ignore_errors=True)
