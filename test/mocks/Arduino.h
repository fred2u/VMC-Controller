// Fake Arduino core used only by the native unit tests.
// It records every GPIO write and every delay so the tests can check
// what the firmware would have done on the real board.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#define OUTPUT 0x01
#define LOW 0x0
#define HIGH 0x1

class String
{
public:
  String() {}
  String(const char *s) : value(s ? s : "") {}
  String(const std::string &s) : value(s) {}
  explicit String(int n) : value(std::to_string(n)) {}

  const char *c_str() const { return value.c_str(); }
  bool operator==(const String &o) const { return value == o.value; }
  bool operator!=(const String &o) const { return value != o.value; }
  bool operator==(const char *o) const { return value == o; }
  bool operator!=(const char *o) const { return value != o; }
  String operator+(const String &o) const { return String(value + o.value); }

  std::string value;
};

inline String operator+(const char *a, const String &b) { return String(std::string(a) + b.value); }
inline bool operator==(const char *a, const String &b) { return b == a; }
inline bool operator!=(const char *a, const String &b) { return b != a; }

class IPAddress
{
public:
  IPAddress() {}
  explicit IPAddress(const char *text) : value(text) {}
  String toString() const { return value; }

  String value = "0.0.0.0";
};

class FakeSerial
{
public:
  void begin(unsigned long) {}
  void println() {}
  void println(const char *) {}
  void println(const String &) {}
  void println(const IPAddress &) {}
  void print(const char *) {}
};
extern FakeSerial Serial;

// One entry per digitalWrite / delay call, in order.
struct HwEvent
{
  enum Kind { WRITE, DELAY } kind;
  int pin;       // WRITE only
  int value;     // WRITE: LOW/HIGH, DELAY: milliseconds
  unsigned long atMillis; // fake clock when the event happened
};

namespace fake
{
extern std::vector<HwEvent> events;
extern int pinModes[32];
extern int pinLevels[32];
extern unsigned long nowMillis; // returned by millis(), advanced by delay() and the tests
void reset();
}

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
void delay(unsigned long ms);
unsigned long millis();
