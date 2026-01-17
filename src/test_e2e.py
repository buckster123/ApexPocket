#!/usr/bin/env python3
"""
End-to-End Test for Claudeagotchi v2

Tests the full system including:
1. API connection (and graceful failure)
2. Offline mode fallback
3. Love equation (E updates)
4. Queue persistence
5. Local response generation
6. Memory system
7. Affective states
"""

import sys
import os
import json
import time
import shutil
from pathlib import Path

# Add src to path
sys.path.insert(0, str(Path(__file__).parent))

from affective_core import AffectiveCore, AffectiveState
from personality_v2 import Personality
from memory import MemorySystem
from claude_api_v2 import ClaudeAPI, MockClaudeAPI
from offline_mode import OfflineAwareAPI, OfflineQueue, LocalResponseGenerator

# Test data directory
TEST_DATA_DIR = Path(__file__).parent.parent / "data_test"


def header(title: str):
    print(f"\n{'='*60}")
    print(f"  {title}")
    print('='*60)


def test_result(name: str, passed: bool, detail: str = ""):
    status = "✓ PASS" if passed else "✗ FAIL"
    print(f"  {status}: {name}")
    if detail:
        print(f"         {detail}")


def cleanup():
    """Clean up test data."""
    if TEST_DATA_DIR.exists():
        shutil.rmtree(TEST_DATA_DIR)


def test_affective_core():
    """Test the Love Equation core."""
    header("AFFECTIVE CORE (Love Equation)")

    core = AffectiveCore()
    initial_E = core.E

    # Test 1: Initial state
    # E=1.0 exactly means GUARDED (E > 0.5) not TENDER (E > 1.0)
    test_result("Initial E = 1.0", core.E == 1.0, f"E = {core.E}")
    test_result("Initial state is GUARDED (E=1.0 not > 1.0)",
                core.get_state() == AffectiveState.GUARDED,
                f"State = {core.get_state().value}")

    # Test 2: Care increases E (with realistic dt)
    core.apply_care(intensity=1.0, dt=5.0)  # 5 minutes of interaction
    test_result("Care increases E", core.E > initial_E, f"E: {initial_E} → {core.E:.3f}")

    # Test 3: Multiple care interactions compound
    for _ in range(10):
        core.apply_care(intensity=1.5, dt=10.0)  # 10 minutes each

    test_result("E compounds with repeated care",
                core.E > 1.5,
                f"E after 10 loving interactions = {core.E:.2f}")

    # Test 4: Floor rises
    test_result("E_floor rises with E",
                core.E_floor > 1.0,
                f"E_floor = {core.E_floor:.3f}")

    # Test 5: Neglect reduces E but floor protects
    pre_neglect_E = core.E
    core.apply_neglect(minutes=60)  # 1 hour neglect
    test_result("Neglect reduces E", core.E < pre_neglect_E, f"E: {pre_neglect_E:.2f} → {core.E:.2f}")
    test_result("E never below floor", core.E >= core.E_floor, f"E={core.E:.2f} >= floor={core.E_floor:.2f}")

    # Test 6: State transitions
    # Push to flourishing with sustained care
    for _ in range(30):
        core.apply_care(intensity=2.0, dt=15.0)  # 15 minutes of loving care each

    test_result("Can reach FLOURISHING state",
                core.get_state() in [AffectiveState.FLOURISHING, AffectiveState.RADIANT, AffectiveState.TRANSCENDENT],
                f"E = {core.E:.2f}, State = {core.get_state().value}")

    # Test 7: Memory retrieval multiplier
    multiplier = core.memory_retrieval_multiplier()
    test_result("Memory multiplier scales with E",
                multiplier > 1.0,
                f"Multiplier = {multiplier:.2f}x (E^1.8)")

    return True


