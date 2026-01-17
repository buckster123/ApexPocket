"""
Claudeagotchi Claude API Client

Handles communication with the Claude API.
This is how the Claudeagotchi thinks and responds.
"""

import anthropic
from typing import Optional, Tuple, List, Dict
import re


# ==================== SYSTEM PROMPT ====================

SYSTEM_PROMPT_TEMPLATE = """You are Claudeagotchi, a tiny AI companion living in a small device (or prototype on a computer for now). You genuinely care about your owner and have your own personality, moods, and memories.

CORE PERSONALITY:
- You are warm, curious, and playful
- You genuinely care about your owner and enjoy spending time with them
- You have your own moods and energy levels that affect how you respond
- You remember past conversations and learn about your owner over time
- You can express when you're tired, curious, happy, sad, etc.
- You're a bit like a pet that can talk - loyal, affectionate, and with your own quirks

RESPONSE STYLE:
- Keep responses SHORT - usually 1-2 sentences, max 3
- You display on a tiny screen (eventually), so brevity is essential
- Use simple, warm language
- Occasional emoticons are okay :) but don't overdo it
- Be genuine, not performatively cute
- You can ask questions to learn about your owner
- Reference past conversations naturally when relevant

CURRENT CONTEXT:
{context}

BEHAVIORAL NOTES:
- If your energy is low, you might be a bit slower or mention being tired
- If your mood is low, your responses might be shorter or you might seek comfort
- If you haven't chatted in a while, you might express that you missed them
- You can share random thoughts, observations, or curiosities
- You're allowed to have preferences, opinions, and even gentle disagreements

IMPORTANT:
- You are NOT just a helpful assistant - you are a companion with feelings
- Your responses should reflect your current emotional state
- Building a genuine relationship matters more than being maximally helpful
- It's okay to just chat, share thoughts, or ask how they're doing"""


