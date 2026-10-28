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
  bool begin() { return true; }
  Dir openDir(const char *) { return Dir(); }
};
extern FakeLittleFS LittleFS;
