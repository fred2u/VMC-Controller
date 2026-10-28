// Unit tests for src/main.cpp, run on the computer (no ESP needed):
//   pio test -e native
//
// The firmware file is compiled as-is against the fake Arduino, WiFi,
// LittleFS and web-server headers in test/mocks/. Nothing in src/ is
// changed for the tests.

#include <unity.h>
#include "ESPAsyncWebServer.h"
#include "ArduinoOTA.h"
#include "ESPAsyncWiFiManager.h"

// ----- fake hardware state -------------------------------------------------
FakeSerial Serial;
FakeWiFi WiFi;
FakeLittleFS LittleFS;
FakeArduinoOTA ArduinoOTA;
FakeWiFiManagerState wifiManagerState;

namespace fake
{
std::vector<HwEvent> events;
int pinModes[32];
int pinLevels[32];
long nowMillis = 0;
time_t nowEpoch = 1000000;

void reset()
{
  events.clear();
  for (int i = 0; i < 32; i++)
  {
    pinModes[i] = -1;
    pinLevels[i] = -1;
  }
  nowMillis = 0;
  nowEpoch = 1000000;
}
}

void pinMode(uint8_t pin, uint8_t mode) { fake::pinModes[pin] = mode; }

void digitalWrite(uint8_t pin, uint8_t value)
{
  fake::pinLevels[pin] = value;
  fake::events.push_back({HwEvent::WRITE, pin, value, fake::nowMillis});
}

void delay(unsigned long ms)
{
  fake::events.push_back({HwEvent::DELAY, -1, (int)ms, fake::nowMillis});
  fake::nowMillis += ms;
}

time_t fake_time(time_t *) { return fake::nowEpoch; }

// ----- the firmware under test (unchanged) ---------------------------------
// On the ESP, scripts/ota_password.py sets this from VMC_OTA_PASSWORD.
#define OTA_PASSWORD "test-ota-password"
#include "../../src/main.cpp"

// ----- helpers -------------------------------------------------------------

// Put the firmware back in its power-on state and run setup().
// savedSsid: network already saved in flash. wifiConnects: whether the
// Wi-Fi manager gets a connection (otherwise its portal times out).
static void boot(const char *savedSsid = "", bool wifiConnects = true)
{
  fake::reset();
  command = "";
  currentMode = "";
  tempMode = "";
  tempModeExpire = 0;
  otaStarted = false;
  ArduinoOTA.reset();
  WiFi = FakeWiFi();
  WiFi.savedSsid = savedSsid;
  wifiManagerState.reset();
  wifiManagerState.connects = wifiConnects;
  webServer.routes.clear();
  webServer.notFound = nullptr;
  setup();
  fake::events.clear();
}

// Boot, then let loop() send the Medium command that setup() queues.
static void bootAndSettle()
{
  boot();
  loop();
  fake::events.clear();
}

static String modeText() { return webServer.request("/mode").content; }

// How many times the button on `pin` was pressed (pulled LOW then released).
static int pressCount(int pin)
{
  int n = 0;
  for (const HwEvent &e : fake::events)
    if (e.kind == HwEvent::WRITE && e.pin == pin && e.value == LOW)
      n++;
  return n;
}

static int pressCountOnButtons()
{
  return pressCount(PIN_DEFAULT) + pressCount(PIN_AUTO) + pressCount(PIN_LOW) +
         pressCount(PIN_MEDIUM) + pressCount(PIN_HIGH) + pressCount(PIN_MAX);
}

static void sendAndRun(const char *url)
{
  TEST_ASSERT_EQUAL(200, webServer.request(url).status);
  loop();
}

void setUp() { bootAndSettle(); }
void tearDown() {}

// ----- boot ----------------------------------------------------------------

void test_setup_puts_every_button_pin_in_output_and_released()
{
  boot();
  const int pins[] = {PIN_LED, PIN_DEFAULT, PIN_AUTO, PIN_LOW, PIN_MEDIUM, PIN_HIGH, PIN_MAX};
  for (int pin : pins)
  {
    TEST_ASSERT_EQUAL_MESSAGE(OUTPUT, fake::pinModes[pin], "pin not in OUTPUT");
    TEST_ASSERT_EQUAL_MESSAGE(HIGH, fake::pinLevels[pin], "pin not released (HIGH)");
  }
}

void test_setup_connects_wifi_with_hostname_vmc()
{
  boot();
  TEST_ASSERT_EQUAL_STRING("vmc", WiFi.hostnameSet.c_str());
  TEST_ASSERT_EQUAL(WIFI_STA, WiFi.modeSet);
  TEST_ASSERT_TRUE(webServer.started);
  TEST_ASSERT_EQUAL(80, webServer.port);
}

