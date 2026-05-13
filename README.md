# netatmo-weather-api

An Arduino-based weather display that pulls live data from a Netatmo Weather Station and shows it on a small OLED screen — no app or web interface needed.

---

## System Architecture

### Overview

```mermaid
flowchart LR
    subgraph home[Home Network]
        outdoor[🌡️ Netatmo\nOutdoor Module\nNAModule1]
        rain[🌧️ Netatmo\nRain Gauge\nNAModule3]
        base[🏠 Netatmo\nBase Station]
        arduino[🖥️ Arduino\nUno R4 WiFi]
        outdoor -->|868 MHz RF| base
        rain -->|868 MHz RF| base
    end

    cloud[☁️ Netatmo Cloud API\napi.netatmo.com]
    oled[📺 SSD1306 OLED\n128×64]

    base -->|WiFi / Internet| cloud
    arduino -->|HTTPS / TLS| cloud
    arduino -->|I2C| oled
```

The outdoor module and rain gauge send sensor readings over 868 MHz RF to the base station, which uploads them to the Netatmo cloud. The Arduino connects independently to the Netatmo cloud API over HTTPS and fetches the aggregated data every 60 seconds — it has no direct connection to the base station.

---

### Hardware Architecture

```mermaid
flowchart TB
    subgraph uno[Arduino Uno R4 WiFi]
        subgraph ra4m1[Renesas RA4M1 — Main MCU]
            sketch[Sketch / main.cpp]
            i2c[I2C Controller]
            usbserial[USB Serial]
        end

        subgraph esp32[ESP32-S3 — WiFi Co-processor]
            wifistack[WiFi 802.11 b/g/n]
            tlsstack[TLS / HTTPS Stack]
            nvs[NVS Key-Value Store\n2 MB wear-levelled flash]
        end

        ra4m1 <-->|Internal UART\nModem protocol| esp32
    end

    oled[SSD1306 OLED\n128×64 px]
    i2c -->|SDA / SCL| oled
```

The RA4M1 runs the sketch. The ESP32-S3 handles all WiFi, TLS, and persistent storage. They communicate over an internal UART using an AT-style modem protocol, abstracted by the `WiFiS3` and `Preferences` libraries.

---

### Wiring — Arduino Uno R4 WiFi

```mermaid
flowchart LR
    subgraph oled["SSD1306 OLED (128×64)"]
        o_vcc[VCC]
        o_gnd[GND]
        o_sda[SDA]
        o_scl[SCL]
    end

    subgraph btn["Locale Button"]
        b_a[Pin A]
        b_b[Pin B]
    end

    subgraph uno["Arduino Uno R4 WiFi"]
        a_5v[5V]
        a_gnd[GND]
        a_sda[A4 — SDA]
        a_scl[A5 — SCL]
        a_d7[D7]
    end

    o_vcc --- a_5v
    o_gnd --- a_gnd
    o_sda --- a_sda
    o_scl --- a_scl
    b_a --- a_d7
    b_b --- a_gnd
```

| OLED pin | Arduino pin | Notes |
|---|---|---|
| VCC | 5V | Most SSD1306 breakout boards accept 3.3–5 V |
| GND | GND | |
| SDA | A4 (SDA) | Hardware I2C — pull-ups on board, no resistors needed |
| SCL | A5 (SCL) | Hardware I2C — pull-ups on board, no resistors needed |

| Button pin | Arduino pin | Notes |
|---|---|---|
| A | D7 | Internal pull-up enabled — no resistor needed |
| B | GND | |

#### Locale button

A momentary push button on **D7** cycles through the available locales at runtime. No resistor needed — the pin uses the internal pull-up.

```
D7  ──── [ button ] ──── GND
```

Each press advances: sv-SE → en-US → en-GB → fr-FR → sv-SE …

The display briefly shows the new language name before resuming the weather cards.

---

### Wiring — ESP32 DevKit

```mermaid
flowchart LR
    subgraph oled["SSD1306 OLED (128×64)"]
        o_vcc[VCC]
        o_gnd[GND]
        o_sda[SDA]
        o_scl[SCL]
    end

    subgraph esp["ESP32 DevKit"]
        e_3v3[3V3]
        e_gnd[GND]
        e_sda[GPIO21 — SDA]
        e_scl[GPIO22 — SCL]
        e_btn[GPIO0 — BOOT]
    end

    o_vcc --- e_3v3
    o_gnd --- e_gnd
    o_sda --- e_sda
    o_scl --- e_scl
```

