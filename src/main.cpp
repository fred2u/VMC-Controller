#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include "LittleFS.h"
#include <Updater.h>
#include <flash_hal.h>

// The Wi-Fi network is picked from the setup portal. Updates over Wi-Fi
// need OTA_PASSWORD, set from the VMC_OTA_PASSWORD environment variable
// at build time by scripts/passwords.py.
#ifndef OTA_PASSWORD
#warning "VMC_OTA_PASSWORD is not set: updates over Wi-Fi are disabled"
#endif

// Optional password for the web interface and the HTTP routes, from the
// VMC_HTTP_PASSWORD environment variable at build time (scripts/passwords.py).
// The user name is always "admin". Without it, the routes are open, as on a
// trusted home network, and the web update page (/update) is disabled.
#define HTTP_USER "admin"
#ifdef HTTP_PASSWORD
const bool web_update = true;
#else
const bool web_update = false;
#endif

// Optional password for the VMC-Setup access point (VMC_SETUP_PASSWORD, at
// least 8 characters). Without it, the access point is open.

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

#define NO_PIN 255

const unsigned long command_delay = 500;         // ms a button is held down
const unsigned long between_command_delay = 600; // ms released after a press
// Remote maintenance, two buttons held together. Pairing: the manual says
// "until the LED blinks", 5 s leaves it time. Reset: at least 3 s.
const unsigned long pair_delay = 5000;
const unsigned long remote_reset_delay = 4000;

// Every command the web server accepts. Its position in this list is the
// number stored as the waiting command. Max presses the Timer button 1, 2 or 3
// times: the unit then stays in High for 15, 30 or 60 minutes. The last two
// press two buttons at once and leave the mode as it is.
struct Command
{
  const char *url;
  const char *mode;    // answered on /mode while it is the last mode sent
  uint8_t pin;
  uint8_t pin2;        // pressed together with pin, or NO_PIN
  uint8_t presses;
  uint8_t maxMinutes;  // 0: a normal mode, kept until the next command
  unsigned long hold;  // ms each press lasts
  bool keepsMode;      // remote maintenance: /mode does not change
};

const Command commands[] = {
  {"/default", "Default", PIN_DEFAULT, NO_PIN, 1, 0, command_delay, false},
  {"/auto", "Auto", PIN_AUTO, NO_PIN, 1, 0, command_delay, false},
  {"/low", "Low", PIN_LOW, NO_PIN, 1, 0, command_delay, false},
  {"/medium", "Medium", PIN_MEDIUM, NO_PIN, 1, 0, command_delay, false},
  {"/high", "High", PIN_HIGH, NO_PIN, 1, 0, command_delay, false},
  {"/max1", "Max 15 min", PIN_MAX, NO_PIN, 1, 15, command_delay, false},
  {"/max2", "Max 30 min", PIN_MAX, NO_PIN, 2, 30, command_delay, false},
  {"/max3", "Max 60 min", PIN_MAX, NO_PIN, 3, 60, command_delay, false},
  // Pairing: 1 and Auto. Remote reset: Default (house) and Timer.
  {"/pair", "Pairing", PIN_LOW, PIN_AUTO, 1, 0, pair_delay, true},
  {"/reset-remote", "Remote reset", PIN_DEFAULT, PIN_MAX, 1, 0, remote_reset_delay, true},
};
const int8_t command_count = sizeof(commands) / sizeof(commands[0]);
const int8_t no_command = -1;
const int8_t command_medium = 3;
const int8_t command_high = 4;
// From this one on, the commands are remote maintenance, sent with POST so
// that a browser or a link preview never triggers them by accident.
const int8_t first_remote_command = 8;

// Access point opened when the ESP cannot join a network, and how long
// the ESP waits for the network and for someone to use the portal.
const char *setup_ap_name = "VMC-Setup";
const int wifi_connect_timeout = 30; // seconds
const int setup_portal_timeout = 180; // seconds


void onStationModeGotIP(const WiFiEventStationModeGotIP& event);
void handleDefault(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);
void handleJS(AsyncWebServerRequest *request);
void handleCSS(AsyncWebServerRequest *request);
void handleCommand(AsyncWebServerRequest *request);
void handleMode(AsyncWebServerRequest *request);
void handleVersion(AsyncWebServerRequest *request);
void handleInfo(AsyncWebServerRequest *request);
void handleMaintenance(AsyncWebServerRequest *request);
void handleRestart(AsyncWebServerRequest *request);
void handleForgetWiFi(AsyncWebServerRequest *request);
void handleUpdateDone(AsyncWebServerRequest *request);
void handleUpdateUpload(AsyncWebServerRequest *request, const String &filename,
                        size_t index, uint8_t *data, size_t len, bool final);
