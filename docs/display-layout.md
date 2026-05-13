# Display Layout

Both display variants show the same three data cards. Labels and units reflect the active locale.

| Field | Card | Source | Unit |
|---|---|---|---|
| Indoor temp | 0 | Base station `dashboard_data.Temperature` | °C / °F |
| Indoor humidity | 0 | Base station `dashboard_data.Humidity` | % |
| Outdoor temp | 1 | NAModule1 `dashboard_data.Temperature` | °C / °F |
| Air pressure | 1 | Base station `dashboard_data.Pressure` | hPa / inHg |
| Rain 1 h | 2 | NAModule3 `dashboard_data.sum_rain_1` | mm / in |
| Rain 24 h | 2 | NAModule3 `dashboard_data.sum_rain_24` | mm / in |
| Rain-now indicator | 2 | NAModule3 `dashboard_data.Rain` > 0 | — |

---

## SSD1306 OLED — 128×64 (Uno R4 WiFi · ESP32 DevKit)

**Boot splash** — shown for 5 seconds:

```
┌──────────────────────────────┐
│ Netatmo Weather              │
│ v1.4                         │
│ May 13 2026                  │
│ 14244ed                      │  ← git commit hash
└──────────────────────────────┘
```

**Locale switch** — shown for 1.5 seconds:

```
┌──────────────────────────────┐
│ Language:                    │
│                              │
│  Svenska                     │  ← logisoso16 font
│                              │
│  sv-SE                       │
└──────────────────────────────┘
```

Three full-screen cards rotate every 5 seconds. Each shows a 16×16 Open Iconic weather icon, a large primary value, and a smaller secondary value.

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

---

## TFT — 320×172 landscape (Waveshare ESP32-C6)

**Boot splash** — shown on boot:

```
┌────────────────────────────────────────────────────────────────┐
│ Netatmo Weather                                                │
│ v1.4                                                           │
│ May 13 2026                                                    │
│ 14244ed                                                        │
└────────────────────────────────────────────────────────────────┘
```

**Locale switch** — shown for 1.5 seconds when BOOT is pressed:

```
┌────────────────────────────────────────────────────────────────┐
│ Language:                                                      │
│                                                                │
│  Svenska                                                       │
│                                                                │
│  sv-SE                                                         │
└────────────────────────────────────────────────────────────────┘
```

Three cards rotate every 5 seconds. Each card has a coloured header bar, a large primary value, and a secondary line.

**Card 0 — Indoor** (warm amber header)
```
┌────────────────────────────────────────────────────────────────┐
│▓▓▓▓▓▓ INNE / INDOOR ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ sv-SE ▓▓▓▓▓▓│
│                                                                │
│  21.5           C                                              │  ← 48px 7-segment + unit
│                                                                │
│                                                                │
│                                                                │
│  Fukt: 45% / Humidity: 45%                                     │  ← secondary line
└────────────────────────────────────────────────────────────────┘
```

**Card 1 — Outdoor** (sky-blue header)
```
┌────────────────────────────────────────────────────────────────┐
│▓▓▓▓▓▓ Stockholm ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ sv-SE ▓▓▓▓▓▓│  ← city name
│                                                                │
│  8.3            C                                              │
│                                                                │
│                                                                │
│                                                                │
│  Tryck: 1013hPa / 29.92inHg                                    │
└────────────────────────────────────────────────────────────────┘
```

**Card 2 — Rain** (teal header)
```
┌────────────────────────────────────────────────────────────────┐
│▓▓▓▓▓▓ REGN / RAIN ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ * sv-SE ▓▓▓▓▓▓│  ← * = raining now
│                                                                │
│  1h:  0.6mm / 0.02in                                           │  ← 26px font
│                                                                │
│  24h: 3.2mm / 0.13in                                           │
│                                                                │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```
