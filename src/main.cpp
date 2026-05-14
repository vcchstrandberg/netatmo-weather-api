#include <Arduino.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include <Preferences.h>

// ── Platform selection ────────────────────────────────────────────────────────
// WAVESHARE_ESP32C6_LCD  — Waveshare ESP32-C6 Touch LCD 1.47 (integrated TFT)
// ESP32CAM               — AI-Thinker ESP32-CAM + external SSD1306 (I2C GPIO14/15)
// ESP32                  — generic ESP32 DevKit + external SSD1306 (I2C GPIO21/22)
// (neither)              — Arduino Uno R4 WiFi + external SSD1306 (I2C)
// All boards use a continuous polling loop. ESP32 platforms fetch every
// 5 minutes; Uno R4 fetches every 60 seconds.

#ifdef WAVESHARE_ESP32C6_LCD
#  include "LGFX_config.h"
#  include <LGFX_TFT_eSPI.hpp>
#  include <WiFi.h>
#  include <WiFiClientSecure.h>
#  define BUTTON_PIN 9   // BOOT button on ESP32-C6
#elif defined(ESP32)
#  include <U8g2lib.h>
#  include <Wire.h>
#  include <WiFi.h>
#  include <WiFiClientSecure.h>
#  define BUTTON_PIN 0   // BOOT button on most ESP32 devkit boards
#else
#  include <U8g2lib.h>
#  include <Wire.h>
#  include "WiFiS3.h"
#  include "WiFiSSLClient.h"
#  define BUTTON_PIN 7
#endif

// ── Locale ────────────────────────────────────────────────────────────────────
struct Locale {
  const char* name;
  const char* code;
  const char* indoor;
  const char* outdoor;
  const char* rain;
  const char* humidity;
  const char* pressure;
  const char* temp_unit;
  const char* pressure_unit;
  const char* rain_unit;
  uint8_t     pressure_decimals;
  uint8_t     rain_decimals;
  bool        fahrenheit;
  bool        inhg;
  bool        inches;
  const char* connecting;
  const char* wifi_failed;
  const char* check_creds;
  const char* retrying;
  const char* api_unreachable;
  const char* token_expired;
  const char* reflash;
};

static const Locale L_SV_SE = {
  "Svenska",    "sv-SE",
  "INNE",       "UTE",        "REGN",
  "Fukt: ",     "Tryck: ",
  "C",          "hPa",        "mm",
  0, 1, false, false, false,
  "Ansluter WiFi:",  "WiFi fel",      "Kontrollera",
  "Forsoker...",     "API oatkomlig", "Token utgatt", "Ladda om cfg"
};
static const Locale L_EN_US = {
  "English US", "en-US",
  "INDOOR",     "OUTDOOR",    "RAIN",
  "Humidity: ", "Pressure: ",
  "F",          "inHg",       "in",
  2, 2, true, true, true,
  "Connecting to WiFi:", "WiFi failed",  "Check credentials",
  "Retrying...",         "API unreachable", "Token expired", "Reflash secrets"
};
static const Locale L_EN_GB = {
  "English UK", "en-GB",
  "INDOOR",     "OUTDOOR",    "RAIN",
  "Humidity: ", "Pressure: ",
  "C",          "hPa",        "mm",
  0, 1, false, false, false,
  "Connecting to WiFi:", "WiFi failed",  "Check credentials",
  "Retrying...",         "API unreachable", "Token expired", "Reflash secrets"
};
static const Locale L_FR_FR = {
  "Francais",   "fr-FR",
  "INTERIEUR",  "EXTERIEUR",  "PLUIE",
  "Humidite: ", "Pression: ",
  "C",          "hPa",        "mm",
  0, 1, false, false, false,
  "Connexion WiFi:", "WiFi echoue",   "Ver. identifiants",
  "Reessai...",      "API inaccessible", "Token expire", "Reflasher cfg"
};

// Cycle order: sv-SE → en-US → en-GB → fr-FR → sv-SE …
static const Locale* const locales[] = { &L_SV_SE, &L_EN_US, &L_EN_GB, &L_FR_FR };
static const uint8_t LOCALE_COUNT = 4;
static uint8_t       g_localeIndex = 0;
static const Locale* g_loc = locales[0];  // synced from g_localeIndex at top of setup()

