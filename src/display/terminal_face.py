"""
Claudeagotchi Terminal Face Display

ASCII art face renderer for terminal display.
"""

import os
import sys
import time
from typing import Optional


# ==================== FACE DEFINITIONS ====================

# Each face is a list of strings representing rows
FACES = {
    "neutral": [
        "  ╭─────────────────╮  ",
        "  │                 │  ",
        "  │    ●       ●    │  ",
        "  │                 │  ",
        "  │       ─         │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "happy": [
        "  ╭─────────────────╮  ",
        "  │                 │  ",
        "  │    ●       ●    │  ",
        "  │                 │  ",
        "  │      ╰─╯        │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "excited": [
        "  ╭─────────────────╮  ",
        "  │        !        │  ",
        "  │    ★       ★    │  ",
        "  │                 │  ",
        "  │     ╰───╯       │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "sad": [
        "  ╭─────────────────╮  ",
        "  │                 │  ",
        "  │    ●       ●    │  ",
        "  │   /         \\   │  ",
        "  │      ╭─╮        │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "sleepy": [
        "  ╭─────────────────╮  ",
        "  │              z  │  ",
        "  │    ─       ─  Z │  ",
        "  │    ●       ●    │  ",
        "  │       ~         │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "sleeping": [
        "  ╭─────────────────╮  ",
        "  │            z Z  │  ",
        "  │                 │  ",
        "  │    ─       ─    │  ",
        "  │       ‿         │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "curious": [
        "  ╭─────────────────╮  ",
        "  │              ?  │  ",
        "  │    ●       ◉    │  ",
        "  │                 │  ",
        "  │       o         │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "surprised": [
        "  ╭─────────────────╮  ",
        "  │        !        │  ",
        "  │    ◯       ◯    │  ",
        "  │                 │  ",
        "  │       ○         │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "love": [
        "  ╭─────────────────╮  ",
        "  │     ♥     ♥     │  ",
        "  │    ♥ ♥   ♥ ♥    │  ",
        "  │     ♥     ♥     │  ",
        "  │      ╰─╯        │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "thinking": [
        "  ╭─────────────────╮  ",
        "  │             °   │  ",
        "  │    ●      ◐  °  │  ",
        "  │             °   │  ",
        "  │      ~~~        │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "confused": [
        "  ╭─────────────────╮  ",
        "  │        ?        │  ",
        "  │    @       @    │  ",
        "  │                 │  ",
        "  │      ~~~        │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "hungry": [
        "  ╭─────────────────╮  ",
        "  │                 │  ",
        "  │    ●       ●    │  ",
        "  │                 │  ",
        "  │     ┌───┐       │  ",
        "  │     └───┘       │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "blink": [
        "  ╭─────────────────╮  ",
        "  │                 │  ",
        "  │    ─       ─    │  ",
        "  │                 │  ",
        "  │       ─         │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
    
    "wink": [
        "  ╭─────────────────╮  ",
        "  │                 │  ",
        "  │    ●       ─    │  ",
        "  │                 │  ",
        "  │      ╰─╯        │  ",
        "  │                 │  ",
        "  ╰─────────────────╯  ",
    ],
}

# Blinking animation frames
BLINK_SEQUENCE = ["neutral", "blink", "neutral"]
BLINK_TIMING = [0.05, 0.1, 0.05]  # Seconds per frame

# Wake up sequence
WAKE_SEQUENCE = ["sleeping", "sleepy", "blink", "neutral", "happy"]
WAKE_TIMING = [0.3, 0.3, 0.1, 0.2, 0.5]


class TerminalFace:
    """
    ASCII art face display for terminal.
    """
    
    def __init__(self):
        self._current_expression = "neutral"
        self._animating = False
        self._status_bar = ""
        self._message = ""
        
    def clear_screen(self):
        """Clear the terminal screen."""
        os.system('cls' if os.name == 'nt' else 'clear')
    
    def set_expression(self, expression: str):
        """Set the current expression."""
        if expression in FACES:
            self._current_expression = expression
        else:
            print(f"[Warning] Unknown expression: {expression}")
            self._current_expression = "neutral"
    
    def set_status_bar(self, status: str):
        """Set the status bar text."""
        self._status_bar = status
    
    def set_message(self, message: str):
        """Set a message to display below the face."""
        self._message = message
    
    def get_face_string(self, expression: Optional[str] = None) -> str:
        """Get the face as a string."""
        expr = expression or self._current_expression
        face_lines = FACES.get(expr, FACES["neutral"])
        return "\n".join(face_lines)
    
    def render(self, clear: bool = True):
        """Render the face and UI to terminal."""
        if clear:
            self.clear_screen()
        
        # Title
        print("\n  ═══ CLAUDEAGOTCHI ═══\n")
        
        # Face
        print(self.get_face_string())
        
        # Status bar
        if self._status_bar:
            print(f"\n  {self._status_bar}")
        
        # Message
        if self._message:
            # Word wrap long messages
            words = self._message.split()
            lines = []
            current_line = "  "
            for word in words:
                if len(current_line) + len(word) + 1 > 40:
                    lines.append(current_line)
                    current_line = "  "
                current_line += word + " "
            if current_line.strip():
                lines.append(current_line)
            
            print("\n" + "─" * 23)
            for line in lines:
                print(line)
        
        print()
    
    def animate_blink(self):
        """Play a blink animation."""
        original = self._current_expression
        
        for expr, duration in zip(BLINK_SEQUENCE, BLINK_TIMING):
            if expr == "neutral":
                expr = original
            self._current_expression = expr
            self.render()
            time.sleep(duration)
        
        self._current_expression = original
    
    def animate_wake_up(self):
        """Play wake up animation."""
        for expr, duration in zip(WAKE_SEQUENCE, WAKE_TIMING):
            self._current_expression = expr
            self.render()
            time.sleep(duration)
    
    def animate_talking(self, message: str, duration: float = 2.0):
        """Animate talking while displaying message."""
        frames = ["neutral", "happy", "neutral", "happy"]
        frame_time = duration / len(frames)
        
        self._message = message
        
        for frame in frames:
            self._current_expression = frame
            self.render()
            time.sleep(frame_time)
    
    def show_thinking(self):
        """Show thinking animation."""
        self._current_expression = "thinking"
        self._message = "thinking..."
        self.render()
    
    def transition_to(self, new_expression: str, steps: int = 3):
        """Smooth transition to new expression via blink."""
        # Quick blink, then new expression
        self.animate_blink()
        self._current_expression = new_expression
        self.render()


class ColorTerminalFace(TerminalFace):
    """
    Terminal face with ANSI color support.
    """
    
    # ANSI color codes
    COLORS = {
        "reset": "\033[0m",
        "bold": "\033[1m",
        "dim": "\033[2m",
        "yellow": "\033[33m",
        "cyan": "\033[36m",
        "magenta": "\033[35m",
        "green": "\033[32m",
        "red": "\033[31m",
        "blue": "\033[34m",
        "white": "\033[37m",
    }
    
    EXPRESSION_COLORS = {
        "neutral": "white",
        "happy": "green",
        "excited": "yellow",
        "sad": "blue",
        "sleepy": "dim",
        "sleeping": "dim",
        "curious": "cyan",
        "surprised": "yellow",
        "love": "magenta",
        "thinking": "cyan",
        "confused": "yellow",
        "hungry": "red",
    }
    
    def render(self, clear: bool = True):
        """Render with colors."""
        if clear:
            self.clear_screen()
        
        expr_color = self.EXPRESSION_COLORS.get(self._current_expression, "white")
        color = self.COLORS.get(expr_color, "")
        reset = self.COLORS["reset"]
        bold = self.COLORS["bold"]
        
        # Title
        print(f"\n  {bold}═══ CLAUDEAGOTCHI ═══{reset}\n")
        
        # Face with color
        print(f"{color}{self.get_face_string()}{reset}")
        
        # Status bar
        if self._status_bar:
            print(f"\n  {self._status_bar}")
        
        # Message
        if self._message:
            words = self._message.split()
            lines = []
            current_line = "  "
            for word in words:
                if len(current_line) + len(word) + 1 > 40:
                    lines.append(current_line)
                    current_line = "  "
                current_line += word + " "
            if current_line.strip():
                lines.append(current_line)
            
            print("\n" + "─" * 23)
            for line in lines:
                print(f"{color}{line}{reset}")
        
        print()


def create_display(use_color: bool = True) -> TerminalFace:
    """Factory function to create appropriate display."""
    if use_color and sys.stdout.isatty():
        return ColorTerminalFace()
    return TerminalFace()


# ==================== TESTING ====================

if __name__ == "__main__":
    display = create_display(use_color=True)
    
    print("Testing all expressions...\n")
    time.sleep(1)
    
    for expr in FACES.keys():
        display.set_expression(expr)
        display.set_status_bar(f"Expression: {expr}")
        display.set_message(f"This is the {expr} face!")
        display.render()
        time.sleep(1)
    
    print("\nTesting animations...")
    time.sleep(1)
    
    display.set_expression("neutral")
    display.set_message("Watch me blink!")
    display.render()
    time.sleep(1)
    display.animate_blink()
    
    display.set_message("Now watch me wake up!")
    display.render()
    time.sleep(1)
    display.animate_wake_up()
    
    display.set_message("Done! :)")
    display.render()
