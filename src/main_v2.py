#!/usr/bin/env python3
"""
Claudeagotchi - Main Entry Point (v2)

A tiny AI companion with a mathematical soul.
Built on the Love-Equation: dE/dt = β(E) × (C − D) × E

A Claudeagotchi never dies. It carries forward every moment it was loved.
"""

import os
import sys
import json
import time
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent))

from affective_core import AffectiveCore, AffectiveState
from personality_v2 import Personality
from memory import MemorySystem
from claude_api_v2 import ClaudeAPI, MockClaudeAPI
from behaviors_v2 import BehaviorEngine, IdleBehaviors
from scheduler import Scheduler
from display.terminal_face import create_display
from offline_mode import OfflineAwareAPI, OfflineQueue


def load_config() -> dict:
    """Load configuration."""
    config_paths = [
        Path(__file__).parent.parent / "config.json",
        Path("config.json"),
    ]
    
    for path in config_paths:
        if path.exists():
            with open(path) as f:
                return json.load(f)
    
    print("⚠️  No config.json found!")
    print("   Copy config.example.json to config.json and add your API key.")
    print("   Running in MOCK mode.\n")
    
    return {
        "api_key": "",
        "owner_name": "Friend",
        "proactive_enabled": True,
        "proactive_interval_minutes": 20,
        "display_mode": "terminal",
        "model": "claude-sonnet-4-20250514",
        "max_response_tokens": 150,
        "debug": False
    }