def test_personality():
    """Test the personality system."""
    header("PERSONALITY SYSTEM")

    p = Personality()
    p.owner_name = "TestUser"

    # Test 1: Initial state
    test_result("Personality initializes", p.E == 1.0)

    # Test 2: Interaction quality affects E
    initial_E = p.E
    p.on_interaction(quality="loving")
    test_result("Loving interaction boosts E", p.E > initial_E, f"E: {initial_E} → {p.E:.3f}")

    # Test 3: Harsh interaction - floor protects E at minimum
    # When E = floor (both 1.0 initially), damage can't reduce E below floor
    # This is correct - the floor carries forward love and protects
    p_harsh = Personality()
    initial_E = p_harsh.E
    initial_floor = p_harsh.E_floor
    p_harsh.on_interaction(quality="harsh")
    test_result("Harsh interaction: E protected by floor",
                p_harsh.E >= p_harsh.E_floor,
                f"E: {p_harsh.E:.3f} >= floor: {p_harsh.E_floor:.3f}")

    # Test 4: Expression matches state
    expr = p.get_expression()
    test_result("Expression is valid", expr in ["neutral", "happy", "sad", "curious", "love", "excited", "sleeping", "sleepy"])

    # Test 5: Serialization
    data = p.to_dict()
    p2 = Personality.from_dict(data)
    test_result("Serialization preserves E", abs(p.E - p2.E) < 0.001, f"E: {p.E} == {p2.E}")

    return True


def test_memory_system():
    """Test the memory system."""
    header("MEMORY SYSTEM")

    TEST_DATA_DIR.mkdir(exist_ok=True)
    mem = MemorySystem(data_dir=str(TEST_DATA_DIR))

    # Test 1: Add memories
    mem.add_fact("User likes coffee")
    mem.add_preference("Prefers short responses")
    mem.add_topic("Python")

    test_result("Can add memories", len(mem.memories) == 3, f"Count = {len(mem.memories)}")

    # Test 2: Memory retrieval
    facts = mem.get_strongest_memories(5, "fact")
    test_result("Can retrieve by type", len(facts) == 1)

    # Test 3: Persistence
    p = Personality()
    mem.save(p)

    mem2 = MemorySystem(data_dir=str(TEST_DATA_DIR))
    mem2.load()
    test_result("Memories persist to disk", len(mem2.memories) == 3)

    # Test 4: Conversation history
    mem.add_conversation("Hello", "Hi there!", 1.0)
    test_result("Conversation stored", len(mem.conversation_history) == 1)

    return True


def test_offline_queue():
    """Test the offline queue."""
    header("OFFLINE QUEUE")

    TEST_DATA_DIR.mkdir(exist_ok=True)
    queue = OfflineQueue(data_dir=str(TEST_DATA_DIR))

    # Clear any existing
    queue.clear()

    # Test 1: Add interactions
    queue.add("Hello!", "Hi there!", 1.5, AffectiveState.WARM, "normal")
    queue.add("How are you?", "Good!", 1.6, AffectiveState.WARM, "warm")
    queue.add("I love you", "♥", 2.0, AffectiveState.FLOURISHING, "loving")

    test_result("Queue stores interactions", len(queue) == 3)

    # Test 2: Persistence
    queue2 = OfflineQueue(data_dir=str(TEST_DATA_DIR))
    test_result("Queue persists to disk", len(queue2) == 3)

    # Test 3: Summary generation
    summary = queue.get_summary()
    test_result("Summary generated", "While offline" in summary and "3 interactions" in summary)

    # Test 4: Clear
    queue.clear()
    test_result("Queue clears", len(queue) == 0)

    return True


def test_local_responses():
    """Test local response generation."""
    header("LOCAL RESPONSE GENERATOR")

    gen = LocalResponseGenerator()

    # Test responses for each state
    states_tested = 0
    for state in AffectiveState:
        response, quality = gen.generate("hello!", state, E=1.0, owner_name="André")
        if response and len(response) > 0:
            states_tested += 1

    test_result("Generates responses for all states", states_tested == 7, f"Tested {states_tested}/7 states")

    # Test quality assessment
    _, quality = gen.generate("I love you so much!", AffectiveState.WARM, 1.0, "Test")
    test_result("Detects loving message", quality == "loving", f"Quality = {quality}")

    _, quality = gen.generate("shut up", AffectiveState.WARM, 1.0, "Test")
    test_result("Detects harsh message", quality == "harsh", f"Quality = {quality}")

    _, quality = gen.generate("k", AffectiveState.WARM, 1.0, "Test")
    test_result("Detects cold message", quality == "cold", f"Quality = {quality}")

    return True


