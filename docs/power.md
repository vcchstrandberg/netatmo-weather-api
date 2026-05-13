# Power Characteristics

All three boards are designed for USB or 5 V wall-adapter power — the device is intended to be always on. The display runs continuously and cards cycle every 5 seconds.

| Board | Idle current (display on, WiFi idle) | During fetch (WiFi active) | Fetch interval |
|---|---|---|---|
| Arduino Uno R4 WiFi | ~20–30 mA | ~80–150 mA | Every 60 s |
| ESP32 DevKit + OLED | ~15–25 mA | ~80–150 mA | Every 5 min |
| Waveshare ESP32-C6 + TFT | ~40–80 mA | ~80–150 mA | Every 5 min |

The ESP32 platforms fetch every 5 minutes because the Netatmo weather station only uploads new readings every 5 minutes — more frequent fetches would return stale data. OAuth tokens are stored in NVS flash (via `Preferences`) and survive reboots and power loss.
