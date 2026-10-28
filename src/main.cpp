#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include "LittleFS.h"

// The Wi-Fi network is picked from the setup portal. Updates over Wi-Fi
// need OTA_PASSWORD, set from the VMC_OTA_PASSWORD environment variable
// at build time by scripts/ota_password.py.
#ifndef OTA_PASSWORD
#warning "VMC_OTA_PASSWORD is not set: updates over Wi-Fi are disabled"
#endif

// Name on the network (DHCP, http://vmc/, OTA as vmc.local). The test
// board is built with another name (see platformio.ini) so that it can sit
// on the same Wi-Fi as the production board without taking its name.
#ifndef VMC_HOSTNAME
#define VMC_HOSTNAME "vmc"
#endif

// Firmware version, from the Git tags (scripts/version.py), for example
// "v1.1" or "v1.1-3-g1a2b3c4". Answered on /version and /info.
#ifndef VMC_VERSION
#define VMC_VERSION "unknown"
#endif

#define PIN_LED 16
#define PIN_DEFAULT 12 // 4
#define PIN_AUTO 5 // 5
#define PIN_LOW 2 // 1
#define PIN_MEDIUM 15 // 2
#define PIN_HIGH 4 // 3
#define PIN_MAX 14 // 6

// Every command the web server accepts. Its position in this list is the
// number stored as the waiting command. Max presses the Timer button 1, 2 or 3
// times: the unit then stays in High for 15, 30 or 60 minutes.
struct Command
{
  const char *url;
  const char *mode;    // answered on /mode while it is the last mode sent
  uint8_t pin;
  uint8_t presses;
  uint8_t maxMinutes;  // 0: a normal mode, kept until the next command
};

const Command commands[] = {
  {"/default", "Default", PIN_DEFAULT, 1, 0},
  {"/auto", "Auto", PIN_AUTO, 1, 0},
  {"/low", "Low", PIN_LOW, 1, 0},
  {"/medium", "Medium", PIN_MEDIUM, 1, 0},
  {"/high", "High", PIN_HIGH, 1, 0},
  {"/max1", "Max 15 min", PIN_MAX, 1, 15},
  {"/max2", "Max 30 min", PIN_MAX, 2, 30},
  {"/max3", "Max 60 min", PIN_MAX, 3, 60},
};
const int8_t command_count = sizeof(commands) / sizeof(commands[0]);
const int8_t no_command = -1;
const int8_t command_medium = 3;
const int8_t command_high = 4;

// Access point opened when the ESP cannot join a network, and how long
// the ESP waits for the network and for someone to use the portal.
const char *setup_ap_name = "VMC-Setup";
const int wifi_connect_timeout = 30; // seconds
const int setup_portal_timeout = 180; // seconds

const unsigned long command_delay = 500;         // ms a button is held down
const unsigned long between_command_delay = 600; // ms released after a press


void onStationModeGotIP(const WiFiEventStationModeGotIP& event);
void handleDefault(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);
void handleJS(AsyncWebServerRequest *request);
void handleCSS(AsyncWebServerRequest *request);
void handleCommand(AsyncWebServerRequest *request);
void handleMode(AsyncWebServerRequest *request);
void handleVersion(AsyncWebServerRequest *request);
void handleInfo(AsyncWebServerRequest *request);

int requestCommand(int8_t command);
void runSender();
void startCommand(int8_t command);
void expireMaxMode();
void handleOTA();
void setupWiFi();

AsyncWebServer webServer(80);
DNSServer dnsServer;

// Command waiting to be sent, written by the web server, taken by loop().
// A command received while another one is being sent waits here, and a
// newer one replaces it: only the last command matters. On the ESP8266 the
// web server only runs between two loop() calls, so no lock is needed.
volatile int8_t pendingCommand = no_command;

// What /mode answers: the last normal mode sent, and the Max mode while
// its timer runs. Written by loop(), read by the web server.
volatile int8_t currentMode = no_command;
volatile int8_t maxMode = no_command;
uint32_t maxModeStart = 0;  // millis() when the Max mode was sent
uint32_t maxModeLength = 0; // ms

// The command being sent, one step at a time so that loop() never waits:
// press a button, release it, pause, press again for Max...
enum SendStep { SEND_IDLE, SEND_PRESSED, SEND_PAUSE };
SendStep sendStep = SEND_IDLE;
int8_t sending = no_command;
uint8_t pressesLeft = 0;
uint32_t stepStart = 0;
uint32_t stepLength = 0;

bool otaStarted = false;

void setup()
{
  // Serial port 
  Serial.begin(115200);
  Serial.println();
  Serial.println("Open Serial");
  Serial.println("VMC Controller " VMC_VERSION);

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
  webServer.on("/style.css", handleCSS);
  for(int8_t i = 0; i < command_count; i++)
  {
    webServer.on(commands[i].url, handleCommand);
  }
  webServer.on("/mode", handleMode);
  webServer.on("/version", handleVersion);
  webServer.on("/info", handleInfo);
  webServer.onNotFound(handleNotFound);
  webServer.begin();

  requestCommand(command_medium);
  delay(2000);
}

