# netatmo-weather-api

An Arduino-based weather display that pulls live data from a Netatmo Weather Station and shows it on a small screen — no app or web interface needed. Supports three boards: Arduino Uno R4 WiFi (SSD1306 OLED), ESP32 DevKit (SSD1306 OLED), and Waveshare ESP32-C6 Touch LCD 1.47 (integrated 172×320 IPS TFT).

---

## Supported boards

| Board | MCU | Display | Power model |
|---|---|---|---|
| Arduino Uno R4 WiFi | Renesas RA4M1 (Cortex-M4, 48 MHz) | SSD1306 128×64 OLED (external, I2C) | Always-on, fetches every 60 s |
| ESP32 DevKit | Xtensa LX6, 240 MHz | SSD1306 128×64 OLED (external, I2C) | Always-on, fetches every 5 min |
| Waveshare ESP32-C6 Touch LCD 1.47 | ESP32-C6 (RISC-V, 160 MHz) | 172×320 IPS TFT, ST7789, integrated | Always-on, fetches every 5 min |

All boards use the `Preferences` API for NVS token storage. The Uno R4 and ESP32 DevKit use U8g2 for the OLED; the Waveshare uses LovyanGFX for the integrated TFT.

---

## Features

- **Live Netatmo data** — indoor temperature and humidity, outdoor temperature, air pressure, and rain totals (1 h and 24 h) fetched from the Netatmo Cloud API over HTTPS
- **3-card display** — indoor, outdoor, and rain cards rotate every 5 s on all boards
- **Multi-locale with unit conversion** — Svenska, English US, English UK, Français; automatically converts °C→°F, hPa→inHg, mm→in for en-US
- **Runtime locale switching** — cycle locales at any time without recompiling (button on D7 on Uno R4; BOOT button on ESP32 DevKit and Waveshare ESP32-C6)
- **Three-board support** — Arduino Uno R4 WiFi (always-on, SSD1306), ESP32 DevKit (always-on, SSD1306), Waveshare ESP32-C6 Touch LCD 1.47 (always-on, integrated 172×320 IPS TFT)
- **OAuth2 token refresh** — tokens are refreshed every cycle and written to wear-levelled NVS flash, so the device never loses API access across reboots or power cuts
- **TLS with pinned CA** — all API calls are verified against the DigiCert Global Root G2 certificate
- **Boot splash** — shows app version, build date, and git commit hash on startup
- **3D-printable enclosure** — OpenSCAD source in `enclosure/enclosure.scad`

---

## Documentation

- [Architecture](docs/architecture.md) — system overview, software stack, boot sequence, main loop, OAuth2
- [Wiring](docs/wiring.md) — pin connections and locale button for all three boards
- [Display layout](docs/display-layout.md) — card designs for OLED and TFT
- [Configuration](docs/configuration.md) — credentials, locale/units, building, flashing, serial monitor
- [Power characteristics](docs/power.md) — current draw and fetch intervals
- [Revision history](docs/revision-history.md) — version log

---

## Quick start

1. Install [PlatformIO](https://platformio.org/) — VS Code extension or `pip install platformio`.
2. Clone this repo and create your `arduino_secrets.h` for the target board — see [Configuration](docs/configuration.md).
3. Flash:
   ```bash
   pio run -e uno_r4_wifi            --target upload
   pio run -e esp32dev               --target upload
   pio run -e esp32c6_waveshare_lcd  --target upload
   ```

The first build downloads toolchains and libraries automatically. The Waveshare env downloads arduino-esp32 3.x via pioarduino (~300 MB, one-time).
