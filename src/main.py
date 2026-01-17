#!/usr/bin/env python3
"""
Claudeagotchi - Main Entry Point

A tiny AI companion that lives with you.
Run this to start your Claudeagotchi!
"""

import os
import sys
import json
import time
import threading
import random
from pathlib import Path

# Add src to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from personality import Personality
from memory import MemorySystem
from claude_api import ClaudeAPI, MockClaudeAPI
from behaviors import BehaviorEngine, IdleBehaviors
from scheduler import Scheduler
from display.terminal_face import create_display


# ==================== CONFIGURATION ====================

def load_config() -> dict:
    """Load configuration from config.json."""
    config_paths = [
        Path(__file__).parent.parent / "config.json",
        Path("config.json"),
    ]
    
    for config_path in config_paths:
        if config_path.exists():
            with open(config_path) as f:
                return json.load(f)
    
    print("⚠️  No config.json found!")
    print("   Copy config.example.json to config.json and add your API key.")
    print("   Running in MOCK mode (no real API calls).\n")
    
    return {
        "api_key": "",
        "owner_name": "Friend",
        "proactive_enabled": True,
        "proactive_interval_minutes": 30,
        "display_mode": "terminal",
        "model": "claude-sonnet-4-20250514",
        "max_response_tokens": 150,
        "debug": False
    }


# ==================== MAIN APPLICATION ====================

