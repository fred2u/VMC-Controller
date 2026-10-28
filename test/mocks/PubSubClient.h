// Fake PubSubClient (MQTT): records the connection, the publications and
// the subscriptions, and lets a test deliver a message or drop the broker.
#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Arduino.h"

class WiFiClient
{
};

struct FakePublication
{
  std::string topic;
  std::string payload;
  bool retained;
};

class PubSubClient
{
public:
  explicit PubSubClient(WiFiClient &) {}
  PubSubClient &setServer(const char *h, uint16_t p)
  {
    host = h;
    port = p;
    return *this;
  }
  PubSubClient &setCallback(void (*cb)(char *, uint8_t *, unsigned int))
  {
    callback = cb;
    return *this;
  }
  bool setBufferSize(uint16_t size)
  {
    bufferSize = size;
    return true;
  }
  bool connect(const char *id, const char *u, const char *p, const char *willTopic,
               uint8_t, bool willRetain, const char *willMessage)
  {
    connectCount++;
    clientId = id;
    user = u ? u : "";
    password = p ? p : "";
    will = willTopic;
    willPayload = willMessage;
    willRetained = willRetain;
    isConnected = brokerUp;
    return isConnected;
  }
  bool connected() { return isConnected; }
  bool publish(const char *topic, const char *payload, bool retained)
  {
    if (!isConnected)
      return false;
    if (payload && std::string(payload).size() + std::string(topic).size() + 8 > bufferSize)
      return false; // too big for the buffer: the real client drops it
    published.push_back({topic, payload, retained});
    return true;
  }
  bool subscribe(const char *topic)
  {
    subscriptions.push_back(topic);
    return true;
  }
  bool loop() { loopCount++; return isConnected; }

  // Test helpers.
  void deliver(const char *topic, const char *payload)
  {
    std::string t = topic, p = payload;
    callback(&t[0], (uint8_t *)&p[0], p.size());
  }
  // Last payload published on a topic, or "<none>".
  const std::string &last(const std::string &topic) const
  {
    static const std::string none = "<none>";
    for (auto it = published.rbegin(); it != published.rend(); ++it)
      if (it->topic == topic)
        return it->payload;
    return none;
  }
  int count(const std::string &topic) const
  {
    int n = 0;
    for (const FakePublication &p : published)
      if (p.topic == topic)
        n++;
    return n;
  }

  std::string host;
  uint16_t port = 0;
  uint16_t bufferSize = 256;
  void (*callback)(char *, uint8_t *, unsigned int) = nullptr;
  int connectCount = 0;
  int loopCount = 0;
  std::string clientId, user, password, will, willPayload;
  bool willRetained = false;
  bool isConnected = false;
  bool brokerUp = true;
  std::vector<FakePublication> published;
  std::vector<std::string> subscriptions;
};
