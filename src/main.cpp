#include <Arduino.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include <U8g2lib.h>

#ifdef ARDUINO_ARCH_RENESAS
  #include "WiFiS3.h"
  #include "WiFiSSLClient.h"
  #include <Preferences.h>
#elif defined(ARDUINO_ARCH_SAMD)
  #include <WiFiNINA.h>
  #include <FlashStorage_SAMD.h>
#else
  #error "Unsupported board — add an env for your target in platformio.ini"
#endif

// Initial credentials from arduino_secrets.h — overridden by stored tokens after first refresh
String accessToken  = ACCESS_TOKEN;
String refreshToken = REFRESH_TOKEN;
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

int status = WL_IDLE_STATUS;
const char *server = "api.netatmo.com";

// Cap HTTP responses to prevent RAM exhaustion on the 32KB RA4M1
const size_t MAX_RESPONSE_SIZE = 8192;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
WiFiSSLClient client;

#ifdef ARDUINO_ARCH_RENESAS
  Preferences prefs;
#elif defined(ARDUINO_ARCH_SAMD)
  typedef struct { bool valid; char access_token[512]; char refresh_token[512]; } TokenStore;
  FlashStorage(flash_tokens, TokenStore);
#endif

// DigiCert Global Root G2 — CN: DigiCert Global Root G2, expires 2038-01-15
const char *netatmo_ca =
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

// 8×8 filled water-drop icon (XBM format: bit 0 = leftmost pixel per row)
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

// ── Weather data (updated each fetch) ────────────────────────
float g_indoorTemp     = 0;
int   g_indoorHumidity = 0;
float g_airPressure    = 0;
float g_outdoorTemp    = 0;
float g_rain1h         = 0;
float g_rain24h        = 0;
bool  g_isRaining      = false;
bool  g_hasData        = false;

// ── Display card rotation ─────────────────────────────────────
uint8_t       g_card           = 0;
unsigned long g_lastCardSwitch = 0;
unsigned long g_lastFetch      = 0;
const unsigned long CARD_MS    = 5000;
const unsigned long FETCH_MS   = 60000;

void loadTokens();
void saveTokens();
void fetchWeatherData();
void drawCard(uint8_t card);
void refreshAccessToken();
String readHttpResponse();

void setup()
{
  oled.begin();
  Serial.begin(115200);

  // Timeout after 3s so the device boots standalone without a serial monitor connected
  unsigned long serialDeadline = millis() + 3000;
  while (!Serial && millis() < serialDeadline) { ; }

  loadTokens();
  Serial.println("Starting...");

  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("WiFi module not found!");
    while (true) ;
  }

  if (WiFi.firmwareVersion() < WIFI_FIRMWARE_LATEST_VERSION)
    Serial.println("WiFi firmware update available");

  oled.setFont(u8g2_font_ncenB08_tr);
  oled.clearBuffer();
  oled.drawStr(0, 32, "Connecting...");
  oled.sendBuffer();

  while (status != WL_CONNECTED)
  {
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }

#ifdef ARDUINO_ARCH_RENESAS
  // WiFiNINA (SAMD) validates against the NINA firmware's built-in CA store,
  // which already includes DigiCert Global Root G2 — no explicit cert needed.
  client.setCACert(netatmo_ca);
#endif

  if (client.connect(server, 443)) refreshAccessToken();
  else Serial.println("Connection failed (token refresh)");

  if (client.connect(server, 443)) fetchWeatherData();
  else Serial.println("Connection failed (weather fetch)");

  g_lastFetch      = millis();
  g_lastCardSwitch = millis();
}

void loop()
{
  unsigned long now = millis();

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
    else Serial.println("Connection failed (token refresh)");
    if (client.connect(server, 443)) fetchWeatherData();
    else Serial.println("Connection failed (weather fetch)");
  }

  delay(100);
}

// Reads the full HTTP response, checks for 200 OK, strips headers,
// and returns the JSON body. Returns empty string on any error.
// Capped at MAX_RESPONSE_SIZE to prevent RAM exhaustion.
String readHttpResponse()
{
  // Wait up to 5s for the first byte
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
  {
    response += (char)client.read();
  }
  client.stop();

  if (!response.startsWith("HTTP") || response.indexOf(" 200 ") == -1)
  {
    Serial.print("HTTP error: ");
    Serial.println(response.substring(0, response.indexOf('\n')));
    return "";
  }

  int jsonStart = response.indexOf('{');
  if (jsonStart == -1)
  {
    Serial.println("No JSON found in response");
    return "";
  }

  return response.substring(jsonStart);
}

