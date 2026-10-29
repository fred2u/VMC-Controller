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

// Fake Updater (Update), used by the web update page: records what it
// receives and lets a test make it fail.
class FakeUpdate
{
public:
  void runAsync(bool a) { async = a; }
  bool begin(size_t size, int kind)
  {
    beginSize = size;
    command = kind;
    beginCount++;
    if (failBegin)
      error = true;
    return !failBegin;
  }
  size_t write(uint8_t *, size_t len)
  {
    written += len;
    return len;
  }
  bool end(bool)
  {
    finished = !error;
    return finished;
  }
  bool hasError() { return error; }
  bool isFinished() { return finished; }
  String getErrorString() { return String("fake error"); }
  template <typename T> void printError(T &) {}
  void reset() { *this = FakeUpdate(); }

  bool async = false;
  size_t beginSize = 0;
  int command = -1;
  int beginCount = 0;
  size_t written = 0;
  bool finished = false;
  bool error = false;
  bool failBegin = false;
};
extern FakeUpdate Update;
