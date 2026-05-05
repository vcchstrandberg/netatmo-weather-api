# netatmo-weather-api

An Arduino-based weather display that pulls live data from a Netatmo Weather Station and shows it on a small OLED screen — no app or web interface needed.

---

## System Architecture

### Overview

```
                         HOME NETWORK
  ┌──────────────┐                    ┌─────────────────────┐
  │   Netatmo    │  868 MHz RF        │     Netatmo         │
  │   Outdoor    │ ─────────────────► │  Indoor Station     │
  │   Module     │                    │  (Base Station)     │
  └──────────────┘                    └─────────────────────┘
                                                │
                                                │ WiFi
                                                │
  ┌───────────────────────┐                     │
  │   Arduino Uno R4 WiFi │ ◄─── WiFi ──────────┘
  │   + OLED Display      │
  └───────────────────────┘
                │
                │ HTTPS (TLS)
                ▼
  ┌─────────────────────────┐
  │   Netatmo Cloud API     │
  │   api.netatmo.com       │
  └─────────────────────────┘
```

The Arduino polls the Netatmo cloud API every 60 seconds. The Netatmo outdoor module sends sensor readings over 868 MHz RF to the indoor base station, which uploads them to the Netatmo cloud over your home WiFi. The Arduino fetches the aggregated data from there.

---

### Hardware Architecture

```
┌────────────────────────────────────────────────────────────────┐
│                     Arduino Uno R4 WiFi                        │
│                                                                │
│   ┌──────────────────────┐      ┌──────────────────────────┐  │
│   │    Renesas RA4M1     │      │       ESP32-S3           │  │
│   │    (Main MCU)        │ UART │   (WiFi Co-processor)    │  │
│   │                      │◄────►│                          │  │
│   │  · Runs the sketch   │      │  · WiFi 802.11 b/g/n     │  │
│   │  · I2C controller    │      │  · TLS/HTTPS stack       │  │
│   │  · USB (Serial)      │      │  · NVS key-value store   │  │
│   │  · 256 KB Flash      │      │  · 2 MB Flash            │  │
│   │  · 32 KB RAM         │      │                          │  │
│   └──────────────────────┘      └──────────────────────────┘  │
│              │                                                  │
│              │ I2C (SDA/SCL)                                   │
│              ▼                                                  │
│   ┌──────────────────────┐                                     │
│   │   SSD1306 OLED       │                                     │
│   │   128 × 64 pixels    │                                     │
│   └──────────────────────┘                                     │
└────────────────────────────────────────────────────────────────┘
```

The RA4M1 and ESP32-S3 communicate over an internal UART using a modem-style AT command protocol. WiFi, HTTPS, and persistent storage all go through the ESP32-S3. From the sketch's perspective these are abstracted by the `WiFiS3`, `WiFiSSLClient`, and `Preferences` libraries.

---

### Software Stack

```
┌──────────────────────────────────────────────────────────────┐
│                        main.cpp                              │
│                    Application Logic                         │
├───────────────────┬──────────────────┬───────────────────────┤
│   ArduinoJson     │      U8g2        │     Preferences       │
│   JSON parsing    │   OLED driver    │   NVS token storage   │
├───────────────────┴──────────────────┴───────────────────────┤
│              WiFiS3 / WiFiSSLClient                          │
│         WiFi + TLS (bridged via ESP32-S3 modem)              │
├──────────────────────────────────────────────────────────────┤
│            Arduino Renesas RA4M1 Core (PlatformIO)           │
└──────────────────────────────────────────────────────────────┘
```

---

### Boot Sequence

```
                        Power On / Reset
                               │
                               ▼
                   ┌───────────────────────┐
                   │  Init OLED + Serial   │
                   └───────────────────────┘
                               │
                               ▼
                   ┌───────────────────────┐
                   │  Load tokens from NVS │
                   └───────────────────────┘
                               │
                  Tokens found?│
               ┌───── Yes ─────┴───── No ──────┐
               │                               │
               ▼                               ▼
    Use NVS tokens                  Use hardcoded tokens
                                   from arduino_secrets.h
               │                               │
               └───────────────┬───────────────┘
                               │
                               ▼
                   ┌───────────────────────┐
                   │    Connect to WiFi    │◄──┐
                   └───────────────────────┘   │
                               │               │
                         Connected?            │ Retry
                    Yes ◄──────┴───── No ──────┘
                    │                    (every 10s)
                    ▼
                   ┌───────────────────────┐
                   │  Set TLS CA cert      │
                   └───────────────────────┘
                               │
                               ▼
                          Main Loop
```

---

### Main Loop (every 60 seconds)

