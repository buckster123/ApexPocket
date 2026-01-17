# Wokwi ESP32 Simulation - CLAUDE.md

## Current: v5 Networked Build

**Status**: WiFi+API ready, tested in CLI w/ regular ESP32

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
- ESP32-S3 + Wokwi CLI + PlatformIO = watchdog crash
- Workaround: Use regular ESP32 for CLI, browser for S3
- Real S3 hardware will work fine

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
*"WiFi works. Claude awaits. Hardware incoming."*
