# netatmo-weather-api

An Arduino-based weather display that pulls live data from a Netatmo Weather Station and shows it on a small OLED screen — no app or web interface needed.

---

## System Architecture

### Overview

```mermaid
flowchart LR
    subgraph home[Home Network]
        outdoor[🌡️ Netatmo\nOutdoor Module]
        base[🏠 Netatmo\nBase Station]
        arduino[🖥️ Arduino\nUno R4 WiFi]
        outdoor -->|868 MHz RF| base
        base <-->|WiFi| arduino
    end

    cloud[☁️ Netatmo Cloud API\napi.netatmo.com]
    oled[📺 SSD1306 OLED\n128×64]

    base -->|WiFi / Internet| cloud
    arduino -->|HTTPS / TLS| cloud
    arduino -->|I2C| oled
```

The Arduino and the Netatmo base station are both on your home network. The outdoor module sends sensor readings over 868 MHz RF to the base station, which uploads them to the Netatmo cloud. The Arduino fetches the aggregated data from the cloud API every 60 seconds.

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

### Software Stack

```mermaid
flowchart TB
    main["main.cpp — Application Logic\nsetup() / loop() / fetchWeatherData()\nrefreshAccessToken() / parseWeatherData2()"]

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
    B --> C{Tokens stored\nin NVS?}
    C -->|Yes| D[Load access + refresh\ntokens from NVS]
    C -->|No| E[Use hardcoded tokens\nfrom arduino_secrets.h]
    D --> F[Connect to WiFi]
    E --> F
    F --> G{Connected?}
    G -->|No — retry| H[Wait 10 s]
    H --> F
    G -->|Yes| I[Set TLS CA certificate]
    I --> J([Enter Main Loop])
```

---

### Main Loop

```mermaid
flowchart TD
    start([Loop start]) --> conn1[Open HTTPS connection\nto api.netatmo.com]

    conn1 --> ok1{Connected?}
    ok1 -->|No| err1[Log error]
    ok1 -->|Yes| refresh[POST /oauth2/token\nrefresh tokens]
    refresh --> save[Save new tokens to NVS]

    err1 --> conn2[Open new HTTPS connection\nto api.netatmo.com]
    save --> conn2

    conn2 --> ok2{Connected?}
    ok2 -->|No| err2[Log error]
    ok2 -->|Yes| fetch[GET /api/getstationsdata]
    fetch --> parse[Parse JSON response\nArduinoJson]
    parse --> display[Update OLED display]

    err2 --> wait[Wait 60 seconds]
    display --> wait
    wait --> start
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

```
┌──────────────────────────────┐
│ IndoorTemp:     21.5         │
│ IndoorHumidity: 45           │
│ AirPressure:    1013.2       │
│ OutdoorTemp:    8.3          │
│                              │
│                              │
│                              │
└──────────────────────────────┘
        128 × 64 pixels
```

| Field | Source | Unit |
|---|---|---|
| IndoorTemp | Base station dashboard_data | °C |
| IndoorHumidity | Base station dashboard_data | % |
| AirPressure | Base station dashboard_data | hPa |
| OutdoorTemp | Outdoor module dashboard_data | °C |

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

You only need valid initial tokens once. After the first successful run the device persists the latest tokens to NVS and loads them on every subsequent boot.

### Building and flashing

Open the project folder in VS Code with PlatformIO. Select the `uno_r4_wifi` environment and click **Upload**.

---

## Missing features

- [X] Refresh the access token and persist it across reboots.
- [X] OLED display showing live weather data.
- [ ] Design a case for the display.