void loadTokens()
{
#ifdef ARDUINO_ARCH_RENESAS
  prefs.begin("netatmo", true);
  accessToken  = prefs.getString("access_token",  accessToken);
  refreshToken = prefs.getString("refresh_token", refreshToken);
  prefs.end();
#elif defined(ARDUINO_ARCH_SAMD)
  TokenStore ts;
  flash_tokens.read(ts);
  if (ts.valid) {
    accessToken  = String(ts.access_token);
    refreshToken = String(ts.refresh_token);
  }
#endif
  Serial.println("Tokens loaded from storage");
}

void saveTokens()
{
#ifdef ARDUINO_ARCH_RENESAS
  prefs.begin("netatmo", false);
  prefs.putString("access_token",  accessToken);
  prefs.putString("refresh_token", refreshToken);
  prefs.end();
#elif defined(ARDUINO_ARCH_SAMD)
  TokenStore ts;
  ts.valid = true;
  accessToken.toCharArray(ts.access_token,  sizeof(ts.access_token));
  refreshToken.toCharArray(ts.refresh_token, sizeof(ts.refresh_token));
  flash_tokens.write(ts);
#endif
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
  if (error)
  {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  JsonObject indoor  = doc["body"]["devices"][0]["dashboard_data"];
  JsonArray  modules = doc["body"]["devices"][0]["modules"];

  JsonObject outdoor;
  JsonObject rainData;
  for (JsonObject mod : modules)
  {
    const char *type = mod["type"];
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

  Serial.print("Indoor Temp: ");     Serial.println(g_indoorTemp);
  Serial.print("Indoor Humidity: "); Serial.println(g_indoorHumidity);
  Serial.print("Air Pressure: ");    Serial.println(g_airPressure);
  Serial.print("Outdoor Temp: ");    Serial.println(g_outdoorTemp);
  Serial.print("Rain 1h: ");         Serial.println(g_rain1h);
  Serial.print("Rain 24h: ");        Serial.println(g_rain24h);

  drawCard(g_card);
}

// Card 0: indoor temp + humidity     (thermometer icon)
// Card 1: outdoor temp + pressure    (partly-cloudy icon)
// Card 2: rain 1h + 24h             (rain-cloud icon)
//
// Open Iconic weather 2x glyph codes (16×16 px):
//   66 = rain cloud   68 = partly cloudy   70 = thermometer
void drawCard(uint8_t card)
{
  oled.clearBuffer();

  switch (card)
  {
    case 0:
      oled.setFont(u8g2_font_open_iconic_weather_2x_t);
      oled.drawGlyph(0, 16, 70); // thermometer
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(20, 12, "INDOOR");
      oled.setFont(u8g2_font_logisoso28_tr);
      oled.drawStr(0, 50, (String(g_indoorTemp, 1) + "C").c_str());
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(0, 62, ("Humidity: " + String(g_indoorHumidity) + "%").c_str());
      break;

    case 1:
      oled.setFont(u8g2_font_open_iconic_weather_2x_t);
      oled.drawGlyph(0, 16, 68); // partly cloudy
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(20, 12, "OUTDOOR");
      oled.setFont(u8g2_font_logisoso28_tr);
      oled.drawStr(0, 50, (String(g_outdoorTemp, 1) + "C").c_str());
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(0, 62, ("Pressure: " + String(g_airPressure, 0) + "hPa").c_str());
      break;

    case 2:
      oled.setFont(u8g2_font_open_iconic_weather_2x_t);
      oled.drawGlyph(0, 16, 66); // rain cloud
      oled.setFont(u8g2_font_ncenB08_tr);
      oled.drawStr(20, 12, "RAIN");
      if (g_isRaining)
        oled.drawXBMP(112, 0, 8, 8, rain_drop_bmp); // "raining now" indicator
      oled.setFont(u8g2_font_logisoso16_tr);
      oled.drawStr(0, 38, ("1h:  " + String(g_rain1h, 1) + "mm").c_str());
      oled.drawStr(0, 58, ("24h: " + String(g_rain24h, 1) + "mm").c_str());
      break;
  }

  oled.sendBuffer();
}

void refreshAccessToken()
{
  String postData = "grant_type=refresh_token&refresh_token=" + refreshToken +
                    "&client_id="     + String(CLIENT_ID) +
                    "&client_secret=" + String(CLIENT_SECRET);

  client.println("POST /oauth2/token HTTP/1.1");
  client.println("Host: api.netatmo.com");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: ");
  client.println(postData.length());
  client.println("Connection: close");
  client.println();
  client.println(postData);

  String json = readHttpResponse();
  if (json.isEmpty()) return;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error)
  {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char *newAccessToken  = doc["access_token"];
  const char *newRefreshToken = doc["refresh_token"];

  if (newAccessToken && newRefreshToken)
  {
    accessToken  = String(newAccessToken);
    refreshToken = String(newRefreshToken);
    Serial.println("Tokens refreshed successfully");
    saveTokens();
  }
  else
  {
    Serial.print("Token refresh failed: ");
    Serial.println(doc["error"] | "unknown error");
  }
}
