#include <Arduino.h>
#include "WiFiS3.h"
#include "WiFiSSLClient.h"
#include "IPAddress.h"
#include <ArduinoJson.h>
#include "Arduino_LED_Matrix.h"
#include <U8g2lib.h>
#include <Arduino_UnifiedStorage.h>

String accessToken = "";
String refreshToken = "";
String wifiSSID = "";
String wifiPass = "";
String clientId = "";
String clientSecret = "";

int counter = 0;
int status = WL_IDLE_STATUS;
char server[] = "api.netatmo.com";

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
WiFiSSLClient client;
ArduinoLEDMatrix matrix;
QSPIFlash flashStorage;

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

void printWifiStatus();
void fetchWeatherData();
void parseWeatherData2(const String &jsonResponse);
void refreshAccessToken();
String cleanResponse(String response);
void loadConfig();
void saveTokens();
void enterConfigMode();

void setup()
{
  oled.begin();
  oled.setFont(u8g2_font_ncenB08_tr);
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }

  if (!flashStorage.begin())
  {
    Serial.println("Flash: failed to mount");
  }

  loadConfig();

  if (wifiSSID.length() == 0)
  {
    enterConfigMode();
  }

  Serial.println("Starting connection to server...");

  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("Communication with WiFi module failed!");
    while (true)
      ;
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION)
  {
    Serial.println("Please upgrade the firmware");
  }

  while (status != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(wifiSSID);
    status = WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
    delay(10000);
  }
  client.setCACert(netatmo_ca);
}

void loop()
{
  if (client.connect(server, 443))
  {
    Serial.println("Connected to Netatmo API server");
    refreshAccessToken();
  }
  else
  {
    Serial.println("Connection to server failed");
  }

  if (client.connect(server, 443))
  {
    Serial.println("Connected to Netatmo API server");
    fetchWeatherData();
  }
  else
  {
    Serial.println("Connection to server failed");
  }
  delay(60000);
}

