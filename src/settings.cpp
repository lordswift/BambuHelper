#include "settings.h"
#include "config.h"
#include <Preferences.h>

// Global state
PrinterSlot printers[MAX_PRINTERS];
uint8_t activePrinterIndex = 0;
char wifiSSID[33] = {0};
char wifiPass[65] = {0};
uint8_t brightness = 200;

static Preferences prefs;

void loadSettings() {
  prefs.begin(NVS_NAMESPACE, true);  // read-only

  // WiFi credentials
  strlcpy(wifiSSID, prefs.getString("wifiSSID", "").c_str(), sizeof(wifiSSID));
  strlcpy(wifiPass, prefs.getString("wifiPass", "").c_str(), sizeof(wifiPass));

  brightness = prefs.getUChar("bright", 200);
  activePrinterIndex = prefs.getUChar("activePrt", 0);
  if (activePrinterIndex >= MAX_PRINTERS) activePrinterIndex = 0;

  // Load each printer slot
  for (uint8_t i = 0; i < MAX_PRINTERS; i++) {
    char key[16];
    PrinterConfig& cfg = printers[i].config;

    snprintf(key, sizeof(key), "p%d_on", i);
    cfg.enabled = prefs.getBool(key, false);

    snprintf(key, sizeof(key), "p%d_ip", i);
    strlcpy(cfg.ip, prefs.getString(key, "").c_str(), sizeof(cfg.ip));

    snprintf(key, sizeof(key), "p%d_serial", i);
    strlcpy(cfg.serial, prefs.getString(key, "").c_str(), sizeof(cfg.serial));

    snprintf(key, sizeof(key), "p%d_code", i);
    strlcpy(cfg.accessCode, prefs.getString(key, "").c_str(), sizeof(cfg.accessCode));

    snprintf(key, sizeof(key), "p%d_name", i);
    strlcpy(cfg.name, prefs.getString(key, "").c_str(), sizeof(cfg.name));

    // Zero out state
    memset(&printers[i].state, 0, sizeof(BambuState));
    strcpy(printers[i].state.gcodeState, "UNKNOWN");
  }

  prefs.end();
}

void saveSettings() {
  prefs.begin(NVS_NAMESPACE, false);

  prefs.putString("wifiSSID", wifiSSID);
  prefs.putString("wifiPass", wifiPass);
  prefs.putUChar("bright", brightness);
  prefs.putUChar("activePrt", activePrinterIndex);

  for (uint8_t i = 0; i < MAX_PRINTERS; i++) {
    savePrinterConfig(i);
  }

  prefs.end();
}

void savePrinterConfig(uint8_t index) {
  if (index >= MAX_PRINTERS) return;

  // Caller may already have prefs open, or we open ourselves
  bool needOpen = !prefs.isKey("wifiSSID");  // heuristic check
  if (needOpen) prefs.begin(NVS_NAMESPACE, false);

  char key[16];
  PrinterConfig& cfg = printers[index].config;

  snprintf(key, sizeof(key), "p%d_on", index);
  prefs.putBool(key, cfg.enabled);

  snprintf(key, sizeof(key), "p%d_ip", index);
  prefs.putString(key, cfg.ip);

  snprintf(key, sizeof(key), "p%d_serial", index);
  prefs.putString(key, cfg.serial);

  snprintf(key, sizeof(key), "p%d_code", index);
  prefs.putString(key, cfg.accessCode);

  snprintf(key, sizeof(key), "p%d_name", index);
  prefs.putString(key, cfg.name);

  if (needOpen) prefs.end();
}

void resetSettings() {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  ESP.restart();
}
