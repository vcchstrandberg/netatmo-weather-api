# Architecture

## System Overview

```mermaid
flowchart LR
    subgraph home[Home Network]
        outdoor[🌡️ Netatmo\nOutdoor Module\nNAModule1]
        rain[🌧️ Netatmo\nRain Gauge\nNAModule3]
        base[🏠 Netatmo\nBase Station]
        device[🖥️ Arduino / ESP32\nWeather Display]
        outdoor -->|868 MHz RF| base
        rain -->|868 MHz RF| base
    end

    cloud[☁️ Netatmo Cloud API\napi.netatmo.com]
    display[📺 Display\nOLED or TFT]

    base -->|WiFi / Internet| cloud
    device -->|HTTPS / TLS| cloud
    device -->|I2C or SPI| display
```

The outdoor module and rain gauge send sensor readings over 868 MHz RF to the base station, which uploads them to the Netatmo cloud. The display device connects independently to the Netatmo cloud API over HTTPS and fetches the aggregated data — it has no direct connection to the base station.

---

## Hardware Architecture

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

## Software Stack

```mermaid
flowchart TB
    subgraph main[main.cpp — Application Logic]
        direction LR
        setup["setup()"]
        loop["loop() — all boards"]
        refresh["refreshAccessToken()"]
        fetch["fetchWeatherData()"]
        display["drawCard()"]
        read["readHttpResponse()"]
        tokens["loadTokens() / saveTokens()"]
    end

    subgraph libs[Libraries — selected by platform]
        direction LR
        json[ArduinoJson\nJSON parsing]
        u8g2[U8g2\nSSD1306 OLED — Uno R4 + ESP32 DevKit]
        lgfx[LovyanGFX\nST7789 TFT — Waveshare ESP32-C6]
        prefs[Preferences\nNVS storage]
    end

    wifi["WiFiS3/WiFiSSLClient — Uno R4\nWiFiClientSecure — ESP32 / ESP32-C6"]
    core["renesas-ra core — Uno R4\nespressif32 — ESP32 DevKit\npioarduino espressif32 — ESP32-C6"]

    main --> libs
    main --> wifi
    libs --> core
    wifi --> core
```

---

## Boot Sequence

```mermaid
flowchart TD
    A([Power On / Reset]) --> B[Initialise display + Serial\nOLED or TFT depending on board]
    B --> S[Show version splash\napp version · build date · git commit · 5 s]
    S --> C{Tokens stored\nin NVS?}
    C -->|Yes| D[Load access + refresh\ntokens from NVS]
    C -->|No| E[Use hardcoded tokens\nfrom arduino_secrets.h]
    D --> F[Show Connecting…]
    E --> F
    F --> G[Connect to WiFi]
    G --> H{Connected?}
    H -->|No — retry| I[Wait 0.5–10 s]
    I --> G
    H -->|Yes| J[Set TLS CA certificate]
    J --> K[Refresh token + fetch initial data]
    K --> L[Render first card]
    L --> M([Enter Main Loop])
```

---

## Main Loop

All three boards run the same non-blocking polling loop. Three independent inputs are checked on every iteration:

- **Locale button** — a press cycles the locale and shows the language name for 1.5 s. Debounced at 300 ms.
- **Card rotation** — every 5 s, advance to the next display card and call `drawCard()`.
- **Data fetch** — every **60 s** (Uno R4) or **5 min** (ESP32 / ESP32-C6), refresh the OAuth token and pull fresh weather data; the current card re-renders immediately.

```mermaid
flowchart TD
    start([Loop iteration]) --> btn{Button\npressed?}
    btn -->|Yes — debounced 300 ms| locale[Advance locale\nsv-SE→en-US→en-GB→fr-FR\nShow language name 1.5 s\ndrawCard]
    btn -->|No| card

    locale --> card{5 s elapsed\nsince last card?}
    card -->|Yes| rotate[Advance card 0→1→2→0\ndrawCard]
    card -->|No| fetch
    rotate --> fetch

    fetch{60 s / 5 min elapsed\nsince last fetch?}
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

## OAuth2 Token Refresh

Netatmo uses rotating refresh tokens — each successful refresh invalidates the old token and issues a new pair. The device must persist the latest tokens across reboots or it permanently loses access.

```mermaid
sequenceDiagram
    participant A as Device
    participant N as Netatmo API
    participant S as NVS flash

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
    Note over A: Parse JSON, render to display
```
