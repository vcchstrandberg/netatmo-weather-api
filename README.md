# netatmo-weather-api

Purpose with this is to build an Arduino based display that can present the weather data from my Netatmo Weather Station without the need to go to the app or web interface.

## Getting Started

### Prerequisites

1. Visual Studio Code with PlatformIO installed.
2. Arduino Uno R4 WiFi.
3. SSD1306 128x64 OLED display (I2C).
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

You only need to set valid initial tokens here once. After the first successful run the device automatically refreshes and persists the latest tokens — they survive reboots.

### Token persistence

After each successful OAuth2 token refresh, the new access and refresh tokens are saved to the ESP32-S3's NVS (non-volatile storage) using the `Preferences` library. NVS uses wear-levelled LittleFS flash on the ESP32 side, so write frequency is not a concern. On the next reboot the stored tokens are loaded automatically, overriding the hardcoded values in `arduino_secrets.h`.

### Building and flashing

Open the project folder in VS Code with PlatformIO. Select the `uno_r4_wifi` environment and click **Upload**.

## How it works

1. On boot, any previously saved tokens are loaded from NVS
2. The board connects to WiFi using the credentials in `arduino_secrets.h`
3. Every 60 seconds it:
   - Refreshes the Netatmo OAuth2 access token and saves the new tokens to NVS
   - Fetches current weather data from `api.netatmo.com/api/getstationsdata`
   - Displays indoor temperature, indoor humidity, air pressure, and outdoor temperature on the OLED

## Missing features

- [X] Implement a way to refresh the access token and persist on device when it expires.
- [X] Implement an OLED display to show the weather data.
- [ ] Design a case for the display.
