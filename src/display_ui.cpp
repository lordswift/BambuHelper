#include "display_ui.h"
#include "display_gauges.h"
#include "display_anim.h"
#include "icons.h"
#include "config.h"
#include "bambu_state.h"

TFT_eSPI tft = TFT_eSPI();

static ScreenState currentScreen = SCREEN_SPLASH;
static ScreenState prevScreen = SCREEN_SPLASH;
static bool forceRedraw = true;
static unsigned long lastDisplayUpdate = 0;

// Previous state for smart redraw
static BambuState prevState;

// ---------------------------------------------------------------------------
//  Backlight
// ---------------------------------------------------------------------------
void setBacklight(uint8_t level) {
#ifdef BACKLIGHT_PIN
  ledcWrite(BACKLIGHT_CH, level);
#endif
}

// ---------------------------------------------------------------------------
//  Init
// ---------------------------------------------------------------------------
void initDisplay() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(CLR_BG);

#ifdef BACKLIGHT_PIN
  ledcSetup(BACKLIGHT_CH, BACKLIGHT_FREQ, BACKLIGHT_RES);
  ledcAttachPin(BACKLIGHT_PIN, BACKLIGHT_CH);
  setBacklight(200);
#endif

  memset(&prevState, 0, sizeof(prevState));

  // Splash screen
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(CLR_GREEN, CLR_BG);
  tft.setTextFont(4);
  tft.drawString("BambuHelper", SCREEN_W / 2, SCREEN_H / 2 - 20);
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.drawString("Printer Monitor", SCREEN_W / 2, SCREEN_H / 2 + 10);
  tft.setTextFont(1);
  tft.drawString("v1.0", SCREEN_W / 2, SCREEN_H / 2 + 30);
}

void setScreenState(ScreenState state) {
  currentScreen = state;
}

ScreenState getScreenState() {
  return currentScreen;
}

// ---------------------------------------------------------------------------
//  Speed level name helper
// ---------------------------------------------------------------------------
static const char* speedLevelName(uint8_t level) {
  switch (level) {
    case 1: return "Silent";
    case 2: return "Std";
    case 3: return "Sport";
    case 4: return "Ludicr";
    default: return "---";
  }
}

static uint16_t speedLevelColor(uint8_t level) {
  switch (level) {
    case 1: return CLR_BLUE;
    case 2: return CLR_GREEN;
    case 3: return CLR_ORANGE;
    case 4: return CLR_RED;
    default: return CLR_TEXT_DIM;
  }
}

// ---------------------------------------------------------------------------
//  Screen: AP Mode
// ---------------------------------------------------------------------------
static void drawAPMode() {
  tft.setTextDatum(MC_DATUM);

  // Title
  tft.setTextColor(CLR_GREEN, CLR_BG);
  tft.setTextFont(4);
  tft.drawString("WiFi Setup", SCREEN_W / 2, 40);

  // Instructions
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.drawString("Connect to WiFi:", SCREEN_W / 2, 80);

  // AP SSID
  tft.setTextColor(CLR_CYAN, CLR_BG);
  tft.setTextFont(4);
  char ssid[32];
  uint32_t mac = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
  snprintf(ssid, sizeof(ssid), "%s%04X", WIFI_AP_PREFIX, mac);
  tft.drawString(ssid, SCREEN_W / 2, 110);

  // Password
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.drawString("Password:", SCREEN_W / 2, 140);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.drawString(WIFI_AP_PASSWORD, SCREEN_W / 2, 158);

  // IP
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.drawString("Then open:", SCREEN_W / 2, 185);
  tft.setTextColor(CLR_ORANGE, CLR_BG);
  tft.setTextFont(4);
  tft.drawString("192.168.4.1", SCREEN_W / 2, 210);
}

// ---------------------------------------------------------------------------
//  Screen: Connecting WiFi
// ---------------------------------------------------------------------------
static void drawConnectingWiFi() {
  tft.setTextDatum(MC_DATUM);

  drawSpinner(tft, SCREEN_W / 2, SCREEN_H / 2 - 30, 35, CLR_BLUE);

  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.drawString("Connecting to WiFi", SCREEN_W / 2, SCREEN_H / 2 + 20);

  drawAnimDots(tft, SCREEN_W / 2 + 54, SCREEN_H / 2 + 14, CLR_TEXT);
}

// ---------------------------------------------------------------------------
//  Screen: Connecting MQTT
// ---------------------------------------------------------------------------
static void drawConnectingMQTT() {
  tft.setTextDatum(MC_DATUM);

  drawSpinner(tft, SCREEN_W / 2, SCREEN_H / 2 - 30, 35, CLR_ORANGE);

  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  tft.drawString("Connecting to Printer", SCREEN_W / 2, SCREEN_H / 2 + 20);

  drawAnimDots(tft, SCREEN_W / 2 + 64, SCREEN_H / 2 + 14, CLR_TEXT);

  // Show printer name/IP
  PrinterSlot& p = activePrinter();
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.setTextFont(1);
  tft.drawString(p.config.ip, SCREEN_W / 2, SCREEN_H / 2 + 42);
}

