# Revision History

| Version | Commit | Date | Notes |
|---|---|---|---|
| v1.4 | [`322bb8e`](../../commit/322bb8e) | 2026-05-13 | Removed deep sleep from all ESP32 platforms. All three boards now run an always-on polling loop. ESP32 platforms fetch every 5 min; Uno R4 fetches every 60 s. Cards rotate every 5 s continuously. |
| v1.3 | [`14244ed`](../../commit/14244ed) | 2026-05-13 | Waveshare ESP32-C6 Touch LCD 1.47 support. Integrated 172×320 IPS display (ST7789) via LovyanGFX. pioarduino platform for ESP32-C6 Arduino support. |
| v1.2 | [`2b7d06a`](../../commit/2b7d06a) | 2026-05-13 | ESP32 DevKit support. Deep sleep between 5-min fetches (~3 mA average). Card and locale persist across sleeps via RTC memory. OLED blanked during sleep. |
| v1.11 | [`c47bf68`](../../commit/c47bf68) | 2026-05-10 | Runtime locale switching via push button on D7. Cycles sv-SE → en-US → en-GB → fr-FR. No recompile needed. |
| v1.1 | [`690098e`](../../commit/690098e) | 2026-05-10 | Locale support (en-US, en-GB, sv-SE, fr-FR) with unit conversions (°F/inHg/in for en-US). City name pulled from Netatmo API and shown on outdoor card. |
| v1.0 | [`f240fd0`](../../commit/f240fd0) | 2026-05-10 | First versioned release. Dropped Nano 33 IoT support — UNO R4 WiFi only. Added version splash screen showing app version, build date, and git commit hash. |
| — | [`c0b101f`](../../commit/c0b101f) | — | Initial commit. Base PlatformIO project. |

To restore a specific version locally:

```bash
git checkout f240fd0   # example: check out v1.0
```

To create a local branch from a version:

```bash
git checkout -b restore-v1.0 f240fd0
```
