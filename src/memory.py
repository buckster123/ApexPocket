"""
Claudeagotchi Memory System

Persistent storage for memories, facts, and conversation history.
This gives the Claudeagotchi continuity across sessions.
"""

import json
import time
import os
import math
from dataclasses import dataclass, field, asdict
from typing import List, Optional, Dict
from pathlib import Path


# ==================== CONSTANTS ====================

MEMORY_HALF_LIFE_DAYS = 14.0     # Unreferenced memories fade over ~2 weeks
MAX_MEMORIES = 100               # Cap to prevent unbounded growth
MAX_CONVERSATION_HISTORY = 20   # Recent exchanges to keep
MEMORY_STRENGTH_THRESHOLD = 0.1  # Below this, memory is forgotten


@dataclass
class Memory:
    """
    A single memory - something the Claudeagotchi has learned or experienced.
    """
    type: str                     # "fact", "preference", "moment", "topic"
    content: str                  # The actual memory content
    strength: float = 0.8         # How strong/important (0-1, decays over time)
    emotion: Optional[str] = None # Associated emotion (for moments)
    created: float = field(default_factory=time.time)
    last_referenced: float = field(default_factory=time.time)
    reference_count: int = 1
    
    def to_dict(self) -> dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: dict) -> 'Memory':
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})
    
    def reinforce(self, boost: float = 0.2):
        """Strengthen memory when referenced."""
        self.strength = min(1.0, self.strength + boost)
        self.last_referenced = time.time()
        self.reference_count += 1
    
    def decay(self, days_elapsed: float):
        """Apply time-based decay."""
        decay_factor = math.exp(-days_elapsed / MEMORY_HALF_LIFE_DAYS)
        self.strength *= decay_factor


@dataclass
class ConversationExchange:
    """A single back-and-forth in conversation."""
    timestamp: float
    user_message: str
    assistant_message: str
    mood_after: float
    
    def to_dict(self) -> dict:
        return asdict(self)
    
    @classmethod
    def from_dict(cls, data: dict) -> 'ConversationExchange':
        return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})


