#include <Arduino.h>
#include "display_ui.h"
#include "settings.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "bambu_mqtt.h"
#include "config.h"
#include "bambu_state.h"

static unsigned long splashEnd = 0;
static unsigned long finishScreenStart = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== BambuHelper Starting ===");

  loadSettings();        // load first so rotation/colors are ready
  initDisplay();         // now uses dispSettings.rotation
  splashEnd = millis() + 2000;
  setBacklight(brightness);
}

void loop() {
  // Hold splash for 2s
  if (splashEnd > 0 && millis() > splashEnd) {
    splashEnd = 0;
    initWiFi();
    initWebServer();
    initBambuMqtt();
  }

  if (splashEnd > 0) {
    delay(10);
    return;
  }

  handleWiFi();
  handleWebServer();

  if (isWiFiConnected() && !isAPMode()) {
    if (isPrinterConfigured()) {
      handleBambuMqtt();
    }

    // Auto-select screen based on printer state
    BambuState& s = activePrinter().state;
    ScreenState current = getScreenState();

    if (!isPrinterConfigured()) {
      // No printer configured — show idle (user can configure via web)
      if (current != SCREEN_IDLE && current != SCREEN_OFF) {
        setScreenState(SCREEN_IDLE);
        finishScreenStart = 0;
      }
    } else if (!s.connected && current != SCREEN_CONNECTING_MQTT &&
               current != SCREEN_OFF && current != SCREEN_CLOCK) {
      setScreenState(SCREEN_CONNECTING_MQTT);
      finishScreenStart = 0;
    } else if (!s.connected && (current == SCREEN_OFF || current == SCREEN_CLOCK)) {
      // Stay off/clock when printer is disconnected/off
    } else if (s.connected && s.printing) {
      if (current != SCREEN_PRINTING) {
        setScreenState(SCREEN_PRINTING);
        finishScreenStart = 0;
      }
    } else if (s.connected && !s.printing &&
               strcmp(s.gcodeState, "FINISH") == 0) {
      if (current != SCREEN_FINISHED && current != SCREEN_OFF && current != SCREEN_CLOCK) {
        setScreenState(SCREEN_FINISHED);
        finishScreenStart = millis();
      }
      // Auto-off after finish timeout (unless keepDisplayOn)
      if (current == SCREEN_FINISHED && !dpSettings.keepDisplayOn &&
          dpSettings.finishDisplayMins > 0 && finishScreenStart > 0 &&
          millis() - finishScreenStart > (unsigned long)dpSettings.finishDisplayMins * 60000UL) {
        setScreenState(dpSettings.showClockAfterFinish ? SCREEN_CLOCK : SCREEN_OFF);
      }
    } else if (s.connected && !s.printing &&
               strcmp(s.gcodeState, "FINISH") != 0) {
      if (current == SCREEN_OFF || current == SCREEN_CLOCK) {
        // Printer woke up from off/clock state — restore display
        setBacklight(brightness);
      }
      if (current != SCREEN_IDLE) {
        setScreenState(SCREEN_IDLE);
        finishScreenStart = 0;
      }
    }
  }

  updateDisplay();
}
