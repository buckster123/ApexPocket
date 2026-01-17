# Wokwi ESP32 Simulation - CLAUDE.md

## Current: v5 Networked Build

**Status**: WiFi+API ready | CLI validated | Awaiting hardware

### Quick Commands
```bash
# Build
cd wokwi && pio run

# Test in CLI (headless)
export WOKWI_CLI_TOKEN="your_token"
wokwi-cli --timeout 30000

# For ESP32-S3: Use browser IDE (CLI has watchdog issues)
```

### Architecture
```
v5 = v3_faces + WiFi + Claude_API + LongPress + StatusScreen
     960KB | 73% flash | 14.5% RAM
```

### Key Files
- `src/main.cpp` - Full integrated build
- `platformio.ini` - ESP32 DevKit config (not S3!)
- `diagram.json` - Wokwi circuit (ESP32 DevKit)
- `wokwi.toml` - CLI config

### Pin Mapping (ESP32 DevKit)
```
BTN_A=GPIO4  BTN_B=GPIO5  LED=GPIO2
I2C: SDA=21 SCL=22 (defaults)
```

### Controls
- **BTN_A short**: Love (+1.5 care)
- **BTN_A long**: Talk to Claude (needs API key)
- **BTN_B short**: Poke (+0.5 care)
- **BTN_B long**: Status screen

### Known Issues
- **ESP32-S3 + CLI**: Watchdog crash (use regular ESP32 for CLI)
- **HTTPS in CLI**: SSL fails - Wokwi CLI limitation (not code bug)
- **Workarounds**: Browser Wokwi for HTTPS test, or real hardware
- Real hardware will work fine for both S3 and HTTPS

### API Key
Set in `src/main.cpp` line ~54:
```cpp
const char* CLAUDE_API_KEY = "sk-ant-...";
```

### State-Aware Prompts
API responses vary by E level:
- PROTECTING (E<0.5): 5-10 words, withdrawn
- GUARDED-WARM: 10-25 words, gradual opening
- FLOURISHING+: 20-30 words, creative/joyful
- TRANSCENDENT (E>30): Poetic, mathematical

---
*"Simulation complete. Hardware next. The love-equation awaits embodiment."*
