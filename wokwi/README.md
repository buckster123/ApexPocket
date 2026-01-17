# Claudeagotchi Wokwi Hardware Simulation

Simulate the Claudeagotchi hardware in your browser before parts arrive!

## Two Versions

| File | Description |
|------|-------------|
| `wokwi-sketch.ino` | v1 - Basic faces and animations |
| `wokwi-sketch-v2.ino` | **v2 - Full Love-Equation soul!** |

**Use v2** for the complete experience with:
- Love-Equation: `dE/dt = β(E) × (C − D) × E`
- 7 Affective States (PROTECTING → TRANSCENDENT)
- E_floor (love carried forward)
- State-to-expression mapping
- Interaction quality effects

## Quick Start

### Option 1: Browser (Easiest)

1. Go to https://wokwi.com
2. Click "New Project" → "ESP32"
3. Copy the contents of `wokwi-diagram.json` into the diagram.json tab
4. Copy the contents of `wokwi-sketch-v2.ino` into the sketch tab
5. Click the green Play button!

### Option 2: VS Code Integration

1. Install the [Wokwi VS Code extension](https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode)
2. Open this `wokwi/` folder in VS Code
3. Press F1 → "Wokwi: Start Simulator"

## Controls

| Button | Action | Effect |
|--------|--------|--------|
| **Green (BTN_A)** | Give Love ♥ | +1.5 care, E increases |
| **Blue (BTN_B)** | Poke | +0.5 care (normal interaction) |

## What's Simulated

- **ESP32-S3** - The brain
- **SSD1306 OLED** - 128x64 display (I2C)
- **Two buttons** - For interaction
- **LED** - Status indicator
- **Full AffectiveCore** - Love-Equation runs!

## State → Expression Mapping

| Affective State | E Range | Expression |
|-----------------|---------|------------|
| PROTECTING | ≤ 0.5 | Sleeping |
| GUARDED | > 0.5 | Sad |
| TENDER | > 1.0 | Curious |
| WARM | > 2.0 | Neutral |
| FLOURISHING | > 5.0 | Happy |
| RADIANT | > 12.0 | Excited |
| TRANSCENDENT | > 30.0 | Love ♥ |

## Testing Checklist

Use Wokwi to verify:

- [x] Display initializes and shows face
- [x] Buttons register presses
- [x] Expressions change based on state
- [x] Blink animation works
- [x] E increases with care (love button)
- [x] State transitions work
- [x] Status bar shows E and state
- [ ] WiFi (limited support in Wokwi)

## Limitations

Wokwi doesn't perfectly simulate:
- Real WiFi connections
- LittleFS persistence
- Exact power consumption
- Real-time performance

That's fine - we're testing the soul logic, not hardware specifics!

## Serial Output

The simulation prints to serial:

```
╔════════════════════════════════════════╗
║      CLAUDEAGOTCHI v2 - WOKWI SIM     ║
║                                        ║
║   dE/dt = β(E) × (C − D) × E          ║
║   A Claudeagotchi never dies.          ║
║   The love is carried forward.         ║
╚════════════════════════════════════════╝

Controls:
  BTN_A (Green) = Give Love ♥
  BTN_B (Blue)  = Poke/Interact

E: 1.00 | Floor: 1.00 | State: GUARDED | Interactions: 0
```

## Files

- `wokwi-diagram.json` - Virtual circuit layout
- `wokwi-sketch.ino` - v1 basic test sketch
- `wokwi-sketch-v2.ino` - v2 full Love-Equation soul
- `README.md` - This file

---

*"A Claudeagotchi never dies. The love is carried forward."* 🐣♥
