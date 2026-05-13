// TFT_eSPI configuration for Waveshare ESP32-C6 Touch LCD 1.47
// JD9853 display driver (ST7789-compatible), 172x320 IPS, SPI interface
#pragma once
#define USER_SETUP_LOADED 1

#define ST7789_DRIVER

// Native portrait resolution; setRotation(1) gives landscape 320x172
#define TFT_WIDTH  172
#define TFT_HEIGHT 320

// SPI pins (Waveshare schematic)
#define TFT_MOSI 6   // SDA
#define TFT_SCLK 7   // SCL
#define TFT_CS   14
#define TFT_DC   15
#define TFT_RST  21
#define TFT_BL   22  // Backlight — HIGH = on

#define TFT_BACKLIGHT_ON HIGH

// Fonts to compile in
#define LOAD_GLCD  1   // 8px — fallback
#define LOAD_FONT2 1   // 16px — labels / secondary text
#define LOAD_FONT4 1   // 26px — rain values
#define LOAD_FONT6 1   // 48px 7-segment — large temperature digits
#define LOAD_FONT7 1   // 48px 7-segment proportional
#define LOAD_GFXFF 1

#define SPI_FREQUENCY      27000000
#define SPI_READ_FREQUENCY 20000000