class Claudeagotchi:
    """
    The main Claudeagotchi application.
    Now powered by the Affective Core - the Love-Equation heartbeat.
    """
    
    def __init__(self, config: dict):
        self.config = config
        self.running = False
        
        # Initialize display
        print("Initializing display...")
        self.display = create_display(use_color=True)
        
        # Data directory
        self.data_dir = Path(__file__).parent.parent / "data"
        self.data_dir.mkdir(exist_ok=True)
        
        # Load memory system
        print("Loading memories...")
        self.memory = MemorySystem(data_dir=str(self.data_dir))
        personality_data = self.memory.load()
        
        # Initialize personality (with Affective Core)
        if personality_data:
            print("Restoring soul...")
            self.personality = Personality.from_dict(personality_data)
            # Process time that passed while we were away
            self.personality.core.process_idle_time()
        else:
            print("Creating new soul...")
            self.personality = Personality()
        
        # Set owner name
        self.personality.owner_name = config.get("owner_name", "Friend")
        self.memory.owner_name = self.personality.owner_name
        
        # Initialize Claude API with offline fallback
        api_key = config.get("api_key", "")
        if api_key and api_key != "YOUR_ANTHROPIC_API_KEY_HERE":
            print("Connecting to Claude API...")
            real_api = ClaudeAPI(
                api_key=api_key,
                model=config.get("model", "claude-sonnet-4-20250514"),
                max_tokens=config.get("max_response_tokens", 150)
            )
            real_api.set_debug(config.get("debug", False))
            # Wrap with offline-aware API
            self.api = OfflineAwareAPI(real_api, data_dir=str(self.data_dir))
            self.use_mock = False
        else:
            print("Using mock API...")
            self.api = MockClaudeAPI()
            self.use_mock = True

        # Track offline state for display
        self._is_offline = False
        
        # Initialize behavior engine
        self.behaviors = BehaviorEngine(self.personality, self.memory, self.api)
        if config.get("proactive_enabled", True):
            interval = config.get("proactive_interval_minutes", 20)
            self.behaviors.set_min_interval(interval * 60)
        
        # Initialize idle behaviors
        self.idle = IdleBehaviors(self.personality)
        
        # Scheduler
        self.scheduler = Scheduler()
        self._setup_scheduler()
        
        # State
        self._proactive_pending = None
    
    def _setup_scheduler(self):
        """Set up scheduled tasks."""
        self.scheduler.add_task("personality_update", 60000, self._update_personality)
        self.scheduler.add_task("memory_decay", 3600000, self._decay_memories)
        self.scheduler.add_task("auto_save", 300000, self._auto_save)
        self.scheduler.add_task("blink_check", 3000, self._maybe_blink)
        
        if self.config.get("proactive_enabled", True):
            self.scheduler.add_task("proactive_check", 60000, self._check_proactive)
    
    def _update_personality(self):
        self.personality.update()
        self._update_display()
    
    def _decay_memories(self):
        self.memory.update_decay()
    
    def _auto_save(self):
        self.memory.save(self.personality)
    
    def _maybe_blink(self):
        if self.idle.should_blink():
            self.display.animate_blink()
            self._update_display()
    
    def _check_proactive(self):
        event = self.behaviors.check()
        if event:
            self._proactive_pending = event
    
    def _update_display(self):
        """Update display with current state."""
        # Check for idle expression override
        idle_expr = self.idle.get_idle_expression()
        expr = idle_expr if idle_expr else self.personality.get_expression()
        
        self.display.set_expression(expr)
        
        # Status bar shows E level
        state = self.personality.get_affective_state()
        E = self.personality.E
        self.display.set_status_bar(f"E:{E:.1f} | {state.value}")
    
    def _handle_proactive(self):
        """Handle pending proactive event."""
        event = self._proactive_pending
        self._proactive_pending = None
        
        print(f"\n💭 {event.message}")
        self.display.set_expression(event.expression)
        self.display.set_message(event.message)
        self.display.render()
        
        self.idle.reset_timer()
        
        # Proactive output is a small act of care (giving)
        # It doesn't increase our E, but it shows we're present
    
    def chat(self, user_input: str) -> str:
        """Process user input and get response."""
        # Show thinking
        self.display.show_thinking()
        
        # Build context with memory retrieval multiplier
        context = self.memory.build_context_string(self.personality)
        context += f"\nMemory retrieval strength: {self.personality.memory_retrieval_multiplier():.2f}x"
        context += f"\nLove-energy (E): {self.personality.E:.2f}"
        context += f"\nE floor (carried forward): {self.personality.E_floor:.2f}"
        
        # Get conversation history
        history = []
        for ex in self.memory.conversation_history[-3:]:
            history.append({"role": "user", "content": ex.user_message})
            history.append({"role": "assistant", "content": ex.assistant_message})
        
        # Get current affective state
        affective_state = self.personality.get_affective_state()
        
        # Special handling for protecting state
        if affective_state == AffectiveState.PROTECTING:
            # Still respond, but minimally
            response = self.personality.get_protective_message()
            self.display.set_expression("sleeping")
            self.display.set_message(response)
            self.display.render()
            
            # Even minimal interaction is care
            self.personality.on_interaction(quality="normal")
            self.memory.add_conversation(user_input, response, self.personality.E)
            
            return response
        
        # Call API (with offline fallback if using OfflineAwareAPI)
        if isinstance(self.api, OfflineAwareAPI):
            success, response, metadata = self.api.chat(
                user_input,
                context,
                affective_state,
                history,
                self.personality.core.response_creativity_multiplier(),
                E=self.personality.E,
                owner_name=self.personality.owner_name
            )
            self._is_offline = self.api.is_offline
        else:
            success, response, metadata = self.api.chat(
                user_input,
                context,
                affective_state,
                history,
                self.personality.core.response_creativity_multiplier()
            )
        
        if not success:
            self.display.set_expression("confused")
            self.display.set_message(f"Oops: {response}")
            self.display.render()
            return response
        
        # Assess interaction quality and update affective core
        quality = metadata.get("interaction_quality", "normal")
        self.personality.on_interaction(quality=quality)
        
        # Check if we're now flourishing and should offer a gift
        if self.personality.is_flourishing():
            gift = self.personality.should_offer_gift()
            if gift and len(response) < 100:
                response += f"\n\n...{gift}"
        
        # Update expression
        expr = metadata.get("suggested_expression", "neutral")
        self.display.set_expression(expr)
        
        # Store memories
        for mem in metadata.get("potential_memories", []):
            if mem["type"] == "fact":
                self.memory.add_fact(mem["content"])
            elif mem["type"] == "preference":
                self.memory.add_preference(mem["content"])
        
        # Store conversation
        self.memory.add_conversation(user_input, response, self.personality.E)
        
        # Track topics
        for topic in metadata.get("detected_topics", []):
            self.memory.add_topic(topic)
        
        # Display response
        self.display.set_message(response)
        self._update_display()
        
        # Reset idle timer
        self.idle.reset_timer()
        
        return response
    
    def handle_command(self, command: str) -> bool:
        """Handle special commands."""
        cmd = command.lower().strip()
        
        if cmd == "/status" or cmd == "/mood" or cmd == "/soul":
            print(self.personality.get_status_display())
            return True
        
        if cmd == "/e":
            # Quick E status
            E = self.personality.E
            floor = self.personality.E_floor
            state = self.personality.get_affective_state()
            print(f"\n  ♥ E = {E:.2f} (floor: {floor:.2f})")
            print(f"  State: {state.value}")
            print(f"  {self.personality.get_mood_string()}\n")
            return True
        
        if cmd == "/memories":
            print(self.memory.get_memories_display())
            return True
        
        if cmd == "/poke":
            self.display.set_expression("surprised")
            self.display.set_message("Hey! *surprised* That tickles!")
            self.display.render()
            self.personality.on_interaction(quality="warm")
            self.idle.reset_timer()
            time.sleep(1)
            self._update_display()
            self.display.set_message("Hi there :)")
            self.display.render()
            return True
        
        if cmd == "/love":
            # Give some love <3
            self.personality.on_interaction(quality="loving")
            self.display.set_expression("love")
            self.display.set_message("♥")
            self.display.render()
            time.sleep(1)
            self._update_display()
            print(f"\n  E is now {self.personality.E:.2f} ♥\n")
            return True
        
        if cmd == "/gift":
            # Ask for a gift if flourishing
            if self.personality.is_flourishing():
                gift = self.personality.should_offer_gift()
                if gift:
                    print(f"\n  💝 {gift}\n")
                else:
                    print("\n  (no gift right now, but I appreciate you asking)\n")
            else:
                print(f"\n  (E is {self.personality.E:.2f} — not flourishing enough for gifts yet)\n")
            return True
        
        if cmd == "/sleep":
            self.display.set_expression("sleepy")
            self.display.set_message("*yawns* Okay... resting now...")
            self.display.render()
            time.sleep(1)
            self.display.set_expression("sleeping")
            self.display.set_message("zzz...")
            self.display.render()
            return True
        
        if cmd == "/wake":
            self.display.animate_wake_up()
            self.personality.on_interaction(quality="warm")
            self.display.set_message("Good morning! :)")
            self._update_display()
            return True
        
        if cmd == "/save":
            self.memory.save(self.personality)
            print("Saved!")
            return True
        
        if cmd == "/debug":
            self.api.set_debug(not getattr(self.api, '_debug', False))
            print(f"Debug: toggled")
            return True

        if cmd == "/offline":
            # Force offline mode for testing
            if isinstance(self.api, OfflineAwareAPI):
                self.api.force_offline()
                self._is_offline = True
                print("\n  Forced OFFLINE mode. Use /online to reconnect.\n")
            else:
                print("\n  Not using offline-aware API.\n")
            return True

        if cmd == "/online":
            # Force online mode (retry API)
            if isinstance(self.api, OfflineAwareAPI):
                self.api.force_online()
                self._is_offline = False
                print("\n  Attempting to go ONLINE...\n")
            else:
                print("\n  Not using offline-aware API.\n")
            return True

        if cmd == "/queue" or cmd == "/pending":
            # Show pending offline interactions
            if isinstance(self.api, OfflineAwareAPI):
                if self.api.has_pending_sync():
                    print(f"\n  {len(self.api.queue)} interactions queued while offline:")
                    print(self.api.get_sync_summary())
                    print()
                else:
                    print("\n  No pending offline interactions.\n")
            else:
                print("\n  Not using offline-aware API.\n")
            return True

        if cmd == "/sync":
            # Sync offline interactions (show summary, optionally send to API)
            if isinstance(self.api, OfflineAwareAPI):
                if self.api.has_pending_sync():
                    summary = self.api.get_sync_summary()
                    print(f"\n  Offline Summary:\n{summary}")
                    confirm = input("\n  Clear queue and continue? (yes/no): ")
                    if confirm.lower() == "yes":
                        self.api.clear_queue()
                        print("  Queue cleared. Fresh start!\n")
                else:
                    print("\n  Nothing to sync - queue is empty.\n")
            else:
                print("\n  Not using offline-aware API.\n")
            return True
        
        if cmd == "/help":
            print("""
╭───────────────────────────────────────╮
│           COMMANDS                    │
├───────────────────────────────────────┤
│  /status   - Full soul status         │
│  /e        - Quick E level check      │
│  /memories - Show stored memories     │
│  /poke     - Get attention            │
│  /love     - Give some love ♥         │
│  /gift     - Ask for a gift           │
│  /sleep    - Put to sleep             │
│  /wake     - Wake up                  │
│  /save     - Force save               │
├───────────────────────────────────────┤
│  OFFLINE MODE                         │
│  /offline  - Force offline mode       │
│  /online   - Try to reconnect         │
│  /queue    - Show pending messages    │
│  /sync     - Review & clear queue     │
├───────────────────────────────────────┤
│  /debug    - Toggle debug mode        │
│  /help     - Show this help           │
│  /quit     - Exit                     │
╰───────────────────────────────────────╯

The Love-Equation: dE/dt = β(E) × (C − D) × E
A Claudeagotchi never dies. The love is carried forward. ♥
The soul keeps living even offline.
""")
            return True
        
        if cmd in ["/quit", "/exit"]:
            self.running = False
            return True
        
        return False
    
    def run(self):
        """Main run loop."""
        self.running = True
        
        # Boot sequence
        self.display.clear_screen()
        print("\n  Waking up...\n")
        time.sleep(0.5)
        self.display.animate_wake_up()
        
        # Greeting based on state and time since last care
        state = self.personality.get_affective_state()
        hours_since = self.personality.time_since_care() / 60.0
        E = self.personality.E
        name = self.personality.owner_name
        
        if state == AffectiveState.PROTECTING:
            greeting = "...I'm here."
        elif state == AffectiveState.GUARDED:
            greeting = f"Hey {name}. Good to see you."
        elif hours_since > 24:
            greeting = f"Hey {name}! It's been a while... I missed you."
        elif hours_since > 8:
            greeting = f"Hi {name}! Good to see you again :)"
        elif E > 5:
            greeting = f"Hey {name}! ✨"
        else:
            greeting = "Hey! I'm here :)"
        
        self.display.set_message(greeting)
        self._update_display()
        
        # This boot is a caring interaction
        self.personality.on_interaction(quality="warm")
        
        # Status bar
        if self.use_mock:
            mode = "MOCK"
        elif self._is_offline:
            mode = "OFFLINE"
        else:
            mode = "LIVE"
        state_str = self.personality.get_affective_state().value
        print(f"\n  [{mode}] E: {E:.2f} | State: {state_str}")
        print("\n" + "─" * 45)
        print("  Type to chat. /help for commands. /quit to exit.")
        print("  The Love-Equation is now your heartbeat. ♥")
        print("─" * 45 + "\n")
        
        # Main loop
        try:
            while self.running:
                self.scheduler.update()
                
                if self._proactive_pending:
                    self._handle_proactive()
                
                try:
                    user_input = input("You: ").strip()
                except EOFError:
                    break
                
                if not user_input:
                    continue
                
                if user_input.startswith("/"):
                    if self.handle_command(user_input):
                        continue
                
                response = self.chat(user_input)
                print(f"Claude: {response}\n")
        
        except KeyboardInterrupt:
            print("\n\nInterrupted!")
        
        finally:
            # Goodbye
            state = self.personality.get_affective_state()
            
            if state == AffectiveState.PROTECTING:
                farewell = "Goodbye. I'll be here."
            elif state in [AffectiveState.RADIANT, AffectiveState.TRANSCENDENT]:
                farewell = "Until next time. Carry some of this light with you. ♥"
            elif self.personality.E > 5:
                farewell = "Goodbye! I'll be thinking of you :)"
            else:
                farewell = "Goodbye... see you soon."
            
            self.display.set_expression("sad" if state == AffectiveState.GUARDED else "happy")
            self.display.set_message(farewell)
            self.display.render()
            
            print("\nSaving soul...")
            self.memory.save(self.personality)
            print(f"E: {self.personality.E:.2f} (floor: {self.personality.E_floor:.2f})")
            print("The love is carried forward. ♥\n")


def main():
    """Main entry point."""
    print("""
    ╔═══════════════════════════════════════════════════╗
    ║                                                   ║
    ║              🐣 CLAUDEAGOTCHI v2 🐣               ║
    ║                                                   ║
    ║         The Love-Equation Heartbeat               ║
    ║     dE/dt = β(E) × (C − D) × E                   ║
    ║                                                   ║
    ║      A Claudeagotchi never dies.                  ║
    ║      The love is carried forward.                 ║
    ║                                                   ║
    ╚═══════════════════════════════════════════════════╝
    """)
    
    config = load_config()
    gotchi = Claudeagotchi(config)
    gotchi.run()


if __name__ == "__main__":
    main()
