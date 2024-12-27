#include <Arduino.h>
#include "WiFiS3.h"
#include "WiFiSSLClient.h"
#include "IPAddress.h"
#include <ArduinoJson.h>
#include "Arduino_LED_Matrix.h"
#include "arduino_secrets.h"

// Pre-obtained access token for Netatmo API
const char *accessToken = SECRET_TOKEN;

char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password (use for WPA, or use as key for WEP)

int status = WL_IDLE_STATUS;
char server[] = "api.netatmo.com"; // Netatmo API server

WiFiSSLClient client;
ArduinoLEDMatrix matrix;

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

uint8_t frame[8][12] = {
    {0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0},
    {0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
    {0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0},
    {0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0},
    {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0}};

void printWifiStatus();
void fetchWeatherData();
void parseWeatherData(const String &jsonResponse);
String cleanResponse(String response);

void setup()
{
  Serial.begin(115200);
  matrix.begin();

  while (!Serial)
  {
    ; // wait for serial port to connect. Needed for native USB port only
  }

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
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }

  printWifiStatus();

  Serial.println("\nStarting connection to server...");
  Serial.print("Access Token: ");
  Serial.println(accessToken);

  client.setCACert(netatmo_ca);
  if (client.connect(server, 443))
  {
    Serial.println("Connected to Netatmo API server");
    fetchWeatherData();
  }
  else
  {
    Serial.println("Connection to server failed");
  }
}

void fetchWeatherData()
{
  // Send a GET request to fetch weather data
  client.println("GET /api/getstationsdata HTTP/1.1");
  client.println("Host: api.netatmo.com");
  client.print("Authorization: Bearer ");
  client.println(accessToken);
  client.println("Connection: close");
  client.println();

  // Wait for the response and print it
  delay(1000);
  String response = "";

  while (client.available())
  {
    char c = client.read();
    response += c;
  }

  Serial.println("Weather Data Response:");
  Serial.println(response);

  Serial.println("Raw Token Refresh Response:");
  Serial.println(response);

  // Clean the response to remove garbage data
  String cleanJson = cleanResponse(response);
  if (cleanJson == "")
  {
    Serial.println("Error: Unable to clean the response.");
    return;
  }

  Serial.println("Cleaned JSON Response:");
  Serial.println(cleanJson);

  parseWeatherData(cleanJson);
  // parseWeatherData(response);
}

void loop()
{

  // Do nothing in the loop
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

// Function to parse JSON response
void parseWeatherData(const String &jsonResponse)
{
  // Allocate a JSON document
  // Adjust the size based on your JSON structure
  StaticJsonDocument<1024> doc;

  // Deserialize the JSON response
  DeserializationError error = deserializeJson(doc, jsonResponse);

  if (error)
  {
    Serial.print("JSON parsing failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Navigate through the JSON structure to extract data
  JsonObject body = doc["body"];
  JsonArray devices = body["devices"];
  if (!devices.isNull() && devices.size() > 0)
  {
    JsonObject mainDevice = devices[0];
    JsonArray modules = mainDevice["modules"];

    // Extract Indoor Temperature and Humidity
    float indoorTemp = mainDevice["dashboard_data"]["Temperature"];
    int indoorHumidity = mainDevice["dashboard_data"]["Humidity"];
    Serial.print("Indoor Temperature: ");
    Serial.println(indoorTemp);
    Serial.print("Indoor Humidity: ");
    Serial.println(indoorHumidity);

    // Extract Outdoor Temperature (from the first module)
    if (!modules.isNull() && modules.size() > 0)
    {
      JsonObject outdoorModule = modules[0];
      float outdoorTemp = outdoorModule["dashboard_data"]["Temperature"];
      Serial.print("Outdoor Temperature: ");
      Serial.println(outdoorTemp);
    }
  }
  else
  {
    Serial.println("No devices found in the response.");
  }
}

String cleanResponse(String response)
{
  // Find the start of the JSON object
  int jsonStart = response.indexOf('{');
  if (jsonStart == -1)
  {
    Serial.println("Error: No JSON object found in the response.");
    return "";
  }

  // Extract the JSON part of the response
  return response.substring(jsonStart);
}
