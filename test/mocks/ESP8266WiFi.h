#pragma once
#include "Arduino.h"

#define WIFI_STA 1

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
  void begin(const char *s, const char *) { ssidUsed = s; }
  template <typename F>
  WiFiEventHandler onStationModeGotIP(F) { return WiFiEventHandler(); }
  IPAddress localIP() { return IPAddress(); }

  String hostnameSet;
  String ssidUsed;
  int modeSet = 0;
};
extern FakeWiFi WiFi;