void loop()
{
  handleOTA();
  expireMaxMode();
  runSender();
}

// Sets the command to send next, in place of any command still waiting.
// Answers the HTTP code for the request: 200, or 409 for a Max in High,
// which the unit ignores. A waiting command is replaced, never added to, so
// the mode to check is the last one sent.
int requestCommand(int8_t command)
{
  if(commands[command].maxMinutes > 0 && currentMode == command_high)
  {
    return 409;
  }
  pendingCommand = command;
  return 200;
}

// Moves the command being sent one step forward when its time is up, or
// starts the waiting command. Never waits.
void runSender()
{
  uint32_t now = millis();
  if(sendStep != SEND_IDLE && now - stepStart < stepLength)
  {
    return;
  }

  if(sendStep == SEND_PRESSED)
  {
    // Release, then pause, also after the last press, so that two commands
    // in a row are seen by the remote as two presses.
    digitalWrite(commands[sending].pin, HIGH);
    pressesLeft--;
    sendStep = SEND_PAUSE;
    stepStart = now;
    stepLength = between_command_delay;
    return;
  }

  if(sendStep == SEND_PAUSE && pressesLeft == 0)
  {
    sending = no_command;
    sendStep = SEND_IDLE;
    digitalWrite(PIN_LED, HIGH);
  }

  if(sendStep == SEND_IDLE)
  {
    int8_t command = pendingCommand;
    if(command == no_command)
    {
      return;
    }
    pendingCommand = no_command;
    startCommand(command);
  }

  // Press the button (first press, or the next one for Max).
  digitalWrite(commands[sending].pin, LOW);
  sendStep = SEND_PRESSED;
  stepStart = now;
  stepLength = command_delay;
}

// Updates the mode answered on /mode and lights the LED, before the first
// press of a command.
void startCommand(int8_t command)
{
  const Command &c = commands[command];
  Serial.println(c.url);
  digitalWrite(PIN_LED, LOW);

  if(c.maxMinutes > 0)
  {
    maxModeStart = millis();
    maxModeLength = c.maxMinutes * 60000UL;
    maxMode = command;
  }
  else
  {
    maxMode = no_command;
    currentMode = command;
  }

  sending = command;
  pressesLeft = c.presses;
}

// Back to the last normal mode once the Max timer is over. millis() counts
// from boot and needs no clock sync; the subtraction stays right when it
// wraps around after 49 days.
void expireMaxMode()
{
  if(maxMode != no_command && (uint32_t)millis() - maxModeStart > maxModeLength)
  {
    maxMode = no_command;
  }
}

// Joins the saved Wi-Fi network. If it cannot be reached (first boot, new
// router...), opens the VMC-Setup access point with a portal to pick the
// network. The portal keeps retrying the saved network every few seconds and
// closes after setup_portal_timeout; the ESP then keeps retrying on its own.
void setupWiFi()
{
  WiFi.hostname(VMC_HOSTNAME);
  WiFi.mode(WIFI_STA);
  static WiFiEventHandler onStationModeGotIPHandler = WiFi.onStationModeGotIP(onStationModeGotIP);

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

// Answers the mode the command will set (200), or 409 for a Max in High.
void handleCommand(AsyncWebServerRequest *request)
{
  String url = request->url();
  for(int8_t i = 0; i < command_count; i++)
  {
    if(url == commands[i].url)
    {
      if(requestCommand(i) == 200)
      {
        request->send(200, "text/plain", commands[i].mode);
      }
      else
      {
        request->send(409, "text/plain", "Max is ignored in High mode");
      }
      return;
    }
  }
  request->send(404);
}

void handleMode(AsyncWebServerRequest *request)
{
  int8_t mode = maxMode != no_command ? maxMode : currentMode;
  request->send(200, "text/plain", mode == no_command ? "" : commands[mode].mode);
}

void handleVersion(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", VMC_VERSION);
}

// Board details for the bottom of the web page, as JSON:
// {"version":"v1.1","hostname":"vmc","ip":"192.168.1.42","rssi":-61}
// rssi is the Wi-Fi signal in dBm (-50 is very good, -80 is weak).
void handleInfo(AsyncWebServerRequest *request)
{
  String json = String("{\"version\":\"") + VMC_VERSION +
                "\",\"hostname\":\"" + VMC_HOSTNAME +
                "\",\"ip\":\"" + WiFi.localIP().toString() +
                "\",\"rssi\":" + String(WiFi.RSSI()) + "}";
  request->send(200, "application/json", json);
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
    ArduinoOTA.setHostname(VMC_HOSTNAME);
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
