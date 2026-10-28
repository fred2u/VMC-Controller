#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include "LittleFS.h"

// include/wifi.h is optional: the Wi-Fi network is picked from the setup
// portal. The file can hold the OTA password and a fallback network.
#if __has_include("wifi.h")
#include "wifi.h"
#endif

// Updates over Wi-Fi need OTA_PASSWORD in include/wifi.h.
#ifndef OTA_PASSWORD
#warning "OTA_PASSWORD is not defined in include/wifi.h: updates over Wi-Fi are disabled"
#endif

#define PIN_LED 16
#define PIN_DEFAULT 12 // 4
#define PIN_AUTO 5 // 5
#define PIN_LOW 2 // 1
#define PIN_MEDIUM 15 // 2
#define PIN_HIGH 4 // 3
#define PIN_MAX 14 // 6

const char *mode_default = "Default";
const char *mode_auto = "Auto";
const char *mode_low = "Low";
const char *mode_medium = "Medium";
const char *mode_high = "High";
const char *mode_max1 = "Max 15 min";
const char *mode_max2 = "Max 30 min";
const char *mode_max3 = "Max 60 min";

const char *command_default = "/default";
const char *command_auto = "/auto";
const char *command_low = "/low";
const char *command_medium = "/medium";
const char *command_high = "/high";
const char *command_max1 = "/max1";
const char *command_max2 = "/max2";
const char *command_max3 = "/max3";

// Access point opened when the ESP cannot join a network, and how long
// the ESP waits for the network and for someone to use the portal.
const char *setup_ap_name = "VMC-Setup";
const int wifi_connect_timeout = 30; // seconds
const int setup_portal_timeout = 180; // seconds

const int between_command_delay = 600;
const int command_delay = 500;

void onStationModeGotIP(const WiFiEventStationModeGotIP& event);
void handleDefault(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);
void handleJS(AsyncWebServerRequest *request);
void handleCSS(AsyncWebServerRequest *request);
void handleCommand(AsyncWebServerRequest *request);
void handleMode(AsyncWebServerRequest *request);

void sendCommand(int pin, String mode);
void sendTempCommand(int pin, int count, String tempMode, int tempDelay);
void sendCommand(int pin);
void handleOTA();
void setupWiFi();

AsyncWebServer webServer(80);
DNSServer dnsServer;
String command = "";
String currentMode = "";
String tempMode = "";
time_t tempModeExpire = 0;
bool otaStarted = false;

void setup()
{
  // Serial port 
  Serial.begin(115200);
  Serial.println();
  Serial.println("Open Serial");

  // GPIO
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);  

  pinMode(PIN_DEFAULT, OUTPUT);
  digitalWrite(PIN_DEFAULT, HIGH);  
  pinMode(PIN_AUTO, OUTPUT);
  digitalWrite(PIN_AUTO, HIGH);  
  pinMode(PIN_LOW, OUTPUT);
  digitalWrite(PIN_LOW, HIGH);  
  pinMode(PIN_MEDIUM, OUTPUT);
  digitalWrite(PIN_MEDIUM, HIGH);  
  pinMode(PIN_HIGH, OUTPUT);
  digitalWrite(PIN_HIGH, HIGH);  
  pinMode(PIN_MAX, OUTPUT);
  digitalWrite(PIN_MAX, HIGH);  

  // LittleFS
  if(LittleFS.begin())
  {
    Dir root = LittleFS.openDir("/");
    while(root.next())
    {
      Serial.println("- " + root.fileName());
    }
  }

  // WiFi, before the routes: the setup portal clears the web server.
  setupWiFi();

  // Web Server
  webServer.on("/", handleDefault);
  webServer.on("/index.html", handleDefault);
  webServer.on("/script.js", handleJS);
  webServer.on("/milligram.min.css", handleCSS);
  webServer.on("/style.css", handleCSS);
  webServer.on(command_default, handleCommand);
  webServer.on(command_auto, handleCommand);
  webServer.on(command_low, handleCommand);
  webServer.on(command_medium, handleCommand);
  webServer.on(command_high, handleCommand);
  webServer.on(command_max1, handleCommand);
  webServer.on(command_max2, handleCommand);
  webServer.on(command_max3, handleCommand);
  webServer.on("/mode", handleMode);
  webServer.onNotFound(handleNotFound);
  webServer.begin();

  command = command_medium;
  delay(2000);
}

