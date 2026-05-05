# netatmo-weather-api

Purpose with this is to build an arduino based display that can present the weather data from my Netatmo Weather Station without the need to go to the app or web interface.

## Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

### Prerequisites

What things you need to install the software and how to run them.

1. Visual Studio Code with PlatformIO installed.
2. Arduino Uno R4 Wifi or similar.
3. Netatmo Weather Station with an account and API key from Netatmo.

### Setting up the project in Visual Studio Code

To get started with the project you need to clone the repository to your local machine. Open Visual Studio Code and open the project folder.

### First-run configuration

The device is configured via a `config.json` file stored on its internal QSPI flash. No credentials are hardcoded.

**On first boot (no config found):**
1. Connect the device to your computer via USB
2. The OLED will display "No config found!" and the device will appear as a USB drive
3. Create a file called `config.json` on the drive with the following content:

```json
{
  "ssid": "YourWiFiSSID",
  "pass": "YourWiFiPassword",
  "client_id": "your_netatmo_client_id",
  "client_secret": "your_netatmo_client_secret",
  "access_token": "your_initial_netatmo_access_token",
  "refresh_token": "your_initial_netatmo_refresh_token"
}
```

4. Eject the USB drive and reset the board — it will connect to WiFi and start fetching data

The device automatically refreshes and persists the Netatmo tokens after each successful refresh, so the tokens in `config.json` stay up to date across reboots.

## Missing features

- [X] Implement a way to refresh the access token and persist on device, when it expires.
- [X] Implement a OLED Display to show the weather data.
- [X] USB mass storage config — configure the device without recompiling via config.json.
- [ ] Design a case for the display.