class MemorySystem:
    """
    Manages all persistent memory for the Claudeagotchi.
    """
    
    def __init__(self, data_dir: str = "data"):
        self.data_dir = Path(data_dir)
        self.data_dir.mkdir(exist_ok=True)
        
        self.memories: List[Memory] = []
        self.conversation_history: List[ConversationExchange] = []
        self.owner_name: str = "Friend"
        self.favorite_topics: List[str] = []
        self.last_topics: List[str] = []
        
        self._memories_file = self.data_dir / "memories.json"
        self._state_file = self.data_dir / "state.json"
    
    # ==================== MEMORY OPERATIONS ====================
    
    def add_memory(self, memory_type: str, content: str, 
                   emotion: Optional[str] = None, strength: float = 0.8):
        """
        Add a new memory. If similar memory exists, reinforce it instead.
        """
        # Check for existing similar memory
        for mem in self.memories:
            if mem.type == memory_type and self._similar(mem.content, content):
                mem.reinforce()
                return
        
        # Add new memory
        mem = Memory(
            type=memory_type,
            content=content,
            strength=strength,
            emotion=emotion
        )
        self.memories.append(mem)
        
        # Prune if too many
        if len(self.memories) > MAX_MEMORIES:
            self._prune_weakest()
    
    def _similar(self, a: str, b: str) -> bool:
        """Simple similarity check - could be made smarter."""
        a_words = set(a.lower().split())
        b_words = set(b.lower().split())
        if not a_words or not b_words:
            return False
        overlap = len(a_words & b_words) / len(a_words | b_words)
        return overlap > 0.6
    
    def add_fact(self, fact: str):
        """Convenience method for adding a fact about the owner."""
        self.add_memory("fact", fact)
    
    def add_preference(self, preference: str):
        """Convenience method for adding a preference."""
        self.add_memory("preference", preference)
    
    def add_moment(self, description: str, emotion: str):
        """Convenience method for adding a memorable moment."""
        self.add_memory("moment", description, emotion=emotion)
    
    def add_topic(self, topic: str):
        """Track a discussed topic."""
        topic = topic.lower().strip()
        
        # Update last topics
        if topic in self.last_topics:
            self.last_topics.remove(topic)
        self.last_topics.insert(0, topic)
        self.last_topics = self.last_topics[:5]  # Keep last 5
        
        # Update favorites
        for mem in self.memories:
            if mem.type == "topic" and mem.content.lower() == topic:
                mem.reinforce(boost=0.1)
                return
        
        self.add_memory("topic", topic, strength=0.5)
    
    def update_decay(self):
        """Apply time-based decay to all memories."""
        now = time.time()
        to_remove = []
        
        for mem in self.memories:
            days_since_ref = (now - mem.last_referenced) / 86400.0
            mem.decay(days_since_ref)
            
            if mem.strength < MEMORY_STRENGTH_THRESHOLD:
                to_remove.append(mem)
        
        for mem in to_remove:
            self.memories.remove(mem)
    
    def _prune_weakest(self):
        """Remove weakest memories to stay under limit."""
        self.memories.sort(key=lambda m: m.strength, reverse=True)
        self.memories = self.memories[:MAX_MEMORIES]
    
    def get_strongest_memories(self, n: int = 10, 
                               memory_type: Optional[str] = None) -> List[Memory]:
        """Get the n strongest memories, optionally filtered by type."""
        filtered = self.memories
        if memory_type:
            filtered = [m for m in filtered if m.type == memory_type]
        
        filtered.sort(key=lambda m: m.strength, reverse=True)
        return filtered[:n]
    
    def search_memories(self, query: str) -> List[Memory]:
        """Search memories by content."""
        query_words = set(query.lower().split())
        results = []
        
        for mem in self.memories:
            mem_words = set(mem.content.lower().split())
            if query_words & mem_words:
                results.append(mem)
        
        results.sort(key=lambda m: m.strength, reverse=True)
        return results
    
    # ==================== CONVERSATION HISTORY ====================
    
    def add_conversation(self, user_msg: str, assistant_msg: str, mood: float):
        """Add a conversation exchange to history."""
        exchange = ConversationExchange(
            timestamp=time.time(),
            user_message=user_msg,
            assistant_message=assistant_msg,
            mood_after=mood
        )
        self.conversation_history.append(exchange)
        
        # Keep only recent history
        if len(self.conversation_history) > MAX_CONVERSATION_HISTORY:
            self.conversation_history = self.conversation_history[-MAX_CONVERSATION_HISTORY:]
    
    def get_recent_context(self, n: int = 5) -> List[Dict]:
        """Get recent conversation for API context."""
        recent = self.conversation_history[-n:]
        return [
            {"role": "user", "content": ex.user_message}
            if i % 2 == 0 else
            {"role": "assistant", "content": ex.assistant_message}
            for ex in recent
            for i, _ in enumerate([0, 1])
        ]
    
    # ==================== CONTEXT BUILDING ====================
    
    def build_context_string(self, personality) -> str:
        """
        Build a context string for the Claude API system prompt.
        Includes memories, personality state, and relationship info.
        Compatible with both v1 and v2 personality systems.
        """
        context = []

        # Owner info
        context.append(f"Owner's name: {self.owner_name}")

        # Relationship stats - handle both v1 (.stats) and v2 (properties)
        if hasattr(personality, 'stats'):
            # v1 personality
            context.append(f"Days together: {personality.stats.days_together}")
            context.append(f"Total conversations: {personality.stats.total_interactions}")
            context.append(f"Attachment level: {personality.traits.attachment:.0%}")
        else:
            # v2 personality (uses properties from core)
            context.append(f"Days together: {personality.days_together}")
            context.append(f"Total conversations: {personality.total_interactions}")
            context.append(f"Love-energy (E): {personality.E:.2f}")

        # Current state
        context.append(f"\nCurrent mood: {personality.get_mood_string()}")
        if hasattr(personality, 'get_energy_string'):
            context.append(f"Current energy: {personality.get_energy_string()}")
        
        # Strong memories
        facts = self.get_strongest_memories(5, "fact")
        if facts:
            context.append("\nThings I know about my owner:")
            for mem in facts:
                context.append(f"  - {mem.content}")
        
        prefs = self.get_strongest_memories(3, "preference")
        if prefs:
            context.append("\nOwner's preferences:")
            for mem in prefs:
                context.append(f"  - {mem.content}")
        
        moments = self.get_strongest_memories(3, "moment")
        if moments:
            context.append("\nMemorable moments:")
            for mem in moments:
                context.append(f"  - {mem.content} (felt {mem.emotion})")
        
        # Recent topics
        if self.last_topics:
            context.append(f"\nRecently discussed: {', '.join(self.last_topics)}")
        
        return "\n".join(context)
    
    # ==================== PERSISTENCE ====================
    
    def save(self, personality=None):
        """Save memories and optionally personality state to disk."""
        # Save memories
        memories_data = {
            "memories": [m.to_dict() for m in self.memories],
            "conversation_history": [c.to_dict() for c in self.conversation_history],
            "owner_name": self.owner_name,
            "favorite_topics": self.favorite_topics,
            "last_topics": self.last_topics
        }
        
        with open(self._memories_file, 'w') as f:
            json.dump(memories_data, f, indent=2)
        
        # Save personality state
        if personality:
            with open(self._state_file, 'w') as f:
                json.dump(personality.to_dict(), f, indent=2)
    
    def load(self) -> Optional[dict]:
        """
        Load memories from disk.
        Returns personality dict if state.json exists, None otherwise.
        """
        # Load memories
        if self._memories_file.exists():
            with open(self._memories_file, 'r') as f:
                data = json.load(f)
            
            self.memories = [Memory.from_dict(m) for m in data.get("memories", [])]
            self.conversation_history = [
                ConversationExchange.from_dict(c) 
                for c in data.get("conversation_history", [])
            ]
            self.owner_name = data.get("owner_name", "Friend")
            self.favorite_topics = data.get("favorite_topics", [])
            self.last_topics = data.get("last_topics", [])
        
        # Load personality state
        if self._state_file.exists():
            with open(self._state_file, 'r') as f:
                return json.load(f)
        
        return None
    
    # ==================== DISPLAY ====================
    
    def get_memories_display(self) -> str:
        """Get formatted display of memories."""
        lines = [
            "╭─────────────────────────────────────╮",
            "│         CLAUDEAGOTCHI MEMORIES      │",
            "├─────────────────────────────────────┤"
        ]
        
        if not self.memories:
            lines.append("│  No memories yet!                   │")
        else:
            # Group by type
            for mem_type in ["fact", "preference", "moment", "topic"]:
                mems = [m for m in self.memories if m.type == mem_type]
                if mems:
                    lines.append(f"│  {mem_type.upper()}S:")
                    for m in sorted(mems, key=lambda x: -x.strength)[:3]:
                        strength_bar = "●" * int(m.strength * 5)
                        content = m.content[:28] + "..." if len(m.content) > 28 else m.content
                        lines.append(f"│    [{strength_bar:5}] {content}")
        
        lines.append("╰─────────────────────────────────────╯")
        return "\n".join(lines)


# ==================== TESTING ====================

if __name__ == "__main__":
    # Quick test
    mem = MemorySystem(data_dir="data_test")
    
    # Add some memories
    mem.add_fact("Likes coffee in the morning")
    mem.add_fact("Works as a programmer")
    mem.add_preference("Prefers short responses")
    mem.add_moment("Had a fun conversation about space", emotion="happy")
    mem.add_topic("Python")
    mem.add_topic("ESP32")
    
    print(mem.get_memories_display())
    
    # Test save/load
    mem.save()
    
    mem2 = MemorySystem(data_dir="data_test")
    mem2.load()
    
    print("\nAfter reload:")
    print(mem2.get_memories_display())
    
    # Cleanup test dir
    import shutil
    shutil.rmtree("data_test", ignore_errors=True)