// ----- Wi-Fi setup portal --------------------------------------------------

void test_wifi_manager_opens_vmc_setup_with_timeouts()
{
  boot();
  TEST_ASSERT_EQUAL(1, wifiManagerState.autoConnectCount);
  TEST_ASSERT_EQUAL_STRING("VMC-Setup", wifiManagerState.apName.c_str());
  TEST_ASSERT_EQUAL(30, wifiManagerState.connectTimeout);
  TEST_ASSERT_EQUAL(180, wifiManagerState.portalTimeout);
}

void test_no_network_is_compiled_into_the_firmware()
{
  // A new board goes straight to the portal, with no built-in network.
  boot("");
  TEST_ASSERT_EQUAL_STRING("", WiFi.ssidUsed.c_str());
  TEST_ASSERT_EQUAL(1, wifiManagerState.autoConnectCount);
}

void test_routes_are_added_after_the_setup_portal()
{
  // The real portal clears the web server when it closes.
  boot();
  TEST_ASSERT_EQUAL(0, wifiManagerState.routesBeforeAutoConnect);
  TEST_ASSERT_EQUAL(200, webServer.request("/mode").status);
  TEST_ASSERT_EQUAL(404, webServer.request("/nope").status);
}

void test_without_wifi_the_esp_retries_and_still_sends_medium()
{
  boot("home-network", false);
  TEST_ASSERT_EQUAL(WIFI_STA, WiFi.modeSet);
  TEST_ASSERT_EQUAL(1, WiFi.beginSavedCount);
  TEST_ASSERT_TRUE(webServer.started);
  loop();
  TEST_ASSERT_EQUAL(1, pressCount(PIN_MEDIUM));
}

void test_with_wifi_the_saved_network_is_not_restarted()
{
  boot("home-network", true);
  TEST_ASSERT_EQUAL(0, WiFi.beginSavedCount);
}

void test_mode_is_empty_before_the_first_command()
{
  boot();
  TEST_ASSERT_EQUAL_STRING("", modeText().c_str());
}

void test_boot_sends_medium()
{
  boot();
  loop();
  TEST_ASSERT_EQUAL(1, pressCount(PIN_MEDIUM));
  TEST_ASSERT_EQUAL(1, pressCountOnButtons());
  TEST_ASSERT_EQUAL_STRING("Medium", modeText().c_str());
}

// ----- simple modes --------------------------------------------------------

static void checkSimpleMode(const char *url, int pin, const char *expectedMode)
{
  sendAndRun(url);
  TEST_ASSERT_EQUAL_MESSAGE(1, pressCount(pin), url);
  TEST_ASSERT_EQUAL_MESSAGE(1, pressCountOnButtons(), url);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedMode, modeText().c_str(), url);
  fake::events.clear();
}

void test_each_mode_presses_its_own_button()
{
  checkSimpleMode("/default", PIN_DEFAULT, "Default");
  checkSimpleMode("/auto", PIN_AUTO, "Auto");
  checkSimpleMode("/low", PIN_LOW, "Low");
  checkSimpleMode("/medium", PIN_MEDIUM, "Medium");
  checkSimpleMode("/high", PIN_HIGH, "High");
}

void test_a_press_is_a_500ms_low_pulse_with_the_led_on()
{
  sendAndRun("/low");
  // LED on, button down, 500 ms, button up, LED off.
  TEST_ASSERT_EQUAL(5, (int)fake::events.size());
  TEST_ASSERT_TRUE(fake::events[0].kind == HwEvent::WRITE);
  TEST_ASSERT_EQUAL(PIN_LED, fake::events[0].pin);
  TEST_ASSERT_EQUAL(LOW, fake::events[0].value);
  TEST_ASSERT_EQUAL(PIN_LOW, fake::events[1].pin);
  TEST_ASSERT_EQUAL(LOW, fake::events[1].value);
  TEST_ASSERT_TRUE(fake::events[2].kind == HwEvent::DELAY);
  TEST_ASSERT_EQUAL(500, fake::events[2].value);
  TEST_ASSERT_EQUAL(PIN_LOW, fake::events[3].pin);
  TEST_ASSERT_EQUAL(HIGH, fake::events[3].value);
  TEST_ASSERT_EQUAL(PIN_LED, fake::events[4].pin);
  TEST_ASSERT_EQUAL(HIGH, fake::events[4].value);
}

void test_loop_does_nothing_without_a_command()
{
  loop();
  loop();
  TEST_ASSERT_EQUAL(0, (int)fake::events.size());
}

