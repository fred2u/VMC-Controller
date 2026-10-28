// Fake ESP Async WebServer: keeps the registered routes so a test can
// "call" a URL, and records what the handler answered.
#pragma once
#include <map>
#include <string>
#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "LittleFS.h"

class AsyncWebServerRequest
{
public:
  explicit AsyncWebServerRequest(const String &u) : path(u) {}
  String url() const { return path; }

  void send(int code) { status = code; }
  void send(int code, const char *type, const String &body)
  {
    status = code;
    contentType = type;
    content = body;
  }
  void send(FakeLittleFS &, const String &file, const char *type)
  {
    status = 200;
    contentType = type;
    servedFile = file;
  }

  String path;
  int status = 0;
  String contentType;
  String content;
  String servedFile;
};

typedef void (*ArRequestHandlerFunction)(AsyncWebServerRequest *request);

class AsyncWebServer
{
public:
  explicit AsyncWebServer(int p) : port(p) {}
  void on(const char *uri, ArRequestHandlerFunction f) { routes[uri] = f; }
  void onNotFound(ArRequestHandlerFunction f) { notFound = f; }
  void begin() { started = true; }

  // Test helper: route a GET request like the real server would.
  AsyncWebServerRequest request(const char *uri)
  {
    AsyncWebServerRequest req{String(uri)};
    auto it = routes.find(uri);
    if (it != routes.end())
      it->second(&req);
    else if (notFound)
      notFound(&req);
    return req;
  }

  int port;
  bool started = false;
  std::map<std::string, ArRequestHandlerFunction> routes;
  ArRequestHandlerFunction notFound = nullptr;
};
