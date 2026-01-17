"""
Claudeagotchi Scheduler

Simple task scheduler for periodic operations.
"""

import time
from typing import Callable, Dict, Optional
from dataclasses import dataclass


@dataclass
class ScheduledTask:
    """A task that runs on a schedule."""
    name: str
    callback: Callable
    interval_ms: int
    last_run: float = 0
    enabled: bool = True
    
    def is_due(self) -> bool:
        """Check if task is due to run."""
        if not self.enabled:
            return False
        return (time.time() * 1000 - self.last_run) >= self.interval_ms
    
    def run(self):
        """Execute the task."""
        self.last_run = time.time() * 1000
        self.callback()


class Scheduler:
    """
    Simple scheduler for periodic tasks.
    """
    
    def __init__(self):
        self._tasks: Dict[str, ScheduledTask] = {}
    
    def add_task(self, name: str, interval_ms: int, callback: Callable):
        """Add a new scheduled task."""
        self._tasks[name] = ScheduledTask(
            name=name,
            callback=callback,
            interval_ms=interval_ms,
            last_run=time.time() * 1000  # Don't run immediately
        )
    
    def remove_task(self, name: str):
        """Remove a task."""
        if name in self._tasks:
            del self._tasks[name]
    
    def enable_task(self, name: str):
        """Enable a task."""
        if name in self._tasks:
            self._tasks[name].enabled = True
    
    def disable_task(self, name: str):
        """Disable a task."""
        if name in self._tasks:
            self._tasks[name].enabled = False
    
    def update(self):
        """
        Check and run any due tasks.
        Should be called regularly from the main loop.
        """
        for task in self._tasks.values():
            if task.is_due():
                try:
                    task.run()
                except Exception as e:
                    print(f"[Scheduler] Error in task '{task.name}': {e}")
    
    def run_now(self, name: str):
        """Force a task to run immediately."""
        if name in self._tasks:
            self._tasks[name].run()
    
    def get_task_info(self, name: str) -> Optional[dict]:
        """Get info about a task."""
        if name not in self._tasks:
            return None
        task = self._tasks[name]
        return {
            "name": task.name,
            "interval_ms": task.interval_ms,
            "enabled": task.enabled,
            "last_run": task.last_run,
            "time_until_next": max(0, task.interval_ms - (time.time() * 1000 - task.last_run))
        }
    
    def list_tasks(self) -> list:
        """List all tasks."""
        return [self.get_task_info(name) for name in self._tasks.keys()]


# ==================== TESTING ====================

if __name__ == "__main__":
    scheduler = Scheduler()
    
    counter = {"value": 0}
    
    def increment():
        counter["value"] += 1
        print(f"Task ran! Counter: {counter['value']}")
    
    scheduler.add_task("test", 500, increment)  # Every 500ms
    
    print("Running scheduler for 3 seconds...")
    start = time.time()
    while time.time() - start < 3:
        scheduler.update()
        time.sleep(0.1)
    
    print(f"\nFinal counter: {counter['value']}")
    print(f"Task info: {scheduler.get_task_info('test')}")