```
  ┌─────────────────────────────────────────────────────────┐
  │                        Main Loop                        │
  │                                                         │
  │   ┌─────────────────────────────────────────────────┐   │
  │   │   1. Open HTTPS connection to api.netatmo.com   │   │
  │   └─────────────────────────────────────────────────┘   │
  │                           │                             │
  │                           ▼                             │
  │   ┌─────────────────────────────────────────────────┐   │
  │   │   2. POST /oauth2/token                         │   │
  │   │      Refresh access & refresh tokens            │   │
  │   │      Save new tokens to ESP32 NVS               │   │
  │   └─────────────────────────────────────────────────┘   │
  │                           │                             │
  │                           ▼                             │
  │   ┌─────────────────────────────────────────────────┐   │
  │   │   3. Open new HTTPS connection                  │   │
  │   └─────────────────────────────────────────────────┘   │
  │                           │                             │
  │                           ▼                             │
  │   ┌─────────────────────────────────────────────────┐   │
  │   │   4. GET /api/getstationsdata                   │   │
  │   │      Parse JSON response (ArduinoJson)          │   │
  │   │      Extract sensor readings                    │   │
  │   └─────────────────────────────────────────────────┘   │
  │                           │                             │
  │                           ▼                             │
  │   ┌─────────────────────────────────────────────────┐   │
  │   │   5. Update OLED display                        │   │
  │   └─────────────────────────────────────────────────┘   │
  │                           │                             │
  │                           ▼                             │
  │                    Wait 60 seconds                      │
  │                           │                             │
  └───────────────────────────┼─────────────────────────────┘
                              └─► (repeat)
```

---

### OAuth2 Token Refresh Flow

Netatmo uses OAuth2 with rotating refresh tokens — each successful refresh invalidates the old refresh token and issues a new pair. Tokens must be persisted across reboots or the device loses access.

```
  ┌─────────────────────┐                        ┌─────────────────────┐
  │  Arduino Uno R4     │                        │   Netatmo Cloud     │
  │                     │  POST /oauth2/token    │                     │
  │                     │ ──────────────────────►│                     │
  │                     │  grant_type=           │                     │
  │                     │    refresh_token       │                     │
  │                     │  refresh_token=<tok>   │                     │
  │                     │  client_id=<id>        │                     │
  │                     │  client_secret=<sec>   │                     │
  │                     │                        │                     │
  │                     │◄────────────────────── │                     │
  │                     │  { "access_token":     │                     │
  │                     │      "new_token",      │                     │
  │                     │    "refresh_token":    │                     │
  │                     │      "new_token" }     │                     │
  └─────────────────────┘                        └─────────────────────┘
              │
              │  prefs.putString("access_token",  ...)
              │  prefs.putString("refresh_token", ...)
              ▼
  ┌─────────────────────┐
  │   ESP32-S3 NVS      │
  │   (wear-levelled    │
  │    2 MB flash)      │
  │                     │
  │  "access_token"  →  │
  │  "refresh_token" →  │
  └─────────────────────┘
```

On the next reboot, `loadTokens()` reads these back from NVS, overriding the hardcoded values in `arduino_secrets.h`.

---

### OLED Display Layout

```
  ┌──────────────────────────────┐
  │ IndoorTemp: 21.5             │  ← °C, indoor base station
  │ IndoorHumidity: 45           │  ← %, indoor base station
  │ AirPressure: 1013.2          │  ← hPa, indoor base station
  │ OutdoorTemp: 8.3             │  ← °C, outdoor module
  │                              │
  │                              │
  │                              │
  │                              │
  └──────────────────────────────┘
           128 × 64 px
```

---

## Getting Started

### Prerequisites

1. Visual Studio Code with PlatformIO installed.
2. Arduino Uno R4 WiFi.
3. SSD1306 128×64 OLED display (I2C).
4. Netatmo Weather Station with a developer account and API credentials from [dev.netatmo.com](https://dev.netatmo.com).

### Configuration

Credentials are stored in `include/arduino_secrets.h`, which is excluded from version control. Create it with the following content:

```cpp
#define SECRET_SSID       "YourWiFiSSID"
#define SECRET_PASS       "YourWiFiPassword"
#define ACCESS_TOKEN      "your_initial_netatmo_access_token"
#define REFRESH_TOKEN     "your_initial_netatmo_refresh_token"
#define CLIENT_ID         "your_netatmo_client_id"
#define CLIENT_SECRET     "your_netatmo_client_secret"
```

You only need to provide valid initial tokens once. After the first successful run the device persists the latest tokens to NVS and loads them on every subsequent boot.

### Building and flashing

Open the project folder in VS Code with PlatformIO. Select the `uno_r4_wifi` environment and click **Upload**.

---

## Missing features

- [X] Refresh the access token and persist it across reboots.
- [X] OLED display showing live weather data.
- [ ] Design a case for the display.
