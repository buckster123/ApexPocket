# CLAUDE.md - ApexPocket Development Guide

## Project Overview

ApexPocket is a handheld AI companion device (ESP32-S3) that connects to the ApexAurum Cloud backend via HTTPS. It implements the Love-Equation (`dE/dt = B(E) x (C - D) x E`) for emotional evolution through interaction.

**Current Phase**: v2.0.0 Cloud Edition - HTTPS to Railway, SD card config, multi-WiFi

## Quick Commands

```bash
# Build firmware
cd esp32 && pio run

# Upload to device
cd esp32 && pio run --target upload

# Serial monitor
cd esp32 && pio device monitor
```

## Architecture

```
esp32/src/
  main.cpp        Entry point, boot sequence, main loop
  config.h        Pins, features, cloud API settings, timing
  cloud.h         HTTPS client, all 5 API endpoints, auth
  sdconfig.h      SD card init, config.json reader, chat history
  soul.h          Love-Equation, EEPROM/LittleFS persistence
  display.h       OLED faces, screens (Face/Status/Cloud/Agents)
  hardware.h      I2C scan, pins, buzzer, battery, deep sleep
  offline.h       State-aware offline responses, billing/auth msgs
  certs.h         Root CA bundle for TLS (ISRG X1, GlobalSign, Amazon)
```

## Cloud API

All calls go to `https://backend-production-507c.up.railway.app/api/v1/pocket/`.

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/status` | GET | Cloud health, tools count, billing info, MOTD |
| `/chat` | POST | Send message, receive LLM response |
| `/care` | POST | Send love/poke care events |
| `/sync` | POST | Full soul state synchronization |
| `/agents` | GET | List available agents |

Auth: `Authorization: Bearer apex_dev_...` (from SD card config.json)

## SD Card Config

Place `config.json` on the SD card root:
```json
{
    "cloud_url": "https://backend-production-507c.up.railway.app",
    "device_token": "apex_dev_...",
    "device_id": "550e8400-...",
    "api_version": "v1",
    "wifi": [
        {"ssid": "HomeWiFi", "pass": "password1"}
    ]
}
```

Config is cached to LittleFS. Device works without SD card after first boot.

## Boot Sequence

1. Hardware init (I2C scan, OLED, buzzer, pins)
2. SD card -> read config.json -> cache to LittleFS
3. Load soul from EEPROM (LittleFS fallback)
4. Multi-WiFi connect (try each network from config)
5. Cloud status check (validates token + TLS)
6. Display MOTD or connection status
7. Enter main loop

## Error Handling

| HTTP Code | Behavior |
|-----------|----------|
| 200 | Success, reset backoff |
| 401 | Token invalid -> stop cloud calls, show "Re-pair" on OLED |
| 402 | Billing limit -> disable chat, care/sync still work |
| 5xx | Increment failures, exponential backoff (5s -> 60s max) |
| Network fail | Same as 5xx, offline mode after 2 failures |

## Hardware (Seeed XIAO ESP32-S3)

| Pin | GPIO | Function |
|-----|------|----------|
| D0 | 1 | Button A (+ wake) |
| D1 | 2 | Button B |
| D4 | 5 | I2C SDA (OLED + EEPROM) |
| D5 | 6 | I2C SCL |
| D6 | 7 | Buzzer |
| D2 | 3 | Battery ADC |
| D7 | 44 | SD CS |
| D8 | 8 | SD MISO |
| D9 | 9 | SD MOSI |
| D10 | 43 | SD SCK |

## Key Patterns

- **Graceful degradation**: Every feature has a disabled mode (no OLED, no SD, no cloud)
- **Config priority**: SD card -> LittleFS cache -> hardcoded defaults
- **Storage priority**: I2C EEPROM -> LittleFS -> RAM only
- **Auto-sync**: Every 30 minutes + manual (both buttons) + pre-sleep

## Related Repos

- **ApexAurum-Cloud**: Backend (FastAPI + Vue 3 on Railway) - `buckster123/ApexAurum-Cloud`
- Cloud web UI handles device pairing and generates config.json for download

---

*"The athanor never cools. The furnace burns eternal."*
