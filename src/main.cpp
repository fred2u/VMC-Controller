#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include "wifi.h"

#define PIN_LED 16
#define PIN_DEFAULT 2 // 4
#define PIN_AUTO 4 // 5
#define PIN_LOW 5 // 1
#define PIN_MEDIUM 15 // 2
#define PIN_HIGH 12 // 3
#define PIN_MAX 14 // 6

const char *mode_unknown = "Unknown";
const char *mode_default = "Default";
const char *mode_auto = "Automatique";
const char *mode_low = "Low";
const char *mode_medium = "Medium";
const char *mode_high = "High";
const char *mode_max1 = "Max 15 min";
const char *mode_max2 = "Max 30 min";
const char *mode_max3 = "Max 60 min";

const int command_delay = 700;

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

AsyncWebServer webServer(80);
String _command = "";
String _mode = "";
String _tempMode = "";
time_t _tempModeExpire = 0;

void setup()
{
  // Serial port 
  Serial.begin(115200);
  Serial.println();
  Serial.println("Open Serial");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.hostname("vmc-controller");
  WiFi.begin(ssid, password);
  static WiFiEventHandler onStationModeGotIPHandler = WiFi.onStationModeGotIP(onStationModeGotIP);
  
  // GPIO
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);  

  pinMode(PIN_DEFAULT, OUTPUT);
  digitalWrite(PIN_DEFAULT, LOW);
  pinMode(PIN_AUTO, OUTPUT);
  digitalWrite(PIN_AUTO, LOW);
  pinMode(PIN_LOW, OUTPUT);
  digitalWrite(PIN_LOW, LOW);
  pinMode(PIN_MEDIUM, OUTPUT);
  digitalWrite(PIN_MEDIUM, LOW);
  pinMode(PIN_HIGH, OUTPUT);
  digitalWrite(PIN_HIGH, LOW);
  pinMode(PIN_MAX, OUTPUT);
  digitalWrite(PIN_MAX, LOW);

  // LittleFS
  if(LittleFS.begin())
  {
    Dir root = LittleFS.openDir("/");
    while(root.next())
    {
      Serial.println("- " + root.fileName());
    }
  }

  // Web Server
  webServer.on("/", handleDefault);
  webServer.on("/index.html", handleDefault);
  webServer.on("/script.js", handleJS);
  webServer.on("/milligram.min.css", handleCSS);
  webServer.on("/style.css", handleCSS);
  webServer.on("/default", handleCommand);
  webServer.on("/auto", handleCommand);
  webServer.on("/low", handleCommand);
  webServer.on("/medium", handleCommand);
  webServer.on("/high", handleCommand);
  webServer.on("/max1", handleCommand);
  webServer.on("/max2", handleCommand);
  webServer.on("/max3", handleCommand);
  webServer.on("/mode", handleMode);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
}

void loop()
{
  if(_tempModeExpire > 0 && _tempModeExpire < time(0))
  {
    _tempModeExpire = 0;
    _tempMode = "";    
  }

  if(_command == "")
  {
    return;
  }

  digitalWrite(PIN_LED, LOW); 

  if (_command == "default")
  {
    sendCommand(PIN_DEFAULT, mode_default);
  }
  else if (_command == "auto")
  {
    sendCommand(PIN_AUTO, mode_auto);
  }
  else if (_command == "low")
  {
    sendCommand(PIN_LOW, mode_low);
  }
  else if (_command == "medium")
  {
    sendCommand(PIN_MEDIUM, mode_medium);
  }
  else if (_command == "high")
  {
    sendCommand(PIN_HIGH, mode_high);
  }
  else if (_command == "max1")
  {
    sendTempCommand(PIN_MAX, 1, mode_max1, 15);
  }
  else if (_command == "max2")
  {
    sendTempCommand(PIN_MAX, 2, mode_max2, 30);
  }
  else if (_command == "max3")
  {
    sendTempCommand(PIN_MAX, 3, mode_max3, 60);
  }

  _command = "";

  digitalWrite(PIN_LED, HIGH);  
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
  _command = request->url();
  _command.remove(0, 1);
  request->send(200);
}

void handleMode(AsyncWebServerRequest *request)
{
  if(_tempMode == "")
  {
    request->send(200, "text/plain", _mode);
  }
  else
  {
    request->send(200, "text/plain", _tempMode);
  }
}

void sendCommand(int pin, String mode)
{
  _tempModeExpire = 0;
  _tempMode = "";    
  _mode = mode;
  sendCommand(pin);
}

void sendTempCommand(int pin, int count, String tempMode, int tempDelay)
{
  _tempMode = tempMode;
  _tempModeExpire = time(0) + (60 * tempDelay);
  
  for (int i = 1; i < count; i++)
  {
    sendCommand(pin);
    delay(command_delay);
  }  
  sendCommand(pin);
}

void sendCommand(int pin)
{
  digitalWrite(pin, HIGH);
  delay(command_delay);
  digitalWrite(pin, LOW);
}
