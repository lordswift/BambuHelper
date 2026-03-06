#ifndef DISPLAY_GAUGES_H
#define DISPLAY_GAUGES_H

#include <TFT_eSPI.h>

// Draw H2-style LED progress bar (full-width, top of screen)
void drawLedProgressBar(TFT_eSPI& tft, int16_t y, uint8_t progress);

// Draw large 270-degree progress arc with percentage and time in center
void drawProgressArc(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                     int16_t thickness, uint8_t progress, uint8_t prevProgress,
                     uint16_t remainingMin, bool forceRedraw);

// Draw mini 180-degree temperature arc gauge
void drawTempGauge(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                   float current, float target, float maxTemp,
                   uint16_t accentColor, const char* label,
                   const uint8_t* icon, bool forceRedraw);

#endif // DISPLAY_GAUGES_H