// ---------------------------------------------------------------------------
//  Screen: Idle (connected, not printing)
// ---------------------------------------------------------------------------
static void drawIdle() {
  PrinterSlot& p = activePrinter();
  BambuState& s = p.state;

  tft.setTextDatum(MC_DATUM);

  // Printer name
  tft.setTextColor(CLR_GREEN, CLR_BG);
  tft.setTextFont(4);
  const char* name = (p.config.name[0] != '\0') ? p.config.name : "Bambu P1S";
  tft.drawString(name, SCREEN_W / 2, 30);

  // Status badge
  tft.setTextFont(2);
  uint16_t stateColor = CLR_TEXT_DIM;
  const char* stateStr = s.gcodeState;
  if (strcmp(s.gcodeState, "IDLE") == 0) {
    stateColor = CLR_GREEN;
    stateStr = "Ready";
  } else if (strcmp(s.gcodeState, "UNKNOWN") == 0 || s.gcodeState[0] == '\0') {
    stateStr = "Waiting...";
  }
  tft.setTextColor(stateColor, CLR_BG);
  tft.drawString(stateStr, SCREEN_W / 2, 60);

  // Connected indicator
  tft.fillCircle(SCREEN_W / 2, 85, 5, s.connected ? CLR_GREEN : CLR_RED);

  // Nozzle temp gauge
  drawTempGauge(tft, SCREEN_W / 2 - 55, 140, 30,
                s.nozzleTemp, s.nozzleTarget, 300.0f,
                CLR_ORANGE, "Nozzle", icon_nozzle, forceRedraw);

  // Bed temp gauge
  drawTempGauge(tft, SCREEN_W / 2 + 55, 140, 30,
                s.bedTemp, s.bedTarget, 120.0f,
                CLR_CYAN, "Bed", icon_bed, forceRedraw);

  // WiFi signal at bottom
  tft.setTextFont(1);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  tft.setTextDatum(BC_DATUM);
  char wifiBuf[24];
  snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: %d dBm", s.wifiSignal);
  tft.drawString(wifiBuf, SCREEN_W / 2, SCREEN_H - 5);
}