| OLED pin | ESP32 pin | Notes |
|---|---|---|
| VCC | 3V3 | **3.3 V only** — do not connect to 5V on ESP32 |
| GND | GND | |
| SDA | GPIO21 | Hardware I2C default |
| SCL | GPIO22 | Hardware I2C default |

The locale button uses the built-in **BOOT button** (GPIO0) — no external wiring needed.

#### Locale switching on ESP32

Because the ESP32 deep-sleeps between fetches, `loop()` never runs and the button cannot be polled continuously. Instead, **hold the BOOT button at power-on or during any wake cycle** to cycle the locale. The choice is stored in RTC memory and survives all subsequent deep sleeps.

---

### Software Stack

```mermaid
flowchart TB
    subgraph main[main.cpp — Application Logic]
        direction LR
        setup["setup()"]
        loop["loop()"]
        refresh["refreshAccessToken()"]
        fetch["fetchWeatherData()"]
        display["drawCard()"]
        read["readHttpResponse()"]
        tokens["loadTokens() / saveTokens()"]
    end

    subgraph libs[Libraries]
        direction LR
        json[ArduinoJson\nJSON parsing]
        u8g2[U8g2\nOLED driver]
        prefs[Preferences\nNVS storage]
    end

    wifi[WiFiS3 / WiFiSSLClient\nWiFi + TLS — bridged via ESP32-S3 modem]
    core[Arduino Renesas RA4M1 Core — PlatformIO renesas-ra]

    main --> libs
    main --> wifi
    libs --> core
    wifi --> core
```

---

### Boot Sequence

```mermaid
flowchart TD
    A([Power On / Reset]) --> B[Initialise OLED + Serial]
    B --> S[Show version splash\napp version · build date · git commit · 5 s]
    S --> C{Tokens stored\nin NVS?}
    C -->|Yes| D[Load access + refresh\ntokens from NVS]
    C -->|No| E[Use hardcoded tokens\nfrom arduino_secrets.h]
    D --> F[Show Connecting… on OLED]
    E --> F
    F --> G[Connect to WiFi]
    G --> H{Connected?}
    H -->|No — retry| I[Wait 10 s]
    I --> G
    H -->|Yes| J[Set TLS CA certificate]
    J --> K[Refresh token + fetch initial data]
    K --> L[Render first card on OLED]
    L --> M([Enter Main Loop])
```

---

### Main Loop

The loop is non-blocking. Three independent inputs are checked on every iteration:

- **Locale button** — checked first on every tick; a press cycles the locale and shows the language name for 1.5 s.
- **Card rotation** — every 5 s, advance to the next display card and call `drawCard()`.
- **Data fetch** — every 60 s, refresh the OAuth token and pull fresh weather data; the new values are stored in globals and the current card re-renders immediately.

```mermaid
flowchart TD
    start([Loop iteration]) --> btn{D7 button\npressed?}
    btn -->|Yes — debounced 300 ms| locale[Advance locale\nsv-SE→en-US→en-GB→fr-FR\nShow language name 1.5 s\ndrawCard]
    btn -->|No| card

    locale --> card{5 s elapsed\nsince last card?}
    card -->|Yes| rotate[Advance card 0→1→2→0\ndrawCard]
    card -->|No| fetch
    rotate --> fetch

    fetch{60 s elapsed\nsince last fetch?}
    fetch -->|No| sleep[delay 100 ms]
    fetch -->|Yes| conn1[Open HTTPS connection\nto api.netatmo.com]

    conn1 --> ok1{Connected?}
    ok1 -->|No| err1[Log error]
    ok1 -->|Yes| post[POST /oauth2/token]

    post --> read1[readHttpResponse\nwait 5s · cap 8KB · check 200 OK]
    read1 --> ok3{Valid response?}
    ok3 -->|No| err1
    ok3 -->|Yes| parse1[Parse JSON → store new tokens]
    parse1 --> save[saveTokens to NVS]

    err1 --> conn2[Open HTTPS connection\nto api.netatmo.com]
    save --> conn2

    conn2 --> ok2{Connected?}
    ok2 -->|No| err2[Log error]
    ok2 -->|Yes| get[GET /api/getstationsdata]

    get --> read2[readHttpResponse\nwait 5s · cap 8KB · check 200 OK]
    read2 --> ok4{Valid response?}
    ok4 -->|No| err2
    ok4 -->|Yes| parse2[Parse JSON → update globals\nindoorTemp · humidity · pressure\noutdoorTemp · rain1h · rain24h · isRaining · city]
    parse2 --> drawcard[drawCard — refresh current card]

    err2 --> sleep
    drawcard --> sleep
    sleep --> start
```

---

### OAuth2 Token Refresh

