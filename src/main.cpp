#include <Arduino.h>
#include "display_ui.h"
#include "settings.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "bambu_mqtt.h"
#include "config.h"
#include "bambu_state.h"

static unsigned long splashEnd = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== BambuHelper Starting ===");

  initDisplay();
  splashEnd = millis() + 2000;

  loadSettings();
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
    handleBambuMqtt();

    // Auto-select screen based on printer state
    BambuState& s = activePrinter().state;
    ScreenState current = getScreenState();

    if (!s.connected && current != SCREEN_CONNECTING_MQTT) {
      setScreenState(SCREEN_CONNECTING_MQTT);
    } else if (s.connected && s.printing && current != SCREEN_PRINTING) {
      setScreenState(SCREEN_PRINTING);
    } else if (s.connected && !s.printing &&
               strcmp(s.gcodeState, "FINISH") == 0 &&
               current != SCREEN_FINISHED) {
      setScreenState(SCREEN_FINISHED);
    } else if (s.connected && !s.printing &&
               strcmp(s.gcodeState, "FINISH") != 0 &&
               current != SCREEN_IDLE) {
      setScreenState(SCREEN_IDLE);
    }
  }

  updateDisplay();
}
