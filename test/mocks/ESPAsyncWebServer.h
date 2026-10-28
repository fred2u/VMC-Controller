// Fake ESP Async WebServer: keeps the registered routes so a test can
// "call" a URL, and records what the handler answered.
#pragma once
#include <map>
#include <string>
#include <vector>
#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "LittleFS.h"

class AsyncWebServerRequest
{
public:
  explicit AsyncWebServerRequest(const String &u) : path(u) {}
  String url() const { return path; }

  void send(int code) { status = code; }
  // Password check: the test sets which credentials the request carries.
  bool authenticate(const char *user, const char *password) const
  {
    return authUser == user && authPassword == password;
  }
  void requestAuthentication() { status = 401; }
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
  String authUser;
  String authPassword;
};

// Credentials that the test requests carry by default (see the tests).
extern String fakeAuthUser;
extern String fakeAuthPassword;

typedef void (*ArRequestHandlerFunction)(AsyncWebServerRequest *request);
typedef void (*ArUploadHandlerFunction)(AsyncWebServerRequest *request, const String &filename,
                                        size_t index, uint8_t *data, size_t len, bool final);

enum WebRequestMethod { HTTP_GET = 1, HTTP_POST = 2, HTTP_ANY = 0xFF };

struct FakeRoute
{
  int method;
  ArRequestHandlerFunction handler;
  ArUploadHandlerFunction upload;
};

class AsyncWebServer
{
public:
  explicit AsyncWebServer(int p) : port(p) {}
  void on(const char *uri, ArRequestHandlerFunction f) { routes[uri] = {HTTP_ANY, f, nullptr}; }
  void on(const char *uri, int method, ArRequestHandlerFunction f) { routes[uri] = {method, f, nullptr}; }
  void on(const char *uri, int method, ArRequestHandlerFunction f, ArUploadHandlerFunction u)
  {
    routes[uri] = {method, f, u};
  }
  void onNotFound(ArRequestHandlerFunction f) { notFound = f; }
  void begin() { started = true; }

  // Test helper: route a request like the real server would. A route
  // registered for another method falls through to the not-found handler.
  AsyncWebServerRequest request(const char *uri, int method = HTTP_GET)
  {
    AsyncWebServerRequest req{String(uri)};
    req.authUser = fakeAuthUser;
    req.authPassword = fakeAuthPassword;
    auto it = routes.find(uri);
    if (it != routes.end() && (it->second.method & method))
      it->second.handler(&req);
    else if (notFound)
      notFound(&req);
    return req;
  }

  // Test helper: POST a file in `chunks` pieces of `chunkSize` bytes to an
  // upload route, then call its request handler, like the real server.
  AsyncWebServerRequest upload(const char *uri, int chunks, size_t chunkSize)
  {
    AsyncWebServerRequest req{String(uri)};
    req.authUser = fakeAuthUser;
    req.authPassword = fakeAuthPassword;
    auto it = routes.find(uri);
    if (it == routes.end() || !it->second.upload)
    {
      if (notFound)
        notFound(&req);
      return req;
    }
    std::vector<uint8_t> data(chunkSize, 0xAB);
    for (int i = 0; i < chunks; i++)
      it->second.upload(&req, String("firmware.bin"), i * chunkSize, data.data(), chunkSize, i == chunks - 1);
    it->second.handler(&req);
    return req;
  }

  int port;
  bool started = false;
  std::map<std::string, FakeRoute> routes;
  ArRequestHandlerFunction notFound = nullptr;
};
