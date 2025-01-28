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
You will also need to create a file called `secrets.h` in the `include` folder. This file should contain the following code:

```cpp
#define SECRET_SSID "YourSSID"
#define SECRET_PASS "Your WiFi Password"
#define SECRET_TOKEN "Your Netatmo API Token"
```

Replace `YourSSID` with your WiFi SSID, `Your WiFi Password` with your WiFi password and `Your Netatmo API Token` with your Netatmo API token.

## Missing features

- [ ] Implement a way to refresh the access token and persist on device, when it expires.
- [X] Implement a OLED Display to show the weather data.
- [ ] Design a case for the display.
