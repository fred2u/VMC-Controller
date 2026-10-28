#pragma once
#include "Arduino.h"

class Dir
{
public:
  bool next() { return false; }
  String fileName() { return String(); }
};

class FakeLittleFS
{
public:
  bool begin() { mounted = true; return true; }
  void end() { mounted = false; }
  Dir openDir(const char *) { return Dir(); }

  bool mounted = false;
};
extern FakeLittleFS LittleFS;