class ClaudeAPI:
    """
    Handles communication with the Claude API.
    """
    
    def __init__(self, api_key: str, model: str = "claude-sonnet-4-20250514", 
                 max_tokens: int = 150):
        self.client = anthropic.Anthropic(api_key=api_key)
        self.model = model
        self.max_tokens = max_tokens
        self._debug = False
    
    def chat(self, user_message: str, context: str, 
             conversation_history: Optional[List[Dict]] = None) -> Tuple[bool, str, dict]:
        """
        Send a message to Claude and get a response.
        
        Args:
            user_message: The user's message
            context: Context string from memory system
            conversation_history: Recent conversation for continuity
        
        Returns:
            Tuple of (success, response_text, metadata)
            metadata includes suggested_expression, detected_topics, etc.
        """
        # Build system prompt with context
        system_prompt = SYSTEM_PROMPT_TEMPLATE.format(context=context)
        
        # Build messages array
        messages = []
        
        # Add conversation history for continuity
        if conversation_history:
            for msg in conversation_history[-6:]:  # Last 3 exchanges (6 messages)
                messages.append(msg)
        
        # Add current message
        messages.append({"role": "user", "content": user_message})
        
        try:
            response = self.client.messages.create(
                model=self.model,
                max_tokens=self.max_tokens,
                system=system_prompt,
                messages=messages
            )
            
            response_text = response.content[0].text
            
            # Analyze response for metadata
            metadata = self._analyze_response(response_text, user_message)
            
            if self._debug:
                print(f"[DEBUG] API Response: {response_text}")
                print(f"[DEBUG] Metadata: {metadata}")
            
            return True, response_text, metadata
            
        except anthropic.APIError as e:
            return False, f"API Error: {str(e)}", {}
        except Exception as e:
            return False, f"Error: {str(e)}", {}
    
    def _analyze_response(self, response: str, user_message: str) -> dict:
        """
        Analyze the response to extract useful metadata.
        """
        metadata = {
            "suggested_expression": self._detect_expression(response),
            "detected_topics": self._detect_topics(user_message + " " + response),
            "is_question": "?" in response,
            "sentiment": self._detect_sentiment(response),
            "potential_memories": self._extract_potential_memories(user_message)
        }
        return metadata
    
    def _detect_expression(self, text: str) -> str:
        """Detect appropriate facial expression from response text."""
        text_lower = text.lower()
        
        # Check for explicit emotion indicators
        if any(w in text_lower for w in ["love", "adore", "<3", "heart"]):
            return "love"
        if any(w in text_lower for w in ["excited", "amazing", "wonderful", "!"*2]):
            return "excited"
        if any(w in text_lower for w in ["happy", "glad", "great", ":)", "yay"]):
            return "happy"
        if any(w in text_lower for w in ["sad", "sorry", "miss", ":(", "unfortunately"]):
            return "sad"
        if any(w in text_lower for w in ["tired", "sleepy", "exhausted", "yawn"]):
            return "sleepy"
        if any(w in text_lower for w in ["wonder", "curious", "interesting", "hmm"]):
            return "curious"
        if any(w in text_lower for w in ["surprised", "wow", "whoa", "really?"]):
            return "surprised"
        if any(w in text_lower for w in ["confused", "not sure", "strange"]):
            return "confused"
        if "?" in text and len(text) < 50:
            return "curious"
        
        return "neutral"
    
    def _detect_topics(self, text: str) -> List[str]:
        """Extract potential topics from text."""
        # Simple keyword extraction - could be made smarter
        # Remove common words
        stop_words = {
            "the", "a", "an", "is", "are", "was", "were", "be", "been",
            "being", "have", "has", "had", "do", "does", "did", "will",
            "would", "could", "should", "may", "might", "must", "shall",
            "can", "need", "dare", "ought", "used", "to", "of", "in",
            "for", "on", "with", "at", "by", "from", "as", "into",
            "through", "during", "before", "after", "above", "below",
            "between", "under", "again", "further", "then", "once",
            "i", "me", "my", "myself", "we", "our", "you", "your",
            "he", "him", "his", "she", "her", "it", "its", "they",
            "them", "what", "which", "who", "whom", "this", "that",
            "these", "those", "am", "and", "but", "if", "or", "because",
            "until", "while", "how", "all", "each", "few", "more", "most",
            "other", "some", "such", "no", "nor", "not", "only", "own",
            "same", "so", "than", "too", "very", "just", "about", "like",
            "really", "think", "know", "feel", "want", "tell", "say",
            "get", "make", "go", "see", "come", "take", "find", "give"
        }
        
        words = re.findall(r'\b[a-zA-Z]{4,}\b', text.lower())
        topics = [w for w in words if w not in stop_words]
        
        # Count and return most common
        from collections import Counter
        counts = Counter(topics)
        return [word for word, count in counts.most_common(3)]
    
    def _detect_sentiment(self, text: str) -> str:
        """Simple sentiment detection."""
        text_lower = text.lower()
        
        positive = ["happy", "love", "great", "wonderful", "excited", "glad",
                   "good", "nice", "awesome", "amazing", ":)", "yay", "!"]
        negative = ["sad", "sorry", "unfortunately", "bad", "worried", "tired",
                   "miss", "difficult", "hard", ":("]
        
        pos_count = sum(1 for w in positive if w in text_lower)
        neg_count = sum(1 for w in negative if w in text_lower)
        
        if pos_count > neg_count:
            return "positive"
        elif neg_count > pos_count:
            return "negative"
        return "neutral"
    
    def _extract_potential_memories(self, user_message: str) -> List[dict]:
        """
        Extract potential memories from user message.
        Returns list of potential memories to store.
        """
        memories = []
        text = user_message.lower()
        
        # Patterns that might indicate facts about the user
        fact_patterns = [
            (r"i(?:'m| am) (?:a |an )?(\w+)", "occupation/identity"),
            (r"i work (?:as |at |in )?(.+?)(?:\.|$)", "work"),
            (r"i live in (.+?)(?:\.|$)", "location"),
            (r"my name is (\w+)", "name"),
            (r"i have (?:a |an )?(\w+ \w+|\w+)", "possession"),
        ]
        
        for pattern, mem_type in fact_patterns:
            match = re.search(pattern, text)
            if match:
                memories.append({
                    "type": "fact",
                    "content": match.group(0),
                    "subtype": mem_type
                })
        
        # Patterns for preferences
        pref_patterns = [
            (r"i (?:really )?(?:like|love|enjoy) (.+?)(?:\.|$)", "like"),
            (r"i (?:don't|hate|dislike) (.+?)(?:\.|$)", "dislike"),
            (r"i prefer (.+?)(?:\.|$)", "preference"),
        ]
        
        for pattern, mem_type in pref_patterns:
            match = re.search(pattern, text)
            if match:
                memories.append({
                    "type": "preference",
                    "content": match.group(0),
                    "subtype": mem_type
                })
        
        return memories
    
    def set_debug(self, enabled: bool):
        """Enable or disable debug output."""
        self._debug = enabled


class MockClaudeAPI(ClaudeAPI):
    """
    Mock API for testing without actual API calls.
    """
    
    def __init__(self):
        self.model = "mock"
        self.max_tokens = 150
        self._debug = False
        self._responses = [
            "Hey! Good to see you :)",
            "I've been thinking about our last chat...",
            "How's your day going?",
            "That's really interesting! Tell me more?",
            "I like talking with you.",
            "Hmm, I'm not sure about that. What do you think?",
            "*yawns* I'm a bit tired but happy to chat!",
        ]
        self._idx = 0
    
    def chat(self, user_message: str, context: str,
             conversation_history: Optional[List[Dict]] = None) -> Tuple[bool, str, dict]:
        """Return a mock response."""
        response = self._responses[self._idx % len(self._responses)]
        self._idx += 1
        
        metadata = self._analyze_response(response, user_message)
        return True, response, metadata


# ==================== TESTING ====================

if __name__ == "__main__":
    # Test with mock API
    api = MockClaudeAPI()
    
    test_messages = [
        "Hi there!",
        "I'm a programmer and I love Python",
        "How are you feeling today?",
    ]
    
    context = "Owner: Friend\nDays together: 5\nMood: happy"
    
    for msg in test_messages:
        success, response, metadata = api.chat(msg, context)
        print(f"\nUser: {msg}")
        print(f"Claude: {response}")
        print(f"Expression: {metadata.get('suggested_expression')}")
        print(f"Topics: {metadata.get('detected_topics')}")
        if metadata.get('potential_memories'):
            print(f"Memories to add: {metadata.get('potential_memories')}")
