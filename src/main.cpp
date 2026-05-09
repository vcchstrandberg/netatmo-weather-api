// test only
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
  oled.clearBuffer();
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