Netatmo uses rotating refresh tokens — each successful refresh invalidates the old token and issues a new pair. The device must persist the latest tokens across reboots or it permanently loses access.

```mermaid
sequenceDiagram
    participant A as Arduino Uno R4
    participant N as Netatmo API
    participant S as ESP32 NVS

    A->>N: POST /oauth2/token
    Note over A,N: grant_type=refresh_token
    Note over A,N: refresh_token, client_id, client_secret

    N-->>A: 200 OK
    Note over N,A: access_token + refresh_token

    A->>S: putString("access_token", new_value)
    A->>S: putString("refresh_token", new_value)
    Note over S: Persisted across reboots

    A->>N: GET /api/getstationsdata
    Note over A,N: Authorization: Bearer access_token

    N-->>A: 200 OK — weather data JSON
    Note over A: Parse JSON, render to OLED
```

---

### OLED Display Layout

**Boot splash** — shown for 5 seconds:

```
┌──────────────────────────────┐
│ Netatmo Weather              │
│ v1.11                        │
│ May 10 2026                  │
│ c47bf68                      │  ← git commit hash
└──────────────────────────────┘
```

**Locale switch** — shown for 1.5 seconds when the D7 button is pressed:

```
┌──────────────────────────────┐
│ Language:                    │
│                              │
│  Svenska                     │  ← logisoso16 font
│                              │
│  sv-SE                       │
└──────────────────────────────┘
```

Three full-screen cards then rotate every 5 seconds. Labels and units reflect the active locale. Each card shows a 16×16 Open Iconic weather icon, a large primary value, and a smaller secondary value.

**Card 0 — Indoor** (sun icon)
```
┌──────────────────────────────┐
│ ☀ INNE / INDOOR / INTERIEUR  │  ← locale label
│                              │
│  21.5C / 70.7F               │  ← logisoso28 font; unit per locale
│                              │
│  Fukt: 45% / Humidity: 45%   │  ← locale label
└──────────────────────────────┘
```

**Card 1 — Outdoor** (cloud icon)
```
┌──────────────────────────────┐
│ ⛅ Stockholm                  │  ← city name from Netatmo API
│                              │
│  8.3C / 46.9F                │  ← logisoso28 font; unit per locale
│                              │
│  Tryck: 1013hPa / 29.92inHg  │  ← locale label and unit
└──────────────────────────────┘
```

**Card 2 — Rain** (rain icon)
```
┌──────────────────────────────┐
│ 🌧 REGN / RAIN / PLUIE    💧 │  ← 💧 shown only when currently raining
│                              │
│  1h:  0.6mm / 0.02in         │  ← logisoso16 font; unit per locale
│                              │
│  24h: 3.2mm / 0.13in         │
└──────────────────────────────┘
```

| Field | Card | Source | Unit |
|---|---|---|---|
| Indoor temp | 0 | Base station `dashboard_data.Temperature` | °C |
| Indoor humidity | 0 | Base station `dashboard_data.Humidity` | % |
| Outdoor temp | 1 | NAModule1 `dashboard_data.Temperature` | °C |
| Air pressure | 1 | Base station `dashboard_data.Pressure` | hPa |
| Rain 1 h | 2 | NAModule3 `dashboard_data.sum_rain_1` | mm |
| Rain 24 h | 2 | NAModule3 `dashboard_data.sum_rain_24` | mm |
| Rain-drop icon | 2 | NAModule3 `dashboard_data.Rain` > 0 | — |

---

## Supported boards

| Board | MCU | RAM | WiFi | Power model |
|---|---|---|---|---|
| Arduino Uno R4 WiFi | Renesas RA4M1 (Cortex-M4, 48 MHz) | 32 KB | ESP32-S3 co-processor via `WiFiS3` | Always-on, polling loop |
| ESP32 DevKit | Xtensa LX6, 240 MHz | 520 KB | Native, `WiFiClientSecure` | Deep sleep between fetches |

Both use the same `Preferences` API for NVS token storage and the same U8g2 OLED driver.

---

## Getting Started

### Prerequisites