bool allowed(AsyncWebServerRequest *request);

int requestCommand(int8_t command);
void runSender();
void startCommand(int8_t command);
void expireMaxMode();
void pressButtons(const Command &c, uint8_t level);
void updateUptime();
void handleRestartRequest();
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

// Time since boot, in ms. millis() wraps around after 49 days, this does not.
uint64_t uptime = 0;
uint32_t uptimeLastMillis = 0;

// Restart asked from the maintenance page, done from loop() once the answer
// has left and no button is held. forgetWiFi also erases the saved network.
enum RestartRequest { RESTART_NONE, RESTART_ONLY, RESTART_FORGET_WIFI };
volatile RestartRequest restartRequest = RESTART_NONE;
uint32_t restartRequestedAt = 0;
const unsigned long restart_delay = 1000; // ms

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
  webServer.on("/maintenance", handleMaintenance);
  webServer.on("/maintenance.html", handleMaintenance);
  webServer.on("/maintenance.js", handleJS);
  for(int8_t i = 0; i < command_count; i++)
  {
    if(i < first_remote_command)
    {
      webServer.on(commands[i].url, handleCommand);
    }
    else
    {
      webServer.on(commands[i].url, HTTP_POST, handleCommand);
    }
  }
  webServer.on("/mode", handleMode);
  webServer.on("/version", handleVersion);
  webServer.on("/info", handleInfo);
  webServer.on("/restart", HTTP_POST, handleRestart);
  webServer.on("/forget-wifi", HTTP_POST, handleForgetWiFi);
#ifdef HTTP_PASSWORD
  webServer.on("/update/firmware", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  webServer.on("/update/web", HTTP_POST, handleUpdateDone, handleUpdateUpload);
#endif
  webServer.onNotFound(handleNotFound);
  webServer.begin();

  requestCommand(command_medium);
  delay(2000);
}

void loop()
{
  updateUptime();
  handleOTA();
  expireMaxMode();
  runSender();
  handleRestartRequest();
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
    pressButtons(commands[sending], HIGH);
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
  pressButtons(commands[sending], LOW);
  sendStep = SEND_PRESSED;
  stepStart = now;
  stepLength = commands[sending].hold;
}

// LOW presses the command's button(s), HIGH releases them.
void pressButtons(const Command &c, uint8_t level)
{
  digitalWrite(c.pin, level);
  if(c.pin2 != NO_PIN)
  {
    digitalWrite(c.pin2, level);
  }
}

// Updates the mode answered on /mode and lights the LED, before the first
// press of a command.
void startCommand(int8_t command)
{
  const Command &c = commands[command];
  Serial.println(c.url);
  digitalWrite(PIN_LED, LOW);

  if(c.keepsMode)
  {
    // Pairing or remote reset: the unit keeps its mode.
  }
  else if(c.maxMinutes > 0)
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
#ifdef SETUP_PASSWORD
  bool connected = wifiManager.autoConnect(setup_ap_name, SETUP_PASSWORD);
#else
  bool connected = wifiManager.autoConnect(setup_ap_name);
#endif
  if(!connected)
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
  if(!allowed(request))
  {
    return;
  }
  request->send(LittleFS, "/index.html", "text/html");
}

void handleNotFound(AsyncWebServerRequest *request)
{
 	Serial.println("NotFound: " + request->url());
  request->send(404);
}

void handleJS(AsyncWebServerRequest *request)
{
  if(!allowed(request))
  {
    return;
  }
  request->send(LittleFS, request->url(), "text/javascript");
}

void handleCSS(AsyncWebServerRequest *request)
{
  if(!allowed(request))
  {
    return;
  }
  request->send(LittleFS, request->url(), "text/css");
}

// Answers the mode the command will set (200), or 409 for a Max in High.
// Pairing and remote reset answer what they do ("Pairing", "Remote reset").
void handleCommand(AsyncWebServerRequest *request)
{
  if(!allowed(request))
  {
    return;
  }
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
  if(!allowed(request))
  {
    return;
  }
  int8_t mode = maxMode != no_command ? maxMode : currentMode;
  request->send(200, "text/plain", mode == no_command ? "" : commands[mode].mode);
}

void handleVersion(AsyncWebServerRequest *request)
{
  if(!allowed(request))
  {
    return;
  }
  request->send(200, "text/plain", VMC_VERSION);
}

