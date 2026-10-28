// Fake ArduinoOTA: records how the firmware sets up and drives OTA.
#pragma once
#include <functional>
#include "Arduino.h"

#define U_FLASH 0
#define U_FS 100

class FakeArduinoOTA
{
public:
  void setHostname(const char *h) { hostname = h; }
  void setPassword(const char *p) { password = p; }
  void onStart(std::function<void()> f) { startHandler = f; }
  void begin() { beginCount++; }
  void handle() { handleCount++; }
  int getCommand() { return command; }

  // Test helper: pretend an update of the given kind is starting.
  void simulateStart(int kind)
  {
    command = kind;
    if (startHandler)
      startHandler();
  }

  void reset() { *this = FakeArduinoOTA(); }

  String hostname;
  String password;
  int beginCount = 0;
  int handleCount = 0;
  int command = U_FLASH;
  std::function<void()> startHandler;
};
extern FakeArduinoOTA ArduinoOTA;