inline float toDisplayTemp(float c)     { return g_loc->fahrenheit ? c * 9.0f / 5.0f + 32.0f : c; }
inline float toDisplayPressure(float h) { return g_loc->inhg       ? h * 0.02953f              : h; }
inline float toDisplayRain(float mm)    { return g_loc->inches     ? mm * 0.03937f             : mm; }

// ── Credentials (overridden by stored tokens after first refresh) ─────────────
String accessToken  = ACCESS_TOKEN;
String refreshToken = REFRESH_TOKEN;
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

int status = WL_IDLE_STATUS;
const char* server = "api.netatmo.com";

// Cap HTTP responses to prevent RAM exhaustion on the 32 KB RA4M1
const size_t MAX_RESPONSE_SIZE = 8192;

// ── Display objects ───────────────────────────────────────────────────────────
#ifdef WAVESHARE_ESP32C6_LCD
TFT_eSPI tft;
// Card header colours: indoor (warm amber), outdoor (sky blue), rain (teal)
static const uint16_t CARD_COLOR[] = { 0xFB60, 0x235F, 0x03DF };
#else
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
#endif

// ── WiFi client ───────────────────────────────────────────────────────────────
#if defined(WAVESHARE_ESP32C6_LCD) || defined(ESP32)
WiFiClientSecure client;
#else
WiFiSSLClient    client;
#endif

Preferences prefs;

// DigiCert Global Root G2 — CN: DigiCert Global Root G2, expires 2038-01-15
const char* netatmo_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
    "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
    "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
    "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
    "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
    "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
    "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
    "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
    "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
    "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
    "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
    "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
    "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
    "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
    "MrY=\n"
    "-----END CERTIFICATE-----\n";

// 8×8 filled water-drop icon — only needed for U8g2 (XBM format)
#ifndef WAVESHARE_ESP32C6_LCD
static const uint8_t rain_drop_bmp[] PROGMEM = {
    0x18,  // ...XX...
    0x3C,  // ..XXXX..
    0x7E,  // .XXXXXX.
    0xFF,  // XXXXXXXX
    0xFF,  // XXXXXXXX
    0x7E,  // .XXXXXX.
    0x3C,  // ..XXXX..
    0x18,  // ...XX...
};
#endif

// ── Weather data (updated each fetch) ────────────────────────────────────────
float  g_indoorTemp     = 0;
int    g_indoorHumidity = 0;
float  g_airPressure    = 0;
float  g_outdoorTemp    = 0;
float  g_rain1h         = 0;
float  g_rain24h        = 0;
bool   g_isRaining      = false;
bool   g_hasData        = false;
String g_city           = "";

// ── Display card rotation ─────────────────────────────────────────────────────
uint8_t       g_card           = 0;
unsigned long g_lastCardSwitch = 0;
unsigned long g_lastFetch      = 0;
const unsigned long CARD_MS  = 5000;
#ifdef ESP32
const unsigned long FETCH_MS = 300000;  // 5 min — Netatmo station updates every 5 min
#else
const unsigned long FETCH_MS = 60000;   // 1 min — Uno R4
#endif

// ── Forward declarations ──────────────────────────────────────────────────────
void loadTokens();
void saveTokens();
void fetchWeatherData();
void drawCard(uint8_t card);
void showError(const char* title, const char* detail = nullptr);
void showLocale();
void refreshAccessToken();
String readHttpResponse();

// ── setup() ───────────────────────────────────────────────────────────────────
void setup()
{
  // Restore locale pointer from the RTC-persistent index on every boot/wake.
  g_loc = locales[g_localeIndex];

  Serial.begin(115200);
  unsigned long serialDeadline = millis() + 3000;
  while (!Serial && millis() < serialDeadline) { ; }
  Serial.println("=== Boot ===");
  Serial.println("Serial OK");

// ── Waveshare ESP32-C6 LCD init ───────────────────────────────────────────────
#ifdef WAVESHARE_ESP32C6_LCD
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1);   // landscape: 320 × 172
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Netatmo Weather", 4, 30);
  tft.drawString("v" APP_VERSION, 4, 65);
  tft.drawString(__DATE__, 4, 100);
  tft.drawString(GIT_COMMIT, 4, 135);
  delay(5000);
  tft.fillScreen(TFT_BLACK);