void loop()
{
  handleOTA();

  if(tempModeExpire > 0 && tempModeExpire < time(0))
  {
    tempModeExpire = 0;
    tempMode = "";    
  }

  if(command == "")
  {
    return;
  }

  Serial.println(command);

  digitalWrite(PIN_LED, LOW); 

  if (command == command_default)
  {
    sendCommand(PIN_DEFAULT, mode_default);
  }
  else if (command == command_auto)
  {
    sendCommand(PIN_AUTO, mode_auto);
  }
  else if (command == command_low)
  {
    sendCommand(PIN_LOW, mode_low);
  }
  else if (command == command_medium)
  {
    sendCommand(PIN_MEDIUM, mode_medium);
  }
  else if (command == command_high)
  {
    sendCommand(PIN_HIGH, mode_high);
  }
  else if (currentMode != mode_high)
  { 
    if (command == command_max1)
    {
      sendTempCommand(PIN_MAX, 1, mode_max1, 15);
    }
    else if (command == command_max2)
    {
      sendTempCommand(PIN_MAX, 2, mode_max2, 30);
    }
    else if (command == command_max3)
    {
      sendTempCommand(PIN_MAX, 3, mode_max3, 60);
    }
  }

  command = ""; 

  digitalWrite(PIN_LED, HIGH);  
}

// Joins the saved Wi-Fi network. If it cannot be reached (first boot, new
// router...), opens the VMC-Setup access point with a portal to pick the
// network. The portal keeps retrying the saved network every few seconds and
// closes after setup_portal_timeout; the ESP then keeps retrying on its own.
void setupWiFi()
{
  WiFi.hostname("vmc");
  WiFi.mode(WIFI_STA);
  static WiFiEventHandler onStationModeGotIPHandler = WiFi.onStationModeGotIP(onStationModeGotIP);

#ifdef WIFI_SSID
  // Nothing saved yet: start with the network from include/wifi.h.
  if(WiFi.SSID() == "")
  {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
#endif

  AsyncWiFiManager wifiManager(&webServer, &dnsServer);
  wifiManager.setConnectTimeout(wifi_connect_timeout);
  wifiManager.setConfigPortalTimeout(setup_portal_timeout);
  if(!wifiManager.autoConnect(setup_ap_name))
  {
    Serial.println("No Wi-Fi network, retrying in the background");
    WiFi.mode(WIFI_STA);
    WiFi.begin();
  }
}

void onStationModeGotIP(const WiFiEventStationModeGotIP& event)
{
 	Serial.println("\n");
 	Serial.println("Connection established");
 	Serial.print("Address IP: ");
 	Serial.println(WiFi.localIP());
}

void handleDefault(AsyncWebServerRequest *request)
{
  request->send(LittleFS, "/index.html", "text/html");
}

void handleNotFound(AsyncWebServerRequest *request)
{
 	Serial.println("NotFound: " + request->url());
  request->send(404);
}

void handleJS(AsyncWebServerRequest *request)
{
  request->send(LittleFS, request->url(), "text/javascript");
}

void handleCSS(AsyncWebServerRequest *request)
{
  request->send(LittleFS, request->url(), "text/css");
}

void handleCommand(AsyncWebServerRequest *request)
{
  command = request->url();
  request->send(200);
}

void handleMode(AsyncWebServerRequest *request)
{
  if(tempMode == "")
  {
    request->send(200, "text/plain", currentMode);
  }
  else
  {
    request->send(200, "text/plain", tempMode);
  }
}

void sendCommand(int pin, String mode)
{
  tempModeExpire = 0;
  tempMode = "";    
  currentMode = mode;
  sendCommand(pin);
}

void sendTempCommand(int pin, int count, String mode, int tempDelay)
{
  tempMode = mode;
  tempModeExpire = time(0) + (60 * tempDelay);
  
  for (int i = 1; i < count; i++)
  {
    sendCommand(pin);
    delay(between_command_delay);
  }  
  sendCommand(pin);
}

void sendCommand(int pin)
{ 
  digitalWrite(pin, LOW);
  delay(command_delay);
  digitalWrite(pin, HIGH);
}

// Starts OTA once Wi-Fi is up, then listens for updates on every loop.
void handleOTA()
{
#ifdef OTA_PASSWORD
  if(!otaStarted)
  {
    if(WiFi.status() != WL_CONNECTED)
    {
      return;
    }
    ArduinoOTA.setHostname("vmc");
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]()
    {
      // The web interface image replaces the file system: unmount it first.
      if(ArduinoOTA.getCommand() == U_FS)
      {
        LittleFS.end();
      }
      Serial.println("OTA update started");
    });
    ArduinoOTA.begin();
    otaStarted = true;
  }
  ArduinoOTA.handle();
#endif
}