void test_a_command_is_sent_only_once()
{
  sendAndRun("/high");
  loop();
  TEST_ASSERT_EQUAL(1, pressCount(PIN_HIGH));
}

void test_only_the_last_command_before_loop_is_sent()
{
  webServer.request("/low");
  webServer.request("/auto");
  loop();
  TEST_ASSERT_EQUAL(0, pressCount(PIN_LOW));
  TEST_ASSERT_EQUAL(1, pressCount(PIN_AUTO));
  TEST_ASSERT_EQUAL_STRING("Auto", modeText().c_str());
}

// ----- Max (timer) modes ---------------------------------------------------

static void checkMax(const char *url, int presses, const char *expectedMode, int minutes)
{
  bootAndSettle();
  sendAndRun(url);
  TEST_ASSERT_EQUAL_MESSAGE(presses, pressCount(PIN_MAX), url);
  TEST_ASSERT_EQUAL_MESSAGE(presses, pressCountOnButtons(), url);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedMode, modeText().c_str(), url);
  TEST_ASSERT_EQUAL_MESSAGE(1000000 + 60 * minutes, (long)tempModeExpire, url);
}

void test_max_presses_the_max_button_1_2_or_3_times()
{
  checkMax("/max1", 1, "Max 15 min", 15);
  checkMax("/max2", 2, "Max 30 min", 30);
  checkMax("/max3", 3, "Max 60 min", 60);
}

void test_max_presses_are_600ms_apart()
{
  sendAndRun("/max3");
  std::vector<long> downAt;
  for (const HwEvent &e : fake::events)
    if (e.kind == HwEvent::WRITE && e.pin == PIN_MAX && e.value == LOW)
      downAt.push_back(e.atMillis);
  TEST_ASSERT_EQUAL(3, (int)downAt.size());
  // 500 ms press + 600 ms pause between two presses.
  TEST_ASSERT_EQUAL(1100, downAt[1] - downAt[0]);
  TEST_ASSERT_EQUAL(1100, downAt[2] - downAt[1]);
}

void test_max_mode_ends_after_its_timer_and_shows_the_previous_mode()
{
  sendAndRun("/low");
  sendAndRun("/max1");
  TEST_ASSERT_EQUAL_STRING("Max 15 min", modeText().c_str());

  fake::nowEpoch += 15 * 60; // exactly at the end: still Max
  loop();
  TEST_ASSERT_EQUAL_STRING("Max 15 min", modeText().c_str());

  fake::nowEpoch += 1; // one second later: back to Low
  loop();
  TEST_ASSERT_EQUAL_STRING("Low", modeText().c_str());
  TEST_ASSERT_EQUAL(0, (long)tempModeExpire);
}

void test_a_normal_mode_cancels_a_running_max()
{
  sendAndRun("/max2");
  sendAndRun("/auto");
  TEST_ASSERT_EQUAL_STRING("Auto", modeText().c_str());
  TEST_ASSERT_EQUAL(0, (long)tempModeExpire);
}

void test_a_new_max_restarts_the_timer()
{
  sendAndRun("/max3");
  fake::nowEpoch += 10 * 60;
  sendAndRun("/max1");
  TEST_ASSERT_EQUAL_STRING("Max 15 min", modeText().c_str());
  TEST_ASSERT_EQUAL((long)fake::nowEpoch + 15 * 60, (long)tempModeExpire);
}

// Current behaviour, documented on purpose (see ROADMAP.md): in High mode
// the Max commands are dropped without pressing anything, and the web
// request still answers 200.
void test_max_is_ignored_while_in_high_mode()
{
  sendAndRun("/high");
  fake::events.clear();
  sendAndRun("/max2");
  TEST_ASSERT_EQUAL(0, pressCountOnButtons());
  TEST_ASSERT_EQUAL_STRING("High", modeText().c_str());
  TEST_ASSERT_EQUAL(0, (long)tempModeExpire);
  // The command is consumed: it is not replayed later.
  sendAndRun("/low");
  TEST_ASSERT_EQUAL(0, pressCount(PIN_MAX));
}

// ----- web server ----------------------------------------------------------

void test_pages_are_served_from_littlefs()
{
  AsyncWebServerRequest r = webServer.request("/");
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_EQUAL_STRING("/index.html", r.servedFile.c_str());
  TEST_ASSERT_EQUAL_STRING("text/html", r.contentType.c_str());

  r = webServer.request("/script.js");
  TEST_ASSERT_EQUAL_STRING("/script.js", r.servedFile.c_str());
  TEST_ASSERT_EQUAL_STRING("text/javascript", r.contentType.c_str());

  r = webServer.request("/style.css");
  TEST_ASSERT_EQUAL_STRING("/style.css", r.servedFile.c_str());
  TEST_ASSERT_EQUAL_STRING("text/css", r.contentType.c_str());
}