// ── SSD1306 OLED init (ESP32 devkit + Uno R4) ────────────────────────────────
#else
#ifdef ESP32CAM
  Wire.begin(14, 15);  // SDA=GPIO14, SCL=GPIO15 (camera pins repurposed)
#else
  Wire.begin();        // ESP32 DevKit: SDA=GPIO21, SCL=GPIO22 (hardware defaults)
#endif
  Serial.println("I2C scan:");
  uint8_t found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println("  No I2C devices found!");

  Serial.println("Calling oled.begin()...");
  bool oledOk = oled.begin();
  Serial.print("oled.begin() = ");
  Serial.println(oledOk ? "true (OK)" : "false (FAILED)");

  if (oledOk) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(0, 12, "Netatmo Weather");
    oled.drawStr(0, 28, "v" APP_VERSION);
    oled.drawStr(0, 44, __DATE__);
    oled.drawStr(0, 60, GIT_COMMIT);
    oled.sendBuffer();
    delay(5000);
  } else {
    Serial.println("OLED init failed — skipping draw");
  }

#endif  // display init

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  loadTokens();
  Serial.println("Starting...");

#ifndef ESP32
  // Uno R4: verify the WiFi co-processor is present.
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WiFi module not found!");
    while (true) ;
  }
  if (WiFi.firmwareVersion() < WIFI_FIRMWARE_LATEST_VERSION)
    Serial.println("WiFi firmware update available");
#endif

  // Show "connecting" on whichever display is active.
#ifdef WAVESHARE_ESP32C6_LCD
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(g_loc->connecting, 4, 60);
  tft.drawString(ssid, 4, 90);
#else
  oled.setFont(u8g2_font_ncenB08_tr);
  oled.clearBuffer();
  oled.drawStr(0, 20, g_loc->connecting);
  oled.drawStr(0, 34, ssid);
  oled.sendBuffer();
#endif

  // WiFi connect loop — ESP32/C6 polls status, Uno R4 relies on begin() return value.
#ifdef ESP32
  WiFi.begin(ssid, pass);
  uint8_t wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (++wifiAttempts >= 60)
      { showError(g_loc->wifi_failed, g_loc->check_creds); break; }
  }
#else
  uint8_t wifiAttempts = 0;
  while (status != WL_CONNECTED) {
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
    if (++wifiAttempts == 3)
      showError(g_loc->wifi_failed, g_loc->check_creds);
  }
#endif

  client.setCACert(netatmo_ca);

  if (client.connect(server, 443)) refreshAccessToken();
  else { Serial.println("Connection failed (token refresh)"); showError(g_loc->api_unreachable, "Token refresh"); }

  if (client.connect(server, 443)) fetchWeatherData();
  else { Serial.println("Connection failed (weather fetch)"); showError(g_loc->api_unreachable, "Weather fetch"); }

  g_lastFetch      = millis();
  g_lastCardSwitch = millis();
}