// ---------------------------------------------------------------------------
//  Screen: Printing (main dashboard)
// ---------------------------------------------------------------------------
static void drawPrinting() {
  PrinterSlot& p = activePrinter();
  BambuState& s = p.state;

  bool progChanged = forceRedraw || (s.progress != prevState.progress);
  bool tempChanged = forceRedraw ||
                     (s.nozzleTemp != prevState.nozzleTemp) ||
                     (s.nozzleTarget != prevState.nozzleTarget) ||
                     (s.bedTemp != prevState.bedTemp) ||
                     (s.bedTarget != prevState.bedTarget);
  bool infoChanged = forceRedraw ||
                     (s.layerNum != prevState.layerNum) ||
                     (s.totalLayers != prevState.totalLayers) ||
                     (strcmp(s.subtaskName, prevState.subtaskName) != 0);
  bool metricsChanged = forceRedraw ||
                        (s.coolingFanPct != prevState.coolingFanPct) ||
                        (s.speedLevel != prevState.speedLevel) ||
                        (s.wifiSignal != prevState.wifiSignal);
  bool stateChanged = forceRedraw ||
                      (strcmp(s.gcodeState, prevState.gcodeState) != 0);

  // === H2-style LED progress bar (y=0-5) ===
  if (progChanged) {
    drawLedProgressBar(tft, 0, s.progress);
  }

  // === Header bar (y=7-25) ===
  if (forceRedraw || stateChanged) {
    tft.fillRect(0, 7, SCREEN_W, 20, CLR_HEADER);

    // Printer name (left)
    tft.setTextDatum(ML_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(CLR_TEXT, CLR_HEADER);
    const char* name = (p.config.name[0] != '\0') ? p.config.name : "Bambu P1S";
    tft.drawString(name, 6, 17);

    // State badge (right)
    uint16_t badgeColor = CLR_TEXT_DIM;
    if (strcmp(s.gcodeState, "RUNNING") == 0) badgeColor = CLR_GREEN;
    else if (strcmp(s.gcodeState, "PAUSE") == 0) badgeColor = CLR_YELLOW;
    else if (strcmp(s.gcodeState, "FAILED") == 0) badgeColor = CLR_RED;
    else if (strcmp(s.gcodeState, "PREPARE") == 0) badgeColor = CLR_BLUE;

    tft.fillCircle(SCREEN_W - 60, 17, 4, badgeColor);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(badgeColor, CLR_HEADER);
    tft.setTextFont(1);
    tft.drawString(s.gcodeState, SCREEN_W - 53, 17);
  }

  // === Main progress arc (center, y=28-148) ===
  if (progChanged || forceRedraw) {
    drawProgressArc(tft, SCREEN_W / 2, 88, 58, 10,
                    s.progress, prevState.progress,
                    s.remainingMinutes, forceRedraw);
  }

  // === Temperature mini-gauges (y=148-198) ===
  if (tempChanged) {
    drawTempGauge(tft, 60, 172, 26,
                  s.nozzleTemp, s.nozzleTarget, 300.0f,
                  CLR_ORANGE, "Nozzle", icon_nozzle, forceRedraw);

    drawTempGauge(tft, 180, 172, 26,
                  s.bedTemp, s.bedTarget, 120.0f,
                  CLR_CYAN, "Bed", icon_bed, forceRedraw);
  }

  // === Metrics row (y=200-216) ===
  if (metricsChanged) {
    tft.fillRect(0, 200, SCREEN_W, 16, CLR_BG);

    tft.setTextFont(1);
    tft.setTextDatum(TL_DATUM);

    // Fan icon + percentage
    drawIcon16(tft, 4, 200, icon_fan, CLR_TEXT_DIM);
    tft.setTextColor(CLR_TEXT, CLR_BG);
    char fanBuf[8];
    snprintf(fanBuf, sizeof(fanBuf), "%d%%", s.coolingFanPct);
    tft.drawString(fanBuf, 22, 204);

    // Speed level badge
    tft.setTextColor(speedLevelColor(s.speedLevel), CLR_BG);
    tft.drawString(speedLevelName(s.speedLevel), 65, 204);

    // WiFi signal
    drawIcon16(tft, 130, 200, icon_wifi,
               (s.wifiSignal > -60) ? CLR_GREEN :
               (s.wifiSignal > -75) ? CLR_YELLOW : CLR_RED);
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%ddBm", s.wifiSignal);
    tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
    tft.drawString(rssi, 148, 204);
  }

  // === Info row (y=220-238) ===
  if (infoChanged) {
    tft.fillRect(0, 220, SCREEN_W, 20, CLR_BG);

    tft.setTextFont(1);
    tft.setTextDatum(TL_DATUM);

    // Layer icon + count
    drawIcon16(tft, 4, 222, icon_layers, CLR_TEXT_DIM);
    tft.setTextColor(CLR_TEXT, CLR_BG);
    char layerBuf[16];
    snprintf(layerBuf, sizeof(layerBuf), "%d/%d", s.layerNum, s.totalLayers);
    tft.drawString(layerBuf, 22, 226);

    // File icon + name (truncated)
    drawIcon16(tft, 90, 222, icon_file, CLR_TEXT_DIM);
    tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
    char truncName[20];
    strncpy(truncName, s.subtaskName, 19);
    truncName[19] = '\0';
    tft.drawString(truncName, 108, 226);
  }
}

// ---------------------------------------------------------------------------
//  Screen: Finished
// ---------------------------------------------------------------------------
static void drawFinished() {
  PrinterSlot& p = activePrinter();
  BambuState& s = p.state;

  if (forceRedraw) {
    // H2-style full bar at 100%
    drawLedProgressBar(tft, 0, 100);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(CLR_GREEN, CLR_BG);
    tft.setTextFont(4);
    tft.drawString("Print Complete!", SCREEN_W / 2, 40);

    // Start completion animation
    drawCompletionAnim(tft, SCREEN_W / 2, SCREEN_H / 2, true);
  }

  // Animate
  drawCompletionAnim(tft, SCREEN_W / 2, SCREEN_H / 2, false);

  // File name
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  if (s.subtaskName[0] != '\0') {
    char truncName[26];
    strncpy(truncName, s.subtaskName, 25);
    truncName[25] = '\0';
    tft.drawString(truncName, SCREEN_W / 2, SCREEN_H - 30);
  }
}

// ---------------------------------------------------------------------------
//  Main update (called from loop)
// ---------------------------------------------------------------------------
void updateDisplay() {
  unsigned long now = millis();
  if (now - lastDisplayUpdate < DISPLAY_UPDATE_MS) return;
  lastDisplayUpdate = now;

  // Detect screen change
  if (currentScreen != prevScreen) {
    tft.fillScreen(CLR_BG);
    forceRedraw = true;
    prevScreen = currentScreen;
  }

  switch (currentScreen) {
    case SCREEN_SPLASH:
      // Splash shown in initDisplay(), auto-advance handled by main.cpp
      break;

    case SCREEN_AP_MODE:
      if (forceRedraw) drawAPMode();
      break;

    case SCREEN_CONNECTING_WIFI:
      drawConnectingWiFi();
      break;

    case SCREEN_CONNECTING_MQTT:
      drawConnectingMQTT();
      break;

    case SCREEN_IDLE:
      drawIdle();
      break;

    case SCREEN_PRINTING:
      drawPrinting();
      break;

    case SCREEN_FINISHED:
      drawFinished();
      break;
  }

  // Save state for next smart-redraw comparison
  memcpy(&prevState, &activePrinter().state, sizeof(BambuState));
  forceRedraw = false;
}
