#pragma once
#include "Arduino.h"

#define WIFI_STA 1
#define WL_CONNECTED 3
#define WL_DISCONNECTED 7

struct WiFiEventStationModeGotIP
{
};

class WiFiEventHandler
{
};

class FakeWiFi
{
public:
  void hostname(const char *h) { hostnameSet = h; }
  void mode(int m) { modeSet = m; }
  void begin(const char *s, const char *)
  {
    ssidUsed = s;
    savedSsid = s;
  }
  void begin() { beginSavedCount++; }
  String SSID() { return savedSsid; }
  template <typename F>
  WiFiEventHandler onStationModeGotIP(F) { return WiFiEventHandler(); }
  IPAddress localIP() { return IPAddress(); }
  int status() { return statusValue; }

  String hostnameSet;
  String ssidUsed;
  String savedSsid; // network saved in flash by the Wi-Fi SDK
  int beginSavedCount = 0;
  int modeSet = 0;
  int statusValue = WL_DISCONNECTED;
};
extern FakeWiFi WiFi;