void test_mode_answers_plain_text()
{
  AsyncWebServerRequest r = webServer.request("/mode");
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_EQUAL_STRING("text/plain", r.contentType.c_str());
}

void test_unknown_url_answers_404_and_presses_nothing()
{
  TEST_ASSERT_EQUAL(404, webServer.request("/turbo").status);
  loop();
  TEST_ASSERT_EQUAL(0, pressCountOnButtons());
}

// ----- updates over Wi-Fi (OTA) -------------------------------------------

void test_ota_waits_for_the_wifi_connection()
{
  loop();
  TEST_ASSERT_EQUAL(0, ArduinoOTA.beginCount);
  TEST_ASSERT_EQUAL(0, ArduinoOTA.handleCount);
}

void test_ota_starts_once_with_hostname_and_password_when_wifi_is_up()
{
  WiFi.statusValue = WL_CONNECTED;
  loop();
  loop();
  TEST_ASSERT_EQUAL(1, ArduinoOTA.beginCount);
  TEST_ASSERT_EQUAL_STRING("vmc", ArduinoOTA.hostname.c_str());
  TEST_ASSERT_EQUAL_STRING("test-ota-password", ArduinoOTA.password.c_str());
}

void test_ota_is_handled_on_every_loop_even_without_a_command()
{
  WiFi.statusValue = WL_CONNECTED;
  loop();
  loop();
  loop();
  TEST_ASSERT_EQUAL(3, ArduinoOTA.handleCount);
  TEST_ASSERT_EQUAL(0, pressCountOnButtons());
}

void test_ota_keeps_running_if_wifi_drops()
{
  WiFi.statusValue = WL_CONNECTED;
  loop();
  WiFi.statusValue = WL_DISCONNECTED;
  loop();
  TEST_ASSERT_EQUAL(1, ArduinoOTA.beginCount);
  TEST_ASSERT_EQUAL(2, ArduinoOTA.handleCount);
}

void test_ota_unmounts_littlefs_before_a_web_interface_update()
{
  WiFi.statusValue = WL_CONNECTED;
  loop();
  TEST_ASSERT_TRUE(LittleFS.mounted);
  ArduinoOTA.simulateStart(U_FLASH);
  TEST_ASSERT_TRUE(LittleFS.mounted);
  ArduinoOTA.simulateStart(U_FS);
  TEST_ASSERT_FALSE(LittleFS.mounted);
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_setup_puts_every_button_pin_in_output_and_released);
  RUN_TEST(test_setup_connects_wifi_with_hostname_vmc);
  RUN_TEST(test_wifi_manager_opens_vmc_setup_with_timeouts);
  RUN_TEST(test_no_network_is_compiled_into_the_firmware);
  RUN_TEST(test_routes_are_added_after_the_setup_portal);
  RUN_TEST(test_without_wifi_the_esp_retries_and_still_sends_medium);
  RUN_TEST(test_with_wifi_the_saved_network_is_not_restarted);
  RUN_TEST(test_mode_is_empty_before_the_first_command);
  RUN_TEST(test_boot_sends_medium);
  RUN_TEST(test_each_mode_presses_its_own_button);
  RUN_TEST(test_a_press_is_a_500ms_low_pulse_with_the_led_on);
  RUN_TEST(test_loop_does_nothing_without_a_command);
  RUN_TEST(test_a_command_is_sent_only_once);
  RUN_TEST(test_only_the_last_command_before_loop_is_sent);
  RUN_TEST(test_max_presses_the_max_button_1_2_or_3_times);
  RUN_TEST(test_max_presses_are_600ms_apart);
  RUN_TEST(test_max_mode_ends_after_its_timer_and_shows_the_previous_mode);
  RUN_TEST(test_a_normal_mode_cancels_a_running_max);
  RUN_TEST(test_a_new_max_restarts_the_timer);
  RUN_TEST(test_max_is_ignored_while_in_high_mode);
  RUN_TEST(test_pages_are_served_from_littlefs);
  RUN_TEST(test_mode_answers_plain_text);
  RUN_TEST(test_unknown_url_answers_404_and_presses_nothing);
  RUN_TEST(test_ota_waits_for_the_wifi_connection);
  RUN_TEST(test_ota_starts_once_with_hostname_and_password_when_wifi_is_up);
  RUN_TEST(test_ota_is_handled_on_every_loop_even_without_a_command);
  RUN_TEST(test_ota_keeps_running_if_wifi_drops);
  RUN_TEST(test_ota_unmounts_littlefs_before_a_web_interface_update);
  return UNITY_END();
}
