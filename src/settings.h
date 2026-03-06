#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "bambu_state.h"

extern char wifiSSID[33];
extern char wifiPass[65];
extern uint8_t brightness;

void loadSettings();
void saveSettings();
void savePrinterConfig(uint8_t index);
void resetSettings();

#endif // SETTINGS_H