// ── showLocale() ──────────────────────────────────────────────────────────────
void showLocale()
{
#ifdef WAVESHARE_ESP32C6_LCD
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Language:", 4, 30);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(g_loc->name, 4, 70);
  tft.setTextFont(2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(g_loc->code, 4, 118);
  delay(1500);
  tft.fillScreen(TFT_BLACK);
#else
  oled.clearBuffer();
  oled.setFont(u8g2_font_ncenB08_tr);
  oled.drawStr(0, 12, "Language:");
  oled.setFont(u8g2_font_logisoso16_tr);
  oled.drawStr(0, 38, g_loc->name);
  oled.setFont(u8g2_font_ncenB08_tr);
  oled.drawStr(0, 54, g_loc->code);
  oled.sendBuffer();
  delay(1500);
#endif
}

// ── loop() ────────────────────────────────────────────────────────────────────
void loop()
{
  unsigned long now = millis();

  static unsigned long lastPress = 0;
  if (digitalRead(BUTTON_PIN) == LOW && now - lastPress > 300)
  {
    lastPress = now;
    g_localeIndex = (g_localeIndex + 1) % LOCALE_COUNT;
    g_loc = locales[g_localeIndex];
    Serial.print("Locale: "); Serial.println(g_loc->code);
    showLocale();
    if (g_hasData) drawCard(g_card);
  }

  if (g_hasData && now - g_lastCardSwitch >= CARD_MS)
  {
    g_lastCardSwitch = now;
    g_card = (g_card + 1) % 3;
    drawCard(g_card);
  }

  if (now - g_lastFetch >= FETCH_MS)
  {
    g_lastFetch = now;
    if (client.connect(server, 443)) refreshAccessToken();
    else { Serial.println("Connection failed (token refresh)"); showError(g_loc->api_unreachable, "Token refresh"); }
    if (client.connect(server, 443)) fetchWeatherData();
    else { Serial.println("Connection failed (weather fetch)"); showError(g_loc->api_unreachable, "Weather fetch"); }
  }

  delay(100);
}

// ── Shared networking helpers ─────────────────────────────────────────────────

// Reads the full HTTP response, checks for 200 OK, strips headers,
// and returns the JSON body. Returns empty string on any error.
// Capped at MAX_RESPONSE_SIZE to prevent RAM exhaustion.
String readHttpResponse()
{
  unsigned long timeout = millis() + 5000;
  while (!client.available() && millis() < timeout) { delay(10); }

  if (!client.available())
  {
    Serial.println("HTTP response timeout");
    client.stop();
    return "";
  }

  String response = "";
  while (client.available() && response.length() < MAX_RESPONSE_SIZE)
    response += (char)client.read();
  client.stop();

  if (!response.startsWith("HTTP") || response.indexOf(" 200 ") == -1)
  {
    Serial.print("HTTP error: ");
    Serial.println(response.substring(0, response.indexOf('\n')));
    return "";
  }

  int jsonStart = response.indexOf('{');
  if (jsonStart == -1) { Serial.println("No JSON found"); return ""; }
  return response.substring(jsonStart);
}

void loadTokens()
{
  prefs.begin("netatmo", true);
  accessToken  = prefs.getString("access_token",  accessToken);
  refreshToken = prefs.getString("refresh_token", refreshToken);
  prefs.end();
  Serial.println("Tokens loaded from storage");
}

void saveTokens()
{
  prefs.begin("netatmo", false);
  prefs.putString("access_token",  accessToken);
  prefs.putString("refresh_token", refreshToken);
  prefs.end();
  Serial.println("Tokens saved to storage");
}

void fetchWeatherData()
{
  client.println("GET /api/getstationsdata HTTP/1.1");
  client.println("Host: api.netatmo.com");
  client.print("Authorization: Bearer ");
  client.println(accessToken);
  client.println("Connection: close");
  client.println();

  String json = readHttpResponse();
  if (json.isEmpty()) return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) { Serial.print("JSON parse failed: "); Serial.println(error.c_str()); return; }

  const char* city = doc["body"]["devices"][0]["place"]["city"];
  if (city) g_city = String(city);

  JsonObject indoor  = doc["body"]["devices"][0]["dashboard_data"];
  JsonArray  modules = doc["body"]["devices"][0]["modules"];

  JsonObject outdoor, rainData;
  for (JsonObject mod : modules) {
    const char* type = mod["type"];
    if (!type) continue;
    if (strcmp(type, "NAModule1") == 0) outdoor  = mod["dashboard_data"];
    if (strcmp(type, "NAModule3") == 0) rainData = mod["dashboard_data"];
  }

  g_indoorTemp     = indoor["Temperature"];
  g_indoorHumidity = indoor["Humidity"];
  g_airPressure    = indoor["Pressure"];
  g_outdoorTemp    = outdoor["Temperature"];
  g_rain1h         = rainData["sum_rain_1"]  | 0.0f;
  g_rain24h        = rainData["sum_rain_24"] | 0.0f;
  g_isRaining      = (rainData["Rain"]       | 0.0f) > 0.0f;
  g_hasData        = true;

  Serial.print("City: ");            Serial.println(g_city);
  Serial.print("Indoor Temp: ");     Serial.println(g_indoorTemp);
  Serial.print("Indoor Humidity: "); Serial.println(g_indoorHumidity);
  Serial.print("Air Pressure: ");    Serial.println(g_airPressure);
  Serial.print("Outdoor Temp: ");    Serial.println(g_outdoorTemp);
  Serial.print("Rain 1h: ");         Serial.println(g_rain1h);
  Serial.print("Rain 24h: ");        Serial.println(g_rain24h);

  drawCard(g_card);
}