// Board details for the web pages, as JSON:
// {"version":"v1.2","hostname":"vmc","ip":"192.168.1.42","rssi":-61,
//  "ssid":"home","uptime":3600,"heap":31000,"reset":"Power On","update":true}
// rssi is the Wi-Fi signal in dBm (-50 is very good, -80 is weak), uptime
// the seconds since boot, heap the free memory in bytes, reset the reason
// of the last start given by the ESP, update whether the web update page
// is built in (only with a password).
void handleInfo(AsyncWebServerRequest *request)
{
  if(!allowed(request))
  {
    return;
  }
  String json = String("{\"version\":\"") + VMC_VERSION +
                "\",\"hostname\":\"" + VMC_HOSTNAME +
                "\",\"ip\":\"" + WiFi.localIP().toString() +
                "\",\"rssi\":" + String(WiFi.RSSI()) +
                ",\"ssid\":\"" + WiFi.SSID() +
                "\",\"uptime\":" + String((unsigned long)(uptime / 1000)) +
                ",\"heap\":" + String((unsigned long)ESP.getFreeHeap()) +
                ",\"reset\":\"" + ESP.getResetReason() +
                "\",\"update\":" + (web_update ? "true" : "false") + "}";
  request->send(200, "application/json", json);
}

void handleMaintenance(AsyncWebServerRequest *request)
{
  if(!allowed(request))
  {
    return;
  }
  request->send(LittleFS, "/maintenance.html", "text/html");
}

void handleRestart(AsyncWebServerRequest *request)
{
  if(!allowed(request))
  {
    return;
  }
  restartRequest = RESTART_ONLY;
  restartRequestedAt = millis();
  request->send(200, "text/plain", "Restarting");
}

// Erases the saved Wi-Fi network and restarts: the ESP then opens the
// VMC-Setup access point, see setupWiFi().
void handleForgetWiFi(AsyncWebServerRequest *request)
{
  if(!allowed(request))
  {
    return;
  }
  restartRequest = RESTART_FORGET_WIFI;
  restartRequestedAt = millis();
  request->send(200, "text/plain", "Wi-Fi network forgotten, restarting");
}

// With a password (VMC_HTTP_PASSWORD), answers 401 and asks the browser
// for it when the request does not carry it. Basic and Digest both work
// (Home Assistant's rest_command sends Basic with username and password).
bool allowed(AsyncWebServerRequest *request)
{
#ifdef HTTP_PASSWORD
  if(!request->authenticate(HTTP_USER, HTTP_PASSWORD))
  {
    request->requestAuthentication();
    return false;
  }
#endif
  return true;
}

#ifdef HTTP_PASSWORD
// Web update (/update/firmware and /update/web), only built with a password.
// The file arrives in pieces; the first one starts the update, the last one
// checks it. handleUpdateDone answers once the whole file is in, then the
// ESP restarts on the new firmware or web interface.
bool updateRefused = false;

void handleUpdateUpload(AsyncWebServerRequest *request, const String &filename,
                        size_t index, uint8_t *data, size_t len, bool final)
{
  if(index == 0)
  {
    updateRefused = !request->authenticate(HTTP_USER, HTTP_PASSWORD);
    if(updateRefused)
    {
      return;
    }
    bool web = request->url() == "/update/web";
    Serial.println(web ? "Web interface update" : "Firmware update");
    Update.runAsync(true);
    bool started;
    if(web)
    {
      // The web interface image replaces the file system: unmount it first.
      LittleFS.end();
      started = Update.begin(FS_PHYS_SIZE, U_FS);
    }
    else
    {
      started = Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000, U_FLASH);
    }
    if(!started)
    {
      Update.printError(Serial);
    }
  }
  if(updateRefused || Update.hasError())
  {
    return;
  }
  if(Update.write(data, len) != len)
  {
    Update.printError(Serial);
  }
  if(final && !Update.end(true))
  {
    Update.printError(Serial);
  }
}

void handleUpdateDone(AsyncWebServerRequest *request)
{
  if(!allowed(request))
  {
    return;
  }
  if(updateRefused || Update.hasError() || !Update.isFinished())
  {
    request->send(500, "text/plain", "Update failed: " + Update.getErrorString());
    return;
  }
  request->send(200, "text/plain", "Update done, restarting");
  restartRequest = RESTART_ONLY;
  restartRequestedAt = millis();
}
#endif

void handleRestartRequest()
{
  if(restartRequest == RESTART_NONE || sendStep != SEND_IDLE ||
     millis() - restartRequestedAt < restart_delay)
  {
    return;
  }
  if(restartRequest == RESTART_FORGET_WIFI)
  {
    Serial.println("Forgetting the Wi-Fi network");
    WiFi.disconnect(true);
  }
  Serial.println("Restarting");
  restartRequest = RESTART_NONE;
  ESP.restart();
}

void updateUptime()
{
  uint32_t now = millis();
  uptime += (uint32_t)(now - uptimeLastMillis);
  uptimeLastMillis = now;
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
