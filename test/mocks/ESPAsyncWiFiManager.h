// Fake ESPAsyncWiFiManager: records how the firmware sets up the Wi-Fi
// setup portal, and lets a test choose whether the connection succeeds.
#pragma once
#include "ESPAsyncWebServer.h"

class DNSServer
{
};

struct FakeWiFiManagerState
{
  String apName;
  unsigned long connectTimeout = 0;
  unsigned long portalTimeout = 0;
  int autoConnectCount = 0;
  size_t routesBeforeAutoConnect = 0;
  bool connects = true; // what autoConnect() returns
  void reset() { *this = FakeWiFiManagerState(); }
};
extern FakeWiFiManagerState wifiManagerState;

class AsyncWiFiManager
{
public:
  AsyncWiFiManager(AsyncWebServer *s, DNSServer *) : server(s) {}
  void setConnectTimeout(unsigned long seconds) { wifiManagerState.connectTimeout = seconds; }
  void setConfigPortalTimeout(unsigned long seconds) { wifiManagerState.portalTimeout = seconds; }
  bool autoConnect(const char *apName)
  {
    wifiManagerState.apName = apName;
    wifiManagerState.autoConnectCount++;
    wifiManagerState.routesBeforeAutoConnect = server->routes.size();
    // The real portal registers its own pages, then clears the server.
    server->routes.clear();
    server->notFound = nullptr;
    return wifiManagerState.connects;
  }

private:
  AsyncWebServer *server;
};