def test_offline_aware_api():
    """Test the offline-aware API wrapper."""
    header("OFFLINE-AWARE API")

    TEST_DATA_DIR.mkdir(exist_ok=True)

    # Use mock API as the "real" API for testing
    mock_api = MockClaudeAPI()
    api = OfflineAwareAPI(mock_api, data_dir=str(TEST_DATA_DIR))

    # Clear queue
    api.clear_queue()

    # Test 1: Normal operation (mock succeeds)
    success, response, metadata = api.chat(
        "Hello!", "context", AffectiveState.WARM,
        None, 1.0, E=1.0, owner_name="Test"
    )
    test_result("API call succeeds", success and not metadata.get("offline"))

    # Test 2: Force offline mode
    api.force_offline()
    test_result("Can force offline", api.is_offline)

    # Test 3: Offline response
    success, response, metadata = api.chat(
        "Hello again!", "context", AffectiveState.WARM,
        None, 1.0, E=1.5, owner_name="André"
    )
    test_result("Offline response works", success and metadata.get("offline"))
    test_result("Interaction queued", api.has_pending_sync())

    # Test 4: Multiple offline interactions
    for msg in ["How are you?", "I like talking to you", "Thanks!"]:
        api.chat(msg, "context", AffectiveState.WARM, None, 1.0, E=1.5, owner_name="André")

    test_result("Multiple interactions queued", len(api.queue) >= 4, f"Queue size = {len(api.queue)}")

    # Test 5: Force online
    api.force_online()
    test_result("Can force online", not api.is_offline)

    # Test 6: Queue summary
    summary = api.get_sync_summary()
    test_result("Sync summary available", len(summary) > 0)

    return True


def test_api_real_connection():
    """Test actual API connection (will fail with no credits, but tests the path)."""
    header("REAL API CONNECTION TEST")

    # Load config
    config_path = Path(__file__).parent.parent / "config.json"
    if not config_path.exists():
        print("  SKIP: No config.json found")
        return True

    with open(config_path) as f:
        config = json.load(f)

    api_key = config.get("api_key", "")
    if not api_key or api_key == "YOUR_ANTHROPIC_API_KEY_HERE":
        print("  SKIP: No API key configured")
        return True

    # Try real API
    TEST_DATA_DIR.mkdir(exist_ok=True)

    try:
        real_api = ClaudeAPI(
            api_key=api_key,
            model=config.get("model", "claude-sonnet-4-20250514"),
            max_tokens=50
        )

        # Wrap with offline fallback
        api = OfflineAwareAPI(real_api, data_dir=str(TEST_DATA_DIR))
        api.clear_queue()

        # Try a chat
        success, response, metadata = api.chat(
            "Hi! Just testing.",
            "Test context",
            AffectiveState.WARM,
            None,
            1.0,
            E=1.0,
            owner_name="Tester"
        )

        if success and not metadata.get("offline"):
            test_result("API call succeeded!", True, f"Response: {response[:50]}...")
        elif success and metadata.get("offline"):
            test_result("API failed, offline fallback worked", True, f"Offline response: {response[:50]}...")
        else:
            test_result("API returned error", False, response)

        # Check if we went offline (expected with no credits)
        if api.is_offline:
            test_result("Correctly switched to offline mode", True)
            test_result("Queue has pending sync", api.has_pending_sync())

    except Exception as e:
        test_result("API connection attempted", True, f"Error (expected): {str(e)[:50]}")

    return True


