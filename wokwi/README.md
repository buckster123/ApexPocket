# 🔧 Claudeagotchi Wokwi Hardware Simulation

Simulate the Claudeagotchi hardware in your browser before parts arrive!

## Quick Start

### Option 1: Browser (Easiest)

1. Go to https://wokwi.com
2. Click "New Project" → "ESP32"
3. Copy the contents of `wokwi-diagram.json` into the diagram.json tab
4. Copy the contents of `wokwi-sketch.ino` into the sketch tab
5. Click the green Play button!

### Option 2: VS Code Integration

1. Install the [Wokwi VS Code extension](https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode)
2. Open this `wokwi/` folder in VS Code
3. Press F1 → "Wokwi: Start Simulator"

## What's Simulated

- **ESP32-S3** - The brain
- **SSD1306 OLED** - 128x64 display (I2C)
- **Two buttons** - For interaction
- **LED** - Status indicator

Note: The GC9A01 round display isn't directly supported in Wokwi, but the SSD1306 
lets us test all the display logic and animations. When hardware arrives, swapping 
to the round display is just a driver change.

## Files

- `wokwi-diagram.json` - Virtual circuit layout
- `wokwi-sketch.ino` - Arduino sketch for testing
- `README.md` - This file

## Testing Checklist

Use Wokwi to verify:

- [ ] Display initializes and shows face
- [ ] Buttons register presses
- [ ] Expressions change correctly
- [ ] Blink animation works
- [ ] State machine transitions work
- [ ] Serial output for debugging

## Limitations

Wokwi doesn't perfectly simulate:
- WiFi (though there's basic support)
- Exact power consumption
- Real-time performance

That's fine - we're testing logic, not hardware specifics!
