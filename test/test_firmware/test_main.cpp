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
unsigned long nowMillis = 0;

void reset()
{
  events.clear();
  for (int i = 0; i < 32; i++)
  {
    pinModes[i] = -1;
    pinLevels[i] = -1;
  }
  nowMillis = 0;
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

// 32 bits like on the ESP, so the tests see millis() wrap around.
unsigned long millis() { return (uint32_t)fake::nowMillis; }

// ----- the firmware under test (unchanged) ---------------------------------
// On the ESP, scripts/ota_password.py sets this from VMC_OTA_PASSWORD.
#define OTA_PASSWORD "test-ota-password"
// On the ESP, scripts/version.py sets this from the Git tags.
#define VMC_VERSION "v9.9-test"
#include "../../src/main.cpp"

// ----- helpers -------------------------------------------------------------

// Put the firmware back in its power-on state and run setup().
// savedSsid: network already saved in flash. wifiConnects: whether the
// Wi-Fi manager gets a connection (otherwise its portal times out).
static void boot(const char *savedSsid = "", bool wifiConnects = true)
{
  fake::reset();
  pendingCommand = no_command;
  currentMode = no_command;
  maxMode = no_command;
  maxModeStart = 0;
  maxModeLength = 0;
  sendStep = SEND_IDLE;
  sending = no_command;
  pressesLeft = 0;
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

static bool senderIdle() { return sendStep == SEND_IDLE && pendingCommand == no_command; }

// Calls loop() every millisecond, like the ESP does, until nothing is left
// to send (or `ms` milliseconds have gone by).
static void runFor(unsigned long ms)
{
  for (unsigned long i = 0; i < ms; i++)
  {
    loop();
    fake::nowMillis++;
  }
}

static void runUntilIdle()
{
  for (int i = 0; i < 100000; i++)
  {
    loop();
    if (senderIdle())
      return;
    fake::nowMillis++;
  }
  TEST_FAIL_MESSAGE("the command never finished");
}

// Boot, then let loop() send the Medium command that setup() asks for.
static void bootAndSettle()
{
  boot();
  runUntilIdle();
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
  runUntilIdle();
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
  runUntilIdle();
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
  runUntilIdle();
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
  // LED on, button down, 500 ms later button up, 600 ms pause, LED off.
  TEST_ASSERT_EQUAL(4, (int)fake::events.size());
  TEST_ASSERT_EQUAL(PIN_LED, fake::events[0].pin);
  TEST_ASSERT_EQUAL(LOW, fake::events[0].value);
  TEST_ASSERT_EQUAL(PIN_LOW, fake::events[1].pin);
  TEST_ASSERT_EQUAL(LOW, fake::events[1].value);
  TEST_ASSERT_EQUAL(PIN_LOW, fake::events[2].pin);
  TEST_ASSERT_EQUAL(HIGH, fake::events[2].value);
  TEST_ASSERT_EQUAL(500, (long)(fake::events[2].atMillis - fake::events[1].atMillis));
  TEST_ASSERT_EQUAL(PIN_LED, fake::events[3].pin);
  TEST_ASSERT_EQUAL(HIGH, fake::events[3].value);
  TEST_ASSERT_EQUAL(600, (long)(fake::events[3].atMillis - fake::events[2].atMillis));
}

void test_loop_never_waits()
{
  webServer.request("/max3");
  runUntilIdle();
  for (const HwEvent &e : fake::events)
    TEST_ASSERT_TRUE_MESSAGE(e.kind != HwEvent::DELAY, "delay() called in loop()");
}

void test_the_web_server_answers_while_a_command_is_sent()
{
  webServer.request("/max3");
  runFor(1000);
  TEST_ASSERT_FALSE(senderIdle());
  AsyncWebServerRequest r = webServer.request("/mode");
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_EQUAL_STRING("Max 60 min", r.content.c_str());
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
  runUntilIdle();
  TEST_ASSERT_EQUAL(0, pressCount(PIN_LOW));
  TEST_ASSERT_EQUAL(1, pressCount(PIN_AUTO));
  TEST_ASSERT_EQUAL_STRING("Auto", modeText().c_str());
}

void test_a_command_received_while_sending_is_sent_after_it()
{
  webServer.request("/max2");
  runFor(700); // first Timer press done
  TEST_ASSERT_EQUAL(200, webServer.request("/low").status);
  runUntilIdle();
  TEST_ASSERT_EQUAL(2, pressCount(PIN_MAX)); // Max 30 min is not cut short
  TEST_ASSERT_EQUAL(1, pressCount(PIN_LOW));
  TEST_ASSERT_EQUAL_STRING("Low", modeText().c_str());
}

void test_while_sending_only_the_last_waiting_command_is_kept()
{
  webServer.request("/max3");
  runFor(500);
  webServer.request("/low");
  webServer.request("/high");
  webServer.request("/auto");
  runUntilIdle();
  TEST_ASSERT_EQUAL(3, pressCount(PIN_MAX));
  TEST_ASSERT_EQUAL(0, pressCount(PIN_LOW));
  TEST_ASSERT_EQUAL(0, pressCount(PIN_HIGH));
  TEST_ASSERT_EQUAL(1, pressCount(PIN_AUTO));
  TEST_ASSERT_EQUAL(4, pressCountOnButtons());
}

void test_two_commands_in_a_row_are_600ms_apart()
{
  webServer.request("/low");
  runFor(10);
  webServer.request("/medium");
  runUntilIdle();
  unsigned long lowUp = 0, mediumDown = 0;
  for (const HwEvent &e : fake::events)
  {
    if (e.pin == PIN_LOW && e.value == HIGH) lowUp = e.atMillis;
    if (e.pin == PIN_MEDIUM && e.value == LOW) mediumDown = e.atMillis;
  }
  TEST_ASSERT_EQUAL(600, (long)(mediumDown - lowUp));
}

void test_a_command_answers_the_mode_it_sets()
{
  AsyncWebServerRequest r = webServer.request("/max2");
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_EQUAL_STRING("text/plain", r.contentType.c_str());
  TEST_ASSERT_EQUAL_STRING("Max 30 min", r.content.c_str());
  TEST_ASSERT_EQUAL_STRING("Low", webServer.request("/low").content.c_str());
}

// ----- Max (timer) modes ---------------------------------------------------

static void checkMax(const char *url, int presses, const char *expectedMode, int minutes)
{
  bootAndSettle();
  sendAndRun(url);
  TEST_ASSERT_EQUAL_MESSAGE(presses, pressCount(PIN_MAX), url);
  TEST_ASSERT_EQUAL_MESSAGE(presses, pressCountOnButtons(), url);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(expectedMode, modeText().c_str(), url);
  TEST_ASSERT_EQUAL_MESSAGE(minutes * 60000L, (long)maxModeLength, url);
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
  unsigned long sentAt = fake::nowMillis;
  sendAndRun("/max1");
  TEST_ASSERT_EQUAL_STRING("Max 15 min", modeText().c_str());

  fake::nowMillis = sentAt + 15 * 60000UL; // exactly at the end: still Max
  loop();
  TEST_ASSERT_EQUAL_STRING("Max 15 min", modeText().c_str());

  fake::nowMillis += 1000; // one second later: back to Low
  loop();
  TEST_ASSERT_EQUAL_STRING("Low", modeText().c_str());
}

void test_max_timer_survives_the_millis_wraparound()
{
  // millis() goes back to 0 after about 49 days.
  fake::nowMillis = 0xFFFFFFFFUL - 60000;
  sendAndRun("/max1");
  fake::nowMillis += 10 * 60000UL; // wrapped, 10 min later: still Max
  loop();
  TEST_ASSERT_EQUAL_STRING("Max 15 min", modeText().c_str());
  fake::nowMillis += 6 * 60000UL; // 16 min: over
  loop();
  TEST_ASSERT_EQUAL_STRING("Medium", modeText().c_str());
}

void test_a_normal_mode_cancels_a_running_max()
{
  sendAndRun("/max2");
  sendAndRun("/auto");
  TEST_ASSERT_EQUAL_STRING("Auto", modeText().c_str());
  TEST_ASSERT_EQUAL(no_command, maxMode);
}

void test_a_new_max_restarts_the_timer()
{
  sendAndRun("/max3");
  fake::nowMillis += 10 * 60000UL;
  unsigned long restartedAt = fake::nowMillis;
  sendAndRun("/max1");
  TEST_ASSERT_EQUAL_STRING("Max 15 min", modeText().c_str());
  fake::nowMillis = restartedAt + 15 * 60000UL;
  loop();
  TEST_ASSERT_EQUAL_STRING("Max 15 min", modeText().c_str());
  fake::nowMillis += 1000;
  loop();
  TEST_ASSERT_EQUAL_STRING("Medium", modeText().c_str());
}

// In High the remote's Timer button does nothing (the unit is already in
// its highest position): Max is refused with 409 and nothing is pressed.
void test_max_is_refused_with_409_in_high_mode()
{
  sendAndRun("/high");
  fake::events.clear();
  AsyncWebServerRequest r = webServer.request("/max2");
  TEST_ASSERT_EQUAL(409, r.status);
  TEST_ASSERT_EQUAL_STRING("Max is ignored in High mode", r.content.c_str());
  runUntilIdle();
  TEST_ASSERT_EQUAL(0, pressCountOnButtons());
  TEST_ASSERT_EQUAL_STRING("High", modeText().c_str());
  // It is not replayed later.
  sendAndRun("/low");
  TEST_ASSERT_EQUAL(0, pressCount(PIN_MAX));
}

void test_max_is_refused_while_high_is_being_sent()
{
  webServer.request("/high");
  runFor(100);
  TEST_ASSERT_EQUAL(409, webServer.request("/max1").status);
  runUntilIdle();
  TEST_ASSERT_EQUAL(0, pressCount(PIN_MAX));
}

void test_max_is_accepted_again_after_leaving_high()
{
  sendAndRun("/high");
  sendAndRun("/low");
  TEST_ASSERT_EQUAL(200, webServer.request("/max1").status);
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

void test_version_answers_the_built_in_version_as_plain_text()
{
  AsyncWebServerRequest r = webServer.request("/version");
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_EQUAL_STRING("text/plain", r.contentType.c_str());
  TEST_ASSERT_EQUAL_STRING("v9.9-test", r.content.c_str());
  TEST_ASSERT_EQUAL(0, pressCountOnButtons());
}

void test_info_answers_version_hostname_ip_and_signal_as_json()
{
  WiFi.ipValue = IPAddress("192.168.1.42");
  WiFi.rssiValue = -61;
  AsyncWebServerRequest r = webServer.request("/info");
  TEST_ASSERT_EQUAL(200, r.status);
  TEST_ASSERT_EQUAL_STRING("application/json", r.contentType.c_str());
  TEST_ASSERT_EQUAL_STRING(
      "{\"version\":\"v9.9-test\",\"hostname\":\"vmc\","
      "\"ip\":\"192.168.1.42\",\"rssi\":-61}",
      r.content.c_str());
  TEST_ASSERT_EQUAL(0, pressCountOnButtons());
}

void test_unknown_url_answers_404_and_presses_nothing()
{
  TEST_ASSERT_EQUAL(404, webServer.request("/turbo").status);
  runFor(2000);
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
  RUN_TEST(test_loop_never_waits);
  RUN_TEST(test_the_web_server_answers_while_a_command_is_sent);
  RUN_TEST(test_loop_does_nothing_without_a_command);
  RUN_TEST(test_a_command_is_sent_only_once);
  RUN_TEST(test_only_the_last_command_before_loop_is_sent);
  RUN_TEST(test_a_command_received_while_sending_is_sent_after_it);
  RUN_TEST(test_while_sending_only_the_last_waiting_command_is_kept);
  RUN_TEST(test_two_commands_in_a_row_are_600ms_apart);
  RUN_TEST(test_a_command_answers_the_mode_it_sets);
  RUN_TEST(test_max_presses_the_max_button_1_2_or_3_times);
  RUN_TEST(test_max_presses_are_600ms_apart);
  RUN_TEST(test_max_mode_ends_after_its_timer_and_shows_the_previous_mode);
  RUN_TEST(test_max_timer_survives_the_millis_wraparound);
  RUN_TEST(test_a_normal_mode_cancels_a_running_max);
  RUN_TEST(test_a_new_max_restarts_the_timer);
  RUN_TEST(test_max_is_refused_with_409_in_high_mode);
  RUN_TEST(test_max_is_refused_while_high_is_being_sent);
  RUN_TEST(test_max_is_accepted_again_after_leaving_high);
  RUN_TEST(test_pages_are_served_from_littlefs);
  RUN_TEST(test_mode_answers_plain_text);
  RUN_TEST(test_version_answers_the_built_in_version_as_plain_text);
  RUN_TEST(test_info_answers_version_hostname_ip_and_signal_as_json);
  RUN_TEST(test_unknown_url_answers_404_and_presses_nothing);
  RUN_TEST(test_ota_waits_for_the_wifi_connection);
  RUN_TEST(test_ota_starts_once_with_hostname_and_password_when_wifi_is_up);
  RUN_TEST(test_ota_is_handled_on_every_loop_even_without_a_command);
  RUN_TEST(test_ota_keeps_running_if_wifi_drops);
  RUN_TEST(test_ota_unmounts_littlefs_before_a_web_interface_update);
  return UNITY_END();
}