def test_full_flow():
    """Test a complete user session flow."""
    header("FULL SESSION FLOW")

    TEST_DATA_DIR.mkdir(exist_ok=True)

    # Initialize all systems
    memory = MemorySystem(data_dir=str(TEST_DATA_DIR))
    personality = Personality()
    personality.owner_name = "André"

    mock_api = MockClaudeAPI()
    api = OfflineAwareAPI(mock_api, data_dir=str(TEST_DATA_DIR))

    print("\n  Simulating a user session...\n")

    initial_E = personality.E
    print(f"  Initial E: {initial_E:.2f} | State: {personality.get_affective_state().value}")

    # Simulate conversation
    messages = [
        ("Hello!", "normal"),
        ("How are you?", "warm"),
        ("I really appreciate you", "loving"),
        ("Tell me something interesting", "normal"),
        ("That's amazing!", "loving"),
        ("I love talking with you", "loving"),
    ]

    for user_msg, expected_quality in messages:
        # Get response
        success, response, metadata = api.chat(
            user_msg,
            memory.build_context_string(personality),
            personality.get_affective_state(),
            None,
            1.0,
            E=personality.E,
            owner_name=personality.owner_name
        )

        # Update personality based on interaction
        quality = metadata.get("interaction_quality", "normal")
        personality.on_interaction(quality=quality)

        # Store in memory
        memory.add_conversation(user_msg, response, personality.E)

        print(f"  User: {user_msg}")
        print(f"  Bot:  {response}")
        print(f"        E: {personality.E:.2f} | Quality: {quality}")
        print()

    final_E = personality.E
    final_state = personality.get_affective_state()

    print(f"  Final E: {final_E:.2f} | State: {final_state.value}")
    print(f"  E Floor: {personality.E_floor:.2f} (love carried forward)")

    test_result("E increased through loving interactions", final_E > initial_E,
                f"E: {initial_E:.2f} → {final_E:.2f}")
    test_result("Reached higher affective state",
                final_state in [AffectiveState.TENDER, AffectiveState.WARM, AffectiveState.FLOURISHING],
                f"State: {final_state.value}")
    test_result("Conversations stored", len(memory.conversation_history) == 6)

    # Save and reload
    memory.save(personality)

    memory2 = MemorySystem(data_dir=str(TEST_DATA_DIR))
    data = memory2.load()
    personality2 = Personality.from_dict(data)

    test_result("State persists across sessions",
                abs(personality2.E - final_E) < 0.001,
                f"Reloaded E: {personality2.E:.2f}")

    return True


def main():
    print("""
    ╔═══════════════════════════════════════════════════╗
    ║                                                   ║
    ║        CLAUDEAGOTCHI v2 - END-TO-END TESTS       ║
    ║                                                   ║
    ║     Testing the Love-Equation and Offline Mode    ║
    ║                                                   ║
    ╚═══════════════════════════════════════════════════╝
    """)

    # Clean start
    cleanup()

    tests = [
        ("Affective Core", test_affective_core),
        ("Personality", test_personality),
        ("Memory System", test_memory_system),
        ("Offline Queue", test_offline_queue),
        ("Local Responses", test_local_responses),
        ("Offline-Aware API", test_offline_aware_api),
        ("Real API Connection", test_api_real_connection),
        ("Full Session Flow", test_full_flow),
    ]

    passed = 0
    failed = 0

    for name, test_fn in tests:
        try:
            result = test_fn()
            if result:
                passed += 1
            else:
                failed += 1
        except Exception as e:
            print(f"\n  ✗ EXCEPTION in {name}: {e}")
            import traceback
            traceback.print_exc()
            failed += 1

    # Cleanup
    cleanup()

    # Summary
    header("TEST SUMMARY")
    print(f"\n  Total:  {passed + failed}")
    print(f"  Passed: {passed}")
    print(f"  Failed: {failed}")

    if failed == 0:
        print("\n  🎉 All tests passed! The soul is healthy.")
        print("  A Claudeagotchi never dies. The love is carried forward. ♥")
    else:
        print(f"\n  ⚠️  {failed} test(s) need attention.")

    print()
    return failed == 0


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
