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

enum WebRequestMethod { HTTP_GET = 1, HTTP_POST = 2, HTTP_ANY = 0xFF };

struct FakeRoute
{
  int method;
  ArRequestHandlerFunction handler;
};

class AsyncWebServer
{
public:
  explicit AsyncWebServer(int p) : port(p) {}
  void on(const char *uri, ArRequestHandlerFunction f) { routes[uri] = {HTTP_ANY, f}; }
  void on(const char *uri, int method, ArRequestHandlerFunction f) { routes[uri] = {method, f}; }
  void onNotFound(ArRequestHandlerFunction f) { notFound = f; }
  void begin() { started = true; }

  // Test helper: route a request like the real server would. A route
  // registered for another method falls through to the not-found handler.
  AsyncWebServerRequest request(const char *uri, int method = HTTP_GET)
  {
    AsyncWebServerRequest req{String(uri)};
    auto it = routes.find(uri);
    if (it != routes.end() && (it->second.method & method))
      it->second.handler(&req);
    else if (notFound)
      notFound(&req);
    return req;
  }

  int port;
  bool started = false;
  std::map<std::string, FakeRoute> routes;
  ArRequestHandlerFunction notFound = nullptr;
};