void loadConfig()
{
  Folder root = flashStorage.getRootFolder();
  UFile f = root.openFile("config.json", FileMode::READ);
  if (!f.available())
  {
    Serial.println("Config: no config.json found");
    f.close();
    return;
  }
  String content = f.readAsString();
  f.close();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, content);
  if (error)
  {
    Serial.print("Config: parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  wifiSSID     = doc["ssid"]          | "";
  wifiPass     = doc["pass"]          | "";
  clientId     = doc["client_id"]     | "";
  clientSecret = doc["client_secret"] | "";
  accessToken  = doc["access_token"]  | "";
  refreshToken = doc["refresh_token"] | "";

  Serial.println("Config loaded from flash");
}

void saveTokens()
{
  // Read existing config so we preserve ssid/pass/client credentials
  Folder root = flashStorage.getRootFolder();
  UFile rf = root.openFile("config.json", FileMode::READ);
  String content = rf.available() ? rf.readAsString() : "{}";
  rf.close();

  JsonDocument doc;
  deserializeJson(doc, content);
  doc["access_token"]  = accessToken;
  doc["refresh_token"] = refreshToken;

  String output;
  serializeJson(doc, output);

  UFile wf = root.createFile("config.json", FileMode::WRITE);
  wf.write(output);
  wf.close();
  Serial.println("Tokens saved to config.json");
}

// Exposes the QSPI flash as a USB drive so the user can create config.json.
// Does not return — board must be reset after the file is created.
void enterConfigMode()
{
  Serial.println("No config found. Connect via USB, create config.json, then reset the board.");

  oled.clearDisplay();
  oled.drawStr(0, 10, "No config found!");
  oled.drawStr(0, 25, "Connect USB,");
  oled.drawStr(0, 35, "create config.json,");
  oled.drawStr(0, 45, "then reset board.");
  oled.sendBuffer();

  flashStorage.unmount();

  // Expose QSPI flash as a USB mass storage drive.
  // The Arduino_UnifiedStorage library handles wear-levelled LittleFS underneath.
  // If this method name differs in your library version, check:
  //   https://github.com/arduino-libraries/Arduino_UnifiedStorage
  if (!flashStorage.beginMassStorage())
  {
    Serial.println("Failed to start USB mass storage mode");
  }

  while (true)
  {
    delay(100);
  }
}

void fetchWeatherData()
{
  client.println("GET /api/getstationsdata HTTP/1.1");
  client.println("Host: api.netatmo.com");
  client.print("Authorization: Bearer ");
  client.println(accessToken);
  client.println("Connection: close");
  client.println();

  delay(1000);
  String response = "";
  while (client.available())
  {
    char c = client.read();
    response += c;
  }
  client.stop();
  Serial.println(response);

  String cleanJson = cleanResponse(response);
  if (cleanJson == "")
  {
    Serial.println("Error: Unable to clean the response.");
    return;
  }

  Serial.println("Cleaned JSON Response:");
  Serial.println(cleanJson);
  parseWeatherData2(cleanJson);
}

String cleanResponse(String response)
{
  int jsonStart = response.indexOf('{');
  if (jsonStart == -1)
  {
    Serial.println("Error: No JSON object found in the response.");
    return "";
  }
  return response.substring(jsonStart);
}

void parseWeatherData2(const String &jsonResponse)
{
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, jsonResponse);
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  JsonObject body_devices_0 = doc["body"]["devices"][0];
  const char *body_devices_0_id = body_devices_0["_id"];
  long body_devices_0_date_setup = body_devices_0["date_setup"];
  long body_devices_0_last_setup = body_devices_0["last_setup"];
  const char *body_devices_0_type = body_devices_0["type"];
  long body_devices_0_last_status_store = body_devices_0["last_status_store"];
  const char *body_devices_0_module_name = body_devices_0["module_name"];
  int body_devices_0_firmware = body_devices_0["firmware"];
  int body_devices_0_wifi_status = body_devices_0["wifi_status"];
  bool body_devices_0_reachable = body_devices_0["reachable"];
  bool body_devices_0_co2_calibrating = body_devices_0["co2_calibrating"];

  JsonArray body_devices_0_data_type = body_devices_0["data_type"];
  const char *body_devices_0_data_type_0 = body_devices_0_data_type[0];
  const char *body_devices_0_data_type_1 = body_devices_0_data_type[1];
  const char *body_devices_0_data_type_2 = body_devices_0_data_type[2];
  const char *body_devices_0_data_type_3 = body_devices_0_data_type[3];
  const char *body_devices_0_data_type_4 = body_devices_0_data_type[4];
  JsonObject body_devices_0_place = body_devices_0["place"];
  int body_devices_0_place_altitude = body_devices_0_place["altitude"];
  const char *body_devices_0_place_city = body_devices_0_place["city"];
  const char *body_devices_0_place_country = body_devices_0_place["country"];
  const char *body_devices_0_place_timezone = body_devices_0_place["timezone"];
  double body_devices_0_place_location_0 = body_devices_0_place["location"][0];
  double body_devices_0_place_location_1 = body_devices_0_place["location"][1];
  const char *body_devices_0_station_name = body_devices_0["station_name"];
  const char *body_devices_0_home_id = body_devices_0["home_id"];
  const char *body_devices_0_home_name = body_devices_0["home_name"];

  JsonObject body_devices_0_dashboard_data = body_devices_0["dashboard_data"];
  long body_devices_0_dashboard_data_time_utc = body_devices_0_dashboard_data["time_utc"];
  float body_devices_0_dashboard_data_Temperature = body_devices_0_dashboard_data["Temperature"];
  int body_devices_0_dashboard_data_CO2 = body_devices_0_dashboard_data["CO2"];
  int body_devices_0_dashboard_data_Humidity = body_devices_0_dashboard_data["Humidity"];
  int body_devices_0_dashboard_data_Noise = body_devices_0_dashboard_data["Noise"];
  float body_devices_0_dashboard_data_Pressure = body_devices_0_dashboard_data["Pressure"];
  float body_devices_0_dashboard_data_AbsolutePressure = body_devices_0_dashboard_data["AbsolutePressure"];
  float body_devices_0_dashboard_data_min_temp = body_devices_0_dashboard_data["min_temp"];
  float body_devices_0_dashboard_data_max_temp = body_devices_0_dashboard_data["max_temp"];
  long body_devices_0_dashboard_data_date_max_temp = body_devices_0_dashboard_data["date_max_temp"];
  long body_devices_0_dashboard_data_date_min_temp = body_devices_0_dashboard_data["date_min_temp"];
  const char *body_devices_0_dashboard_data_temp_trend = body_devices_0_dashboard_data["temp_trend"];
  const char *body_devices_0_dashboard_data_pressure_trend = body_devices_0_dashboard_data["pressure_trend"];

  JsonObject body_devices_0_modules_0 = body_devices_0["modules"][0];
  const char *body_devices_0_modules_0_id = body_devices_0_modules_0["_id"];
  const char *body_devices_0_modules_0_type = body_devices_0_modules_0["type"];
  const char *body_devices_0_modules_0_module_name = body_devices_0_modules_0["module_name"];
  long body_devices_0_modules_0_last_setup = body_devices_0_modules_0["last_setup"];

  const char *body_devices_0_modules_0_data_type_0 = body_devices_0_modules_0["data_type"][0];
  const char *body_devices_0_modules_0_data_type_1 = body_devices_0_modules_0["data_type"][1];

  int body_devices_0_modules_0_battery_percent = body_devices_0_modules_0["battery_percent"];
  bool body_devices_0_modules_0_reachable = body_devices_0_modules_0["reachable"];
  int body_devices_0_modules_0_firmware = body_devices_0_modules_0["firmware"];
  long body_devices_0_modules_0_last_message = body_devices_0_modules_0["last_message"];
  long body_devices_0_modules_0_last_seen = body_devices_0_modules_0["last_seen"];
  int body_devices_0_modules_0_rf_status = body_devices_0_modules_0["rf_status"];
  int body_devices_0_modules_0_battery_vp = body_devices_0_modules_0["battery_vp"];

  JsonObject body_devices_0_modules_0_dashboard_data = body_devices_0_modules_0["dashboard_data"];
  long body_devices_0_modules_0_dashboard_data_time_utc = body_devices_0_modules_0_dashboard_data["time_utc"];
  int body_devices_0_modules_0_dashboard_data_Temperature = body_devices_0_modules_0_dashboard_data["Temperature"];
  int body_devices_0_modules_0_dashboard_data_Humidity = body_devices_0_modules_0_dashboard_data["Humidity"];
  float body_devices_0_modules_0_dashboard_data_min_temp = body_devices_0_modules_0_dashboard_data["min_temp"];
  float body_devices_0_modules_0_dashboard_data_max_temp = body_devices_0_modules_0_dashboard_data["max_temp"];
  long body_devices_0_modules_0_dashboard_data_date_max_temp = body_devices_0_modules_0_dashboard_data["date_max_temp"];
  long body_devices_0_modules_0_dashboard_data_date_min_temp = body_devices_0_modules_0_dashboard_data["date_min_temp"];
  const char *body_devices_0_modules_0_dashboard_data_temp_trend = body_devices_0_modules_0_dashboard_data["temp_trend"];

  const char *body_user_mail = doc["body"]["user"]["mail"];

  JsonObject body_user_administrative = doc["body"]["user"]["administrative"];
  const char *body_user_administrative_lang = body_user_administrative["lang"];
  const char *body_user_administrative_reg_locale = body_user_administrative["reg_locale"];
  const char *body_user_administrative_country = body_user_administrative["country"];
  int body_user_administrative_unit = body_user_administrative["unit"];
  int body_user_administrative_windunit = body_user_administrative["windunit"];
  int body_user_administrative_pressureunit = body_user_administrative["pressureunit"];
  int body_user_administrative_feel_like_algo = body_user_administrative["feel_like_algo"];

  const char *apiStatus = doc["status"];
  double time_exec = doc["time_exec"];
  long time_server = doc["time_server"];

  float indoorTemp = body_devices_0_dashboard_data["Temperature"];
  int indoorHumidity = body_devices_0_dashboard_data["Humidity"];
  float airPressure = body_devices_0_dashboard_data["Pressure"];
  float outTemperature = body_devices_0_modules_0_dashboard_data["Temperature"];

  Serial.print("Indoor Temperature: ");
  Serial.println(indoorTemp);
  Serial.println("Clear Display");
  oled.clearDisplay();
  String temp = String("IndoorTemp: ");
  temp.concat(indoorTemp);
  oled.drawStr(0, 10, temp.c_str());
  String hum = String("IndoorHumidity: ");
  hum.concat(indoorHumidity);
  oled.drawStr(0, 20, hum.c_str());
  String airP = String("AirPressure: ");
  airP.concat(airPressure);
  oled.drawStr(0, 30, airP.c_str());
  String outTemp = String("OutdoorTemp: ");
  outTemp.concat(outTemperature);
  oled.drawStr(0, 40, outTemp.c_str());
  oled.sendBuffer();
  Serial.print("Indoor Humidity: ");
  Serial.println(indoorHumidity);
  Serial.print("Air Pressure: ");
  Serial.println(airPressure);
  Serial.print("Outdoor Temperature: ");
  Serial.println(outTemperature);
  counter++;
}

void refreshAccessToken()
{
  String postData = "grant_type=refresh_token&refresh_token=" + refreshToken +
                    "&client_id=" + clientId +
                    "&client_secret=" + clientSecret;

  client.println("POST /oauth2/token HTTP/1.1");
  client.println("Host: api.netatmo.com");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: ");
  client.println(postData.length());
  client.println("Connection: close");
  client.println();
  client.println(postData);
  delay(1000);

  String response = "";
  while (client.available())
  {
    char c = client.read();
    response += c;
  }
  client.stop();

  int jsonStart = response.indexOf('{');
  if (jsonStart == -1)
  {
    Serial.println("Error: No JSON object found in the response.");
    return;
  }

  String jsonResponse = response.substring(jsonStart);
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonResponse);
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  const char *newAccessToken = doc["access_token"];
  const char *newRefreshToken = doc["refresh_token"];
  if (newAccessToken)
  {
    accessToken = String(newAccessToken);
    refreshToken = String(newRefreshToken);
    Serial.println("Tokens refreshed successfully");
    saveTokens();
  }
  else
  {
    Serial.println("Error: Unable to refresh access token");
  }
}

void printWifiStatus()
{
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