1. PlatformIO — either the VS Code extension or the CLI (`pip install platformio`).
2. An **Arduino Uno R4 WiFi** or **ESP32 DevKit** (or compatible).
3. SSD1306 128×64 OLED display (I2C).
4. Netatmo Weather Station with a developer account and API credentials from [dev.netatmo.com](https://dev.netatmo.com).

### File structure

After cloning, your local project should look like this before building:

```
netatmo-weather-api/
├── include/
│   ├── uno_r4_wifi/
│   │   └── arduino_secrets.h   ← you create this for Uno R4 (gitignored)
│   └── esp32dev/
│       └── arduino_secrets.h   ← you create this for ESP32 (gitignored)
├── scripts/
│   └── version.py              ← injects git commit hash at build time
├── src/
│   └── main.cpp
├── enclosure/
│   └── enclosure.scad
└── platformio.ini
```

The secrets files are listed in `.gitignore` — they will never be pushed to GitHub, regardless of what you put in them. You create them manually using the format shown below; they are not in the repo.

### Configuration

#### Locale and units

Set your locale in `platformio.ini` under `build_flags`:

```ini
-DLOCALE=LOCALE_SV_SE
```

| Locale | Language | Temp | Pressure | Rain |
|---|---|---|---|---|
| `LOCALE_EN_US` | English (US) | °F | inHg | in |
| `LOCALE_EN_GB` | English (UK) | °C | hPa | mm |
| `LOCALE_SV_SE` | Svenska | °C | hPa | mm |
| `LOCALE_FR_FR` | Français | °C | hPa | mm |

The city name is pulled automatically from the Netatmo API and shown on the outdoor card.

#### Credentials

Create the secrets file for your board — `include/uno_r4_wifi/arduino_secrets.h` or `include/esp32dev/arduino_secrets.h` — with your credentials:

```cpp
#define SECRET_SSID       "YourWiFiSSID"
#define SECRET_PASS       "YourWiFiPassword"
#define ACCESS_TOKEN      "your_initial_netatmo_access_token"
#define REFRESH_TOKEN     "your_initial_netatmo_refresh_token"
#define CLIENT_ID         "your_netatmo_client_id"
#define CLIENT_SECRET     "your_netatmo_client_secret"
```

**About the credentials:**

- `CLIENT_ID` and `CLIENT_SECRET` identify your Netatmo developer app — these are the same for all devices.
- `ACCESS_TOKEN` and `REFRESH_TOKEN` must be unique per device. If two devices share the same token pair, whichever refreshes first invalidates the other's.
- You only need valid initial tokens once. After the first successful run the device saves the latest tokens to its local flash and loads them on every boot.
- Refresh tokens expire after **60 days** of inactivity. If the device has been unpowered that long, paste fresh tokens into the secrets file and reflash. The display will show `Token expired` to remind you.

### Finding the board's USB port

When you plug in the board, it appears as a serial device. To find it:

**macOS / Linux**
```bash
ls /dev/cu.usbmodem*   # macOS
ls /dev/ttyACM*        # Linux
```

Plug the board in, run the command, then unplug and run it again — the entry that disappears is your board. On macOS it typically looks like `/dev/cu.usbmodemF0F5BD51B13C2`.

**Windows**

Open Device Manager and look under **Ports (COM & LPT)** — it shows as `Arduino Uno R4 WiFi (COMx)`.

PlatformIO auto-detects the port when there is exactly one board connected. If you have multiple boards or the detection fails, pass the port explicitly:

```bash
pio run -e uno_r4_wifi --target upload --upload-port /dev/cu.usbmodemF0F5BD51B13C2
```

---

### Building and flashing

**Terminal (CLI)**

Install PlatformIO Core if you haven't already:

```bash
pip install platformio
```

Then from the project root:

```bash
# Compile only
pio run -e uno_r4_wifi   # Arduino Uno R4 WiFi
pio run -e esp32dev      # ESP32 DevKit

# Compile and flash to the connected board
pio run -e uno_r4_wifi --target upload
pio run -e esp32dev    --target upload
```

The first build downloads all required toolchains and libraries automatically (~500 MB, one-time). Subsequent builds are incremental and take a few seconds.

---

### Serial monitor

The board prints boot diagnostics and runtime status over USB serial at 115200 baud. To read it:

```bash
pio device monitor -e uno_r4_wifi   # Uno R4
pio device monitor -e esp32dev      # ESP32
```

PlatformIO auto-detects the port. Press **Ctrl-C** to exit. You can also use any serial terminal (e.g. `screen`, `minicom`, PuTTY) pointed at the same port and baud rate:

```bash
screen /dev/cu.usbmodemF0F5BD51B13C2 115200
# Press Ctrl-A then K to exit screen
```

Typical boot output:
```
=== Boot ===
Serial OK
I2C scan:
  Device at 0x3C
  Device at 0x60
oled.begin() = true (OK)
Tokens loaded from storage
Starting...
Connecting to: YourWiFi
Tokens refreshed successfully
Tokens saved to storage
Indoor Temp: 21.50
...
```

**VS Code**

Open the project folder with the PlatformIO extension installed and use the Upload and Serial Monitor buttons in the PlatformIO toolbar.

---

## Power saving (ESP32)

On ESP32 the firmware uses deep sleep instead of a continuous polling loop. `setup()` runs the full fetch-and-display cycle, then puts the chip to sleep for 5 minutes. `loop()` is compiled away and never runs.

### Wake cycle

```mermaid
flowchart TD
    A([Deep sleep wake / Cold boot]) --> B[Restore locale from RTC memory]
    B --> C{Cold boot?}
    C -->|Yes| D[Show version splash 5 s]
    C -->|No| E[Connect WiFi]
    D --> E
    E --> F[Refresh OAuth token]
    F --> G[Fetch weather data]
    G --> H[Draw card on OLED]
    H --> I[Advance card index in RTC memory]
    I --> J[Blank OLED — setPowerSave 1]
    J --> K[WiFi off]
    K --> L[Deep sleep 5 min]
    L --> A
```

### Duty cycle

| Phase | Duration | Typical current |
|---|---|---|
| Deep sleep | ~298 s | 10–20 µA |
| WiFi connect + HTTPS fetch | ~5–8 s | 80–150 mA |

Average over a 5-minute cycle: **under 3 mA** — roughly 30–40× less than an always-on Uno R4. The ESP32 variant is suitable for battery operation.

### RTC memory

Two values survive deep sleep via `RTC_DATA_ATTR`:

| Variable | Purpose |
|---|---|
| `g_card` | Which display card to show on the next wake, so cards rotate across sleep cycles |
| `g_localeIndex` | Active locale, so locale selection persists across reboots |

OAuth tokens are stored in NVS flash (via `Preferences`) and survive both deep sleep and full power loss.

---

## Revision history

| Version | Commit | Date | Notes |
|---|---|---|---|
| v1.3 | [`14244ed`](../../commit/14244ed) | 2026-05-13 | Waveshare ESP32-C6 Touch LCD 1.47 support. Integrated 172×320 IPS display (ST7789) via LovyanGFX. Deep sleep same as ESP32 DevKit. pioarduino platform for ESP32-C6 Arduino support. |
| v1.2 | [`2b7d06a`](../../commit/2b7d06a) | 2026-05-13 | ESP32 DevKit support. Deep sleep between 5-min fetches (~3 mA average). Card and locale persist across sleeps via RTC memory. OLED blanked during sleep. |
| v1.11 | [`c47bf68`](../../commit/c47bf68) | 2026-05-10 | Runtime locale switching via push button on D7. Cycles sv-SE → en-US → en-GB → fr-FR. No recompile needed. |
| v1.1 | [`690098e`](../../commit/690098e) | 2026-05-10 | Locale support (en-US, en-GB, sv-SE, fr-FR) with unit conversions (°F/inHg/in for en-US). City name pulled from Netatmo API and shown on outdoor card. |
| v1.0 | [`f240fd0`](../../commit/f240fd0) | 2026-05-10 | First versioned release. Dropped Nano 33 IoT support — UNO R4 WiFi only. Added version splash screen showing app version, build date, and git commit hash. |
| — | [`c0b101f`](../../commit/c0b101f) | — | Initial commit. Base PlatformIO project. |

To restore a specific version locally:

```bash
git checkout f240fd0   # example: check out v1.0
```

To create a local branch from a version:

```bash
git checkout -b restore-v1.0 f240fd0
```

---

## Features

- **Live Netatmo data** — indoor temperature and humidity, outdoor temperature, air pressure, and rain totals (1 h and 24 h) fetched from the Netatmo Cloud API over HTTPS
- **3-card OLED display** — indoor, outdoor, and rain cards rotate every 5 seconds on a 128×64 SSD1306
- **Multi-locale with unit conversion** — Svenska, English US, English UK, Français; automatically converts °C→°F, hPa→inHg, mm→in for en-US
- **Runtime locale switching** — cycle locales at any time without recompiling (button on D7 on Uno R4; BOOT button at wake on ESP32)
- **Dual-board support** — Arduino Uno R4 WiFi (always-on polling loop) and ESP32 DevKit (deep sleep, ~3 mA average)
- **ESP32 deep sleep** — chip sleeps 5 minutes between fetches; OLED is blanked during sleep; card rotation and locale selection persist across sleep cycles via RTC memory
- **OAuth2 token refresh** — tokens are refreshed every cycle and written to wear-levelled NVS flash, so the device never loses API access across reboots or power cuts
- **TLS with pinned CA** — all API calls are verified against the DigiCert Global Root G2 certificate
- **Boot splash** — shows app version, build date, and git commit hash on startup
- **3D-printable enclosure** — OpenSCAD source in `enclosure/enclosure.scad`