class Claudeagotchi:
    """
    The main Claudeagotchi application.
    Orchestrates all systems and runs the main loop.
    """
    
    def __init__(self, config: dict):
        self.config = config
        self.running = False
        
        # Initialize display
        print("Initializing display...")
        self.display = create_display(use_color=True)
        
        # Initialize data directory
        self.data_dir = Path(__file__).parent.parent / "data"
        self.data_dir.mkdir(exist_ok=True)
        
        # Initialize memory system
        print("Loading memories...")
        self.memory = MemorySystem(data_dir=str(self.data_dir))
        personality_data = self.memory.load()
        
        # Initialize personality
        if personality_data:
            print("Restoring personality...")
            self.personality = Personality.from_dict(personality_data)
        else:
            print("Creating new personality...")
            self.personality = Personality()
        
        # Set owner name
        self.memory.owner_name = config.get("owner_name", "Friend")
        
        # Initialize Claude API
        api_key = config.get("api_key", "")
        if api_key and api_key != "YOUR_ANTHROPIC_API_KEY_HERE":
            print("Connecting to Claude API...")
            self.api = ClaudeAPI(
                api_key=api_key,
                model=config.get("model", "claude-sonnet-4-20250514"),
                max_tokens=config.get("max_response_tokens", 150)
            )
            self.api.set_debug(config.get("debug", False))
            self.use_mock = False
        else:
            print("Using mock API (no real API calls)...")
            self.api = MockClaudeAPI()
            self.use_mock = True
        
        # Initialize behavior engine
        self.behaviors = BehaviorEngine(self.personality, self.memory, self.api)
        if config.get("proactive_enabled", True):
            interval_min = config.get("proactive_interval_minutes", 30)
            self.behaviors.set_min_interval(interval_min * 60)
        
        # Initialize idle behaviors
        self.idle = IdleBehaviors(self.personality)
        
        # Initialize scheduler
        self.scheduler = Scheduler()
        self._setup_scheduler()
        
        # State
        self._last_expression = "neutral"
        self._proactive_pending = None
    
    def _setup_scheduler(self):
        """Set up scheduled tasks."""
        # Personality update (every minute)
        self.scheduler.add_task("personality_update", 60000, self._update_personality)
        
        # Memory decay (every hour)
        self.scheduler.add_task("memory_decay", 3600000, self._decay_memories)
        
        # Auto-save (every 5 minutes)
        self.scheduler.add_task("auto_save", 300000, self._auto_save)
        
        # Blink (random interval, checked every 3 seconds)
        self.scheduler.add_task("blink_check", 3000, self._maybe_blink)
        
        # Proactive check (every minute)
        if self.config.get("proactive_enabled", True):
            self.scheduler.add_task("proactive_check", 60000, self._check_proactive)
    
    def _update_personality(self):
        """Update personality state over time."""
        self.personality.update()
    
    def _decay_memories(self):
        """Apply memory decay."""
        self.memory.update_decay()
    
    def _auto_save(self):
        """Auto-save state."""
        self.memory.save(self.personality)
    
    def _maybe_blink(self):
        """Maybe trigger a blink animation."""
        if self.idle.should_blink():
            self.display.animate_blink()
            self.display.set_expression(self._last_expression)
            self.display.render(clear=False)
    
    def _check_proactive(self):
        """Check for proactive events."""
        event = self.behaviors.check()
        if event:
            self._proactive_pending = event
    
    def _handle_proactive(self):
        """Handle a pending proactive event."""
        event = self._proactive_pending
        self._proactive_pending = None
        
        print(f"\n💭 {event.message}")
        self.display.set_expression(event.expression)
        self.display.set_message(event.message)
        self.display.render()
        
        self._last_expression = event.expression
        self.idle.reset_timer()
    
    def chat(self, user_input: str) -> str:
        """
        Process user input and get a response.
        """
        # Show thinking
        self.display.show_thinking()
        
        # Build context
        context = self.memory.build_context_string(self.personality)
        
        # Get conversation history
        history = []
        for ex in self.memory.conversation_history[-3:]:
            history.append({"role": "user", "content": ex.user_message})
            history.append({"role": "assistant", "content": ex.assistant_message})
        
        # Call API
        success, response, metadata = self.api.chat(user_input, context, history)
        
        if not success:
            self.display.set_expression("confused")
            self.display.set_message(f"Oops: {response}")
            self.display.render()
            return response
        
        # Update personality
        self.personality.on_interaction(positive=True)
        
        # Update expression
        expr = metadata.get("suggested_expression", "neutral")
        self.display.set_expression(expr)
        self._last_expression = expr
        
        # Store memories
        for mem in metadata.get("potential_memories", []):
            if mem["type"] == "fact":
                self.memory.add_fact(mem["content"])
            elif mem["type"] == "preference":
                self.memory.add_preference(mem["content"])
        
        # Store conversation
        self.memory.add_conversation(user_input, response, self.personality.state.mood)
        
        # Track topics
        for topic in metadata.get("detected_topics", []):
            self.memory.add_topic(topic)
        
        # Display response
        self.display.set_message(response)
        self.display.render()
        
        # Reset idle timer
        self.idle.reset_timer()
        
        return response
    
    def handle_command(self, command: str) -> bool:
        """
        Handle special commands.
        Returns True if command was handled, False otherwise.
        """
        cmd = command.lower().strip()
        
        if cmd == "/mood" or cmd == "/status":
            print(self.personality.get_status_display())
            return True
        
        if cmd == "/memories":
            print(self.memory.get_memories_display())
            return True
        
        if cmd == "/poke":
            self.display.set_expression("surprised")
            self.display.set_message("Hey! That tickles!")
            self.display.render()
            self.personality.on_interaction()
            self.idle.reset_timer()
            time.sleep(1)
            self.display.set_expression("happy")
            self.display.set_message("Hi there! :)")
            self.display.render()
            self._last_expression = "happy"
            return True
        
        if cmd == "/sleep":
            self.display.set_expression("sleepy")
            self.display.set_message("*yawns* Okay, I'll rest...")
            self.display.render()
            time.sleep(1)
            self.display.set_expression("sleeping")
            self.display.set_message("zzz...")
            self.display.render()
            self._last_expression = "sleeping"
            self.personality.rest(30)
            return True
        
        if cmd == "/wake":
            self.display.animate_wake_up()
            self.display.set_message("Good morning! :)")
            self.display.render()
            self._last_expression = "happy"
            return True
        
        if cmd == "/reset":
            confirm = input("Are you sure? This will reset personality and memories. (yes/no): ")
            if confirm.lower() == "yes":
                self.personality = Personality()
                self.memory.memories.clear()
                self.memory.conversation_history.clear()
                self.memory.save(self.personality)
                self.display.set_expression("neutral")
                self.display.set_message("Hello! I'm... new here. Nice to meet you!")
                self.display.render()
                self._last_expression = "neutral"
            return True
        
        if cmd == "/debug":
            self.api.set_debug(not self.api._debug)
            print(f"Debug mode: {'ON' if self.api._debug else 'OFF'}")
            return True
        
        if cmd == "/save":
            self.memory.save(self.personality)
            print("Saved!")
            return True
        
        if cmd == "/help":
            print("""
╭───────────────────────────────────────╮
│           COMMANDS                    │
├───────────────────────────────────────┤
│  /mood     - Show personality status  │
│  /memories - Show stored memories     │
│  /poke     - Get attention            │
│  /sleep    - Put to sleep             │
│  /wake     - Wake up                  │
│  /reset    - Reset everything         │
│  /save     - Force save               │
│  /debug    - Toggle debug mode        │
│  /help     - Show this help           │
│  /quit     - Exit                     │
╰───────────────────────────────────────╯
""")
            return True
        
        if cmd == "/quit" or cmd == "/exit":
            self.running = False
            return True
        
        return False
    
    def run(self):
        """Main run loop."""
        self.running = True
        
        # Show boot sequence
        self.display.clear_screen()
        print("\n  Waking up...\n")
        time.sleep(0.5)
        self.display.animate_wake_up()
        
        # Greeting based on time since last interaction
        hours_since = self.personality.time_since_last_interaction() / 3600
        if hours_since > 24:
            self.display.set_message(f"Hey {self.memory.owner_name}! It's been a while... I missed you!")
        elif hours_since > 8:
            self.display.set_message(f"Hi {self.memory.owner_name}! Good to see you again :)")
        else:
            self.display.set_message("Hey! I'm here :)")
        
        self.display.render()
        self.personality.on_interaction()
        
        # Status bar
        mode = "MOCK" if self.use_mock else "LIVE"
        self.display.set_status_bar(f"[{mode}] Type to chat, /help for commands")
        
        print("\n" + "─" * 40)
        print("Type a message and press Enter to chat.")
        print("Type /help for commands, /quit to exit.")
        print("─" * 40 + "\n")
        
        # Main loop
        try:
            while self.running:
                # Update scheduled tasks
                self.scheduler.update()
                
                # Handle pending proactive event
                if self._proactive_pending:
                    self._handle_proactive()
                
                # Non-blocking input check (simplified - just use input())
                try:
                    user_input = input(f"You: ").strip()
                except EOFError:
                    break
                
                if not user_input:
                    continue
                
                # Check for commands
                if user_input.startswith("/"):
                    if self.handle_command(user_input):
                        continue
                
                # Chat!
                response = self.chat(user_input)
                print(f"Claude: {response}\n")
        
        except KeyboardInterrupt:
            print("\n\nInterrupted!")
        
        finally:
            # Goodbye
            self.display.set_expression("sad")
            self.display.set_message("Goodbye... see you soon!")
            self.display.render()
            
            # Save state
            print("\nSaving state...")
            self.memory.save(self.personality)
            print("Done!")


# ==================== ENTRY POINT ====================

def main():
    """Main entry point."""
    print("""
    ╔═══════════════════════════════════════╗
    ║                                       ║
    ║         🐣 CLAUDEAGOTCHI 🐣           ║
    ║                                       ║
    ║     A tiny AI companion that          ║
    ║       lives with you                  ║
    ║                                       ║
    ╚═══════════════════════════════════════╝
    """)
    
    # Load config
    config = load_config()
    
    # Create and run
    gotchi = Claudeagotchi(config)
    gotchi.run()


if __name__ == "__main__":
    main()
