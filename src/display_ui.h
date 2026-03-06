#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <TFT_eSPI.h>

enum ScreenState {
  SCREEN_SPLASH,
  SCREEN_AP_MODE,
  SCREEN_CONNECTING_WIFI,
  SCREEN_CONNECTING_MQTT,
  SCREEN_IDLE,
  SCREEN_PRINTING,
  SCREEN_FINISHED
};

extern TFT_eSPI tft;

void initDisplay();
void updateDisplay();
void setScreenState(ScreenState state);
ScreenState getScreenState();
void setBacklight(uint8_t level);

#endif // DISPLAY_UI_H
