#include <Arduino.h>
#include "WiFiS3.h"
#include "WiFiSSLClient.h"
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include <U8g2lib.h>
#include <Preferences.h>

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
Preferences prefs;

// GoDaddy Root CA G2 — CN: Go Daddy Root Certificate Authority - G2, expires 2037-12-31
const char *netatmo_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDxTCCAq2gAwIBAgIBADANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx\n"
    "EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT\n"
    "EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp\n"
    "ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5MDkwMTAwMDAwMFoXDTM3MTIzMTIz\n"
    "NTk1OVowgYMxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH\n"
    "EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjExMC8GA1UE\n"
    "AxMoR28gRGFkZHkgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIw\n"
    "DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL9xYgjx+lk09xvJGKP3gElY6SKD\n"
    "E6bFIEMBO4Tx5oVJnyfq9oQbTqC023CYxzIBsQU+B07u9PpPL1kwIuerGVZr4oAH\n"
    "/PMWdYA5UXvl+TW2dE6pjYIT5LY/qQOD+qK+ihVqf94Lw7YZFAXK6sOoBJQ7Rnwy\n"
    "DfMAZiLIjWltNowRGLfTshxgtDj6AozO091GB94KPutdfMh8+7ArU6SSYmlRJQVh\n"
    "GkSBjCypQ5Yj36w6gZoOKcUcqeldHraenjAKOc7xiID7S13MMuyFYkMlNAJWJwGR\n"
    "tDtwKj9useiciAF9n9T521NtYJ2/LOdYq7hfRvzOxBsDPAnrSTFcaUaz4EcCAwEA\n"
    "AaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYE\n"
    "FDqahQcQZyi27/a9BUFuIMGU2g/eMA0GCSqGSIb3DQEBCwUAA4IBAQCZ21151fmX\n"
    "WWcDYfF+OwYxdS2hII5PZYe096acvNjpL9DbWu7PdIxztDhC2gV7+AJ1uP2lsdeu\n"
    "9tfeE8tTEH6KRtGX+rcuKxGrkLAngPnon1rpN5+r5N9ss4UXnT3ZJE95kTXWXwTr\n"
    "gIOrmgIttRD02JDHBHNA7XIloKmf7J6raBKZV8aPEjoJpL1E/QYVN8Gb5DKj7Tjo\n"
    "2GTzLH4U/ALqn83/B2gX2yKQOC16jdFU8WnjXzPKej17CuPKf1855eJ1usV2GDPO\n"
    "LPAvTK33sefOT6jEm0pUBsV/fdUID+Ic/n4XuKxe9tQWskMJDE32p2u0mYRlynqI\n"
    "4uJEvlz36hz1\n"
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

void loadTokens();
void saveTokens();
void fetchWeatherData();
void updateDisplay(float indoorTemp, int indoorHumidity, float airPressure,
                   float outdoorTemp, float rain1h, float rain24h, bool isRaining);
void refreshAccessToken();
String readHttpResponse();

void setup()
{
  oled.begin();
  oled.setFont(u8g2_font_ncenB08_tr);
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
  {
    Serial.println("WiFi firmware update available");
  }

  while (status != WL_CONNECTED)
  {
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }

  client.setCACert(netatmo_ca);
}

void loop()
{
  if (client.connect(server, 443))
  {
    refreshAccessToken();
  }
  else
  {
    Serial.println("Connection failed (token refresh)");
  }

  if (client.connect(server, 443))
  {
    fetchWeatherData();
  }
  else
  {
    Serial.println("Connection failed (weather fetch)");
  }

  delay(60000);
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

  float indoorTemp     = indoor["Temperature"];
  int   indoorHumidity = indoor["Humidity"];
  float airPressure    = indoor["Pressure"];
  float outdoorTemp    = outdoor["Temperature"];
  float rain1h         = rainData["sum_rain_1"]  | 0.0f;
  float rain24h        = rainData["sum_rain_24"] | 0.0f;
  bool  isRaining      = (rainData["Rain"]       | 0.0f) > 0.0f;

  Serial.print("Indoor Temp: ");     Serial.println(indoorTemp);
  Serial.print("Indoor Humidity: "); Serial.println(indoorHumidity);
  Serial.print("Air Pressure: ");    Serial.println(airPressure);
  Serial.print("Outdoor Temp: ");    Serial.println(outdoorTemp);
  Serial.print("Rain 1h: ");         Serial.println(rain1h);
  Serial.print("Rain 24h: ");        Serial.println(rain24h);

  updateDisplay(indoorTemp, indoorHumidity, airPressure, outdoorTemp, rain1h, rain24h, isRaining);
}

void updateDisplay(float indoorTemp, int indoorHumidity, float airPressure,
                   float outdoorTemp, float rain1h, float rain24h, bool isRaining)
{
  oled.clearDisplay();
  oled.drawStr(0, 10, ("IndoorTemp: "     + String(indoorTemp, 1)).c_str());
  oled.drawStr(0, 20, ("IndoorHumidity: " + String(indoorHumidity)).c_str());
  oled.drawStr(0, 30, ("AirPressure: "    + String(airPressure, 1)).c_str());
  oled.drawStr(0, 40, ("OutdoorTemp: "    + String(outdoorTemp, 1)).c_str());
  oled.drawStr(0, 50, ("Rain 1h: "        + String(rain1h, 1) + " mm").c_str());
  oled.drawStr(0, 60, ("Rain 24h: "       + String(rain24h, 1) + " mm").c_str());
  if (isRaining)
    oled.drawXBMP(120, 0, 8, 8, rain_drop_bmp);
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
