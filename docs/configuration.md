# Configuration and Setup

## File structure

After cloning, your local project should look like this before building:

```
netatmo-weather-api/
├── boards/
│   └── waveshare_esp32c6_lcd.json      ← custom board definition for PlatformIO
├── docs/                               ← documentation
├── include/
│   ├── uno_r4_wifi/
│   │   └── arduino_secrets.h           ← you create this for Uno R4 (gitignored)
│   ├── esp32dev/
│   │   └── arduino_secrets.h           ← you create this for ESP32 DevKit (gitignored)
│   └── esp32c6_waveshare_lcd/
│       ├── LGFX_config.h               ← LovyanGFX pin / panel configuration
│       └── arduino_secrets.h           ← you create this for Waveshare (gitignored)
├── scripts/
│   └── version.py                      ← injects git commit hash at build time
├── src/
│   └── main.cpp
├── enclosure/
│   └── enclosure.scad
└── platformio.ini
```

The secrets files are listed in `.gitignore` — they will never be pushed to GitHub. You create them manually using the format shown below.

---

## Locale and units

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

The city name is pulled automatically from the Netatmo API and shown on the outdoor card. Locale can also be changed at runtime by pressing the locale button — see [Wiring](wiring.md).

---

## Credentials

Create the secrets file for your board in the matching include directory:

| Board | Path |
|---|---|
| Arduino Uno R4 WiFi | `include/uno_r4_wifi/arduino_secrets.h` |
| ESP32 DevKit | `include/esp32dev/arduino_secrets.h` |
| Waveshare ESP32-C6 | `include/esp32c6_waveshare_lcd/arduino_secrets.h` |

All three files use the same format:

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

---

## Finding the board's USB port

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

## Building and flashing

Install PlatformIO Core if you haven't already:

```bash
pip install platformio
```

Then from the project root:

```bash
# Compile only
pio run -e uno_r4_wifi            # Arduino Uno R4 WiFi
pio run -e esp32dev               # ESP32 DevKit
pio run -e esp32c6_waveshare_lcd  # Waveshare ESP32-C6 Touch LCD 1.47

# Compile and flash to the connected board
pio run -e uno_r4_wifi            --target upload
pio run -e esp32dev               --target upload
pio run -e esp32c6_waveshare_lcd  --target upload
```

The first build for each environment downloads the required toolchain and libraries automatically. The Waveshare build uses the pioarduino platform which includes arduino-esp32 3.x (~300 MB extra, one-time download).

You can also use the **Upload** and **Serial Monitor** buttons in the PlatformIO toolbar in VS Code.

---

## Serial monitor

The board prints boot diagnostics and runtime status over USB serial at 115200 baud:

```bash
pio device monitor -e uno_r4_wifi            # Uno R4
pio device monitor -e esp32dev               # ESP32 DevKit
pio device monitor -e esp32c6_waveshare_lcd  # Waveshare ESP32-C6
```

PlatformIO auto-detects the port. Press **Ctrl-C** to exit. You can also use any serial terminal pointed at the same port and baud rate:

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