void refreshAccessToken()
{
  String postData = "grant_type=refresh_token&refresh_token=" + refreshToken +
                    "&client_id="     + String(CLIENT_ID) +
                    "&client_secret=" + String(CLIENT_SECRET);

  client.println("POST /oauth2/token HTTP/1.1");
  client.println("Host: api.netatmo.com");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: "); client.println(postData.length());
  client.println("Connection: close");
  client.println();
  client.println(postData);

  String json = readHttpResponse();
  if (json.isEmpty()) return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) { Serial.print("JSON parse failed: "); Serial.println(error.c_str()); return; }

  const char* newAccessToken  = doc["access_token"];
  const char* newRefreshToken = doc["refresh_token"];

  if (newAccessToken && newRefreshToken) {
    accessToken  = String(newAccessToken);
    refreshToken = String(newRefreshToken);
    Serial.println("Tokens refreshed successfully");
    saveTokens();
  } else {
    Serial.print("Token refresh failed: ");
    Serial.println(doc["error"] | "unknown error");
    showError(g_loc->token_expired, g_loc->reflash);
  }
}

// ── showError() ───────────────────────────────────────────────────────────────
void showError(const char* title, const char* detail)
{
#ifdef WAVESHARE_ESP32C6_LCD
  tft.fillScreen(0x4000);   // dark red
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, 0x4000);
  tft.drawString("ERROR", 4, 20);
  tft.setTextFont(2);
  tft.drawString(title, 4, 65);
  if (detail) tft.drawString(detail, 4, 95);
  tft.drawString(g_loc->retrying, 4, 135);
#else
  oled.clearBuffer();
  oled.setFont(u8g2_font_open_iconic_embedded_2x_t);
  oled.drawGlyph(0, 16, 71); // alert icon
  oled.setFont(u8g2_font_ncenB08_tr);
  oled.drawStr(20, 12, "ERROR");
  oled.drawStr(0, 30, title);
  if (detail) oled.drawStr(0, 44, detail);
  oled.drawStr(0, 58, g_loc->retrying);
  oled.sendBuffer();
#endif
}

// ── drawCard() ────────────────────────────────────────────────────────────────
// Card 0: indoor temp + humidity
// Card 1: outdoor temp + pressure  (city name as title if known)
// Card 2: rain 1h + 24h

#ifdef WAVESHARE_ESP32C6_LCD
// ── TFT layout (landscape 320 × 172) ─────────────────────────────────────────
// y=0..27   coloured header bar — card title left, locale code right
// y=35..82  main value: Font 6 (48 px, digits only) + Font 4 unit suffix
// y=138..   secondary label (Font 2)

