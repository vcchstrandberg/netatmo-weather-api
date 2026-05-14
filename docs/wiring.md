# Wiring

## Arduino Uno R4 WiFi

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

### Locale button

A momentary push button on **D7** cycles through the available locales at runtime. No resistor needed — the pin uses the internal pull-up.

```
D7  ──── [ button ] ──── GND
```

Each press advances: sv-SE → en-US → en-GB → fr-FR → sv-SE …

The display briefly shows the new language name before resuming the weather cards.

---

## ESP32 DevKit

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

### Locale switching on ESP32

Press the **BOOT button (GPIO0)** at any time to cycle the locale. The display briefly shows the new language name before resuming the weather cards.

---

## AI-Thinker ESP32-CAM

```mermaid
flowchart LR
    subgraph oled["SSD1306 OLED (128×64)"]
        o_vcc[VCC]
        o_gnd[GND]
        o_sda[SDA]
        o_scl[SCL]
    end

    subgraph cam["AI-Thinker ESP32-CAM"]
        c_3v3[3V3]
        c_gnd[GND]
        c_sda[GPIO14 — SDA]
        c_scl[GPIO15 — SCL]
        c_btn[GPIO0 — BOOT]
    end

    o_vcc --- c_3v3
    o_gnd --- c_gnd
    o_sda --- c_sda
    o_scl --- c_scl
```

| OLED pin | ESP32-CAM pin | Notes |
|---|---|---|
| VCC | 3V3 | **3.3 V only** |
| GND | GND | |
| SDA | GPIO14 | Camera HREF pin — repurposed for I2C |
| SCL | GPIO15 | Camera PCLK pin — repurposed for I2C |

The locale button uses the built-in **BOOT button** (GPIO0) — no external wiring needed.

> **Note:** GPIO14 and GPIO15 are camera interface pins. They are safe to use for I2C when the camera module is not in use, which is always the case for this sketch.

> **Flashing:** The ESP32-CAM has no built-in auto-reset circuit. To flash, connect IO0 → GND and press RST (or power-cycle) before starting the upload. Disconnect IO0 from GND after flashing.

### Locale switching on ESP32-CAM

Press the **BOOT button (GPIO0)** at any time to cycle the locale. The display briefly shows the new language name before resuming the weather cards.

---

## Waveshare ESP32-C6 Touch LCD 1.47

The display is integrated on the board — no external display wiring is required. The SPI connection between the ESP32-C6 and the ST7789 panel is made on-board and configured in `include/esp32c6_waveshare_lcd/LGFX_config.h`.

| Signal | ESP32-C6 GPIO | Notes |
|---|---|---|
| SPI MOSI (SDA) | GPIO6 | On-board — no soldering |
| SPI SCLK (SCL) | GPIO7 | On-board — no soldering |
| CS | GPIO14 | On-board — no soldering |
| DC | GPIO15 | On-board — no soldering |
| RST | GPIO21 | On-board — no soldering |
| Backlight | GPIO22 | On-board — HIGH = on |

### Locale switching on Waveshare ESP32-C6

Press the **BOOT button (GPIO9)** at any time to cycle the locale. The display briefly shows the new language name before resuming the weather cards.