void drawCard(uint8_t card)
{
  tft.fillScreen(TFT_BLACK);

  uint16_t hdrColor = CARD_COLOR[card];
  tft.fillRect(0, 0, tft.width(), 28, hdrColor);

  // Header text
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, hdrColor);
  const char* title = (card == 0) ? g_loc->indoor
                    : (card == 1) ? (g_city.length() > 0 ? g_city.c_str() : g_loc->outdoor)
                    :               g_loc->rain;
  tft.drawString(title, 4, 6);

  tft.setTextDatum(TR_DATUM);
  tft.drawString(g_loc->code, tft.width() - 4, 6);
  tft.setTextDatum(TL_DATUM);

  // Card-specific content
  switch (card)
  {
    case 0:
    case 1: {
      float    temp    = (card == 0) ? g_indoorTemp : g_outdoorTemp;
      String   numStr  = String(toDisplayTemp(temp), 1);
      String   unitStr = String(g_loc->temp_unit);

      // Large digits in Font 6 (7-segment; only has 0–9, minus, decimal)
      tft.setTextFont(6);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(numStr, 10, 38);
      int numW = tft.textWidth(numStr);

      // Unit letter in Font 4 (full char set) immediately to the right
      tft.setTextFont(4);
      tft.drawString(unitStr, 14 + numW, 50);

      // Secondary line
      tft.setTextFont(2);
      tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      if (card == 0) {
        tft.drawString(String(g_loc->humidity) + String(g_indoorHumidity) + "%", 10, 140);
      } else {
        tft.drawString(String(g_loc->pressure) +
                       String(toDisplayPressure(g_airPressure), (unsigned int)g_loc->pressure_decimals) +
                       g_loc->pressure_unit, 10, 140);
      }
      break;
    }
    case 2: {
      // Rain-now dot in header
      if (g_isRaining) {
        tft.setTextDatum(TR_DATUM);
        tft.setTextFont(4);
        tft.setTextColor(TFT_WHITE, hdrColor);
        tft.drawString("*", tft.width() - tft.textWidth(g_loc->code) - 20, 3);
        tft.setTextDatum(TL_DATUM);
      }

      tft.setTextFont(4);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("1h:  " + String(toDisplayRain(g_rain1h),  (unsigned int)g_loc->rain_decimals) + " " + g_loc->rain_unit, 10, 48);
      tft.drawString("24h: " + String(toDisplayRain(g_rain24h), (unsigned int)g_loc->rain_decimals) + " " + g_loc->rain_unit, 10, 108);
      break;
    }
  }
}

#else
// ── SSD1306 OLED layout (128 × 64) ───────────────────────────────────────────
// Open Iconic weather 2x glyph codes (16×16 px):
//   64 = cloud   67 = rain   69 = sun

void drawCard(uint8_t card)
{
  oled.clearBuffer();

  switch (card)
  {
    case 0:
      oled.setFont(u8g2_font_open_iconic_weather_2x_t);
      oled.drawGlyph(0, 16, 69); // sun
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(20, 12, g_loc->indoor);
      oled.setFont(u8g2_font_logisoso28_tr);
      oled.drawStr(0, 50, (String(toDisplayTemp(g_indoorTemp), 1) + g_loc->temp_unit).c_str());
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(0, 62, (String(g_loc->humidity) + String(g_indoorHumidity) + "%").c_str());
      break;

    case 1:
      oled.setFont(u8g2_font_open_iconic_weather_2x_t);
      oled.drawGlyph(0, 16, 64); // cloud
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(20, 12, g_city.length() > 0 ? g_city.c_str() : g_loc->outdoor);
      oled.setFont(u8g2_font_logisoso28_tr);
      oled.drawStr(0, 50, (String(toDisplayTemp(g_outdoorTemp), 1) + g_loc->temp_unit).c_str());
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(0, 62, (String(g_loc->pressure) + String(toDisplayPressure(g_airPressure), (unsigned int)g_loc->pressure_decimals) + g_loc->pressure_unit).c_str());
      break;

    case 2:
      oled.setFont(u8g2_font_open_iconic_weather_2x_t);
      oled.drawGlyph(0, 16, 67); // rain
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(20, 12, g_loc->rain);
      if (g_isRaining)
        oled.drawXBMP(112, 0, 8, 8, rain_drop_bmp);
      oled.setFont(u8g2_font_logisoso16_tr);
      oled.drawStr(0, 38, ("1h:  " + String(toDisplayRain(g_rain1h),  (unsigned int)g_loc->rain_decimals) + g_loc->rain_unit).c_str());
      oled.drawStr(0, 58, ("24h: " + String(toDisplayRain(g_rain24h), (unsigned int)g_loc->rain_decimals) + g_loc->rain_unit).c_str());
      break;
  }

  oled.sendBuffer();
}
#endif
