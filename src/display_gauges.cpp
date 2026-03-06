#include "display_gauges.h"
#include "config.h"
#include "icons.h"

// ---------------------------------------------------------------------------
//  H2-style LED progress bar
// ---------------------------------------------------------------------------
void drawLedProgressBar(TFT_eSPI& tft, int16_t y, uint8_t progress) {
  const int16_t barW = 236;
  const int16_t barH = 5;
  const int16_t barX = (SCREEN_W - barW) / 2;

  // Clear bar area
  tft.fillRect(barX, y, barW, barH, CLR_BG);

  if (progress == 0) return;

  // Fill width
  int16_t fillW = (progress * barW) / 100;
  if (fillW < 1) fillW = 1;

  // Color shifts based on progress
  uint16_t barColor;
  if (progress < 25) {
    barColor = CLR_BLUE;
  } else if (progress < 75) {
    barColor = CLR_GREEN;
  } else if (progress < 100) {
    barColor = CLR_GOLD;
  } else {
    barColor = CLR_GREEN;
  }

  // Draw filled portion with rounded ends
  tft.fillRoundRect(barX, y, fillW, barH, 2, barColor);

  // Glow effect: lighter 1px line in the center of the bar
  uint16_t glowColor = tft.alphaBlend(160, CLR_TEXT, barColor);
  int16_t glowY = y + barH / 2;
  tft.drawFastHLine(barX + 1, glowY, fillW - 2, glowColor);

  // Leading edge bloom: 2px brighter at the tip
  if (fillW > 4 && progress < 100) {
    tft.fillRect(barX + fillW - 3, y, 3, barH, glowColor);
  }

  // Draw dim track for unfilled portion
  if (fillW < barW) {
    tft.fillRoundRect(barX + fillW, y, barW - fillW, barH, 2, CLR_TRACK);
  }
}

// ---------------------------------------------------------------------------
//  Main progress arc (270 degrees)
// ---------------------------------------------------------------------------
void drawProgressArc(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                     int16_t thickness, uint8_t progress, uint8_t prevProgress,
                     uint16_t remainingMin, bool forceRedraw) {
  // Arc from 135 deg to 405 deg (270-degree sweep, gap at bottom)
  const uint16_t startAngle = 135;
  const uint16_t endAngle = 405;  // 135 + 270

  if (forceRedraw) {
    // Draw full background track
    tft.drawSmoothArc(cx, cy, radius, radius - thickness,
                      startAngle, endAngle, CLR_TRACK, CLR_BG, true);
  }

  // Progress fill angle
  uint16_t fillEnd = startAngle + (progress * 270) / 100;
  if (fillEnd > endAngle) fillEnd = endAngle;

  // Pick color based on progress
  uint16_t arcColor;
  if (progress < 25) {
    arcColor = CLR_BLUE;
  } else if (progress < 75) {
    arcColor = CLR_GREEN;
  } else if (progress < 100) {
    arcColor = CLR_GOLD;
  } else {
    arcColor = CLR_GREEN;
  }

  // Draw filled arc
  if (progress > 0) {
    tft.drawSmoothArc(cx, cy, radius, radius - thickness,
                      startAngle, fillEnd, arcColor, CLR_BG, true);
  }

  // If progress decreased, clear the old portion
  if (!forceRedraw && prevProgress > progress) {
    uint16_t oldEnd = startAngle + (prevProgress * 270) / 100;
    if (oldEnd > endAngle) oldEnd = endAngle;
    if (oldEnd > fillEnd) {
      tft.drawSmoothArc(cx, cy, radius, radius - thickness,
                        fillEnd, oldEnd, CLR_TRACK, CLR_BG, true);
    }
  }

  // Center text: percentage (large)
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(CLR_TEXT, CLR_BG);

  // Clear center area for text
  int16_t textR = radius - thickness - 4;
  tft.fillCircle(cx, cy, textR, CLR_BG);

  // Large percentage
  tft.setTextFont(4);
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%d%%", progress);
  tft.drawString(pctBuf, cx, cy - 10);

  // Time remaining (smaller, below)
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  char timeBuf[16];
  if (remainingMin >= 60) {
    snprintf(timeBuf, sizeof(timeBuf), "%dh %dm left",
             remainingMin / 60, remainingMin % 60);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "%dm left", remainingMin);
  }
  tft.drawString(timeBuf, cx, cy + 14);
}

// ---------------------------------------------------------------------------
//  Mini temperature arc gauge (180 degrees)
// ---------------------------------------------------------------------------
void drawTempGauge(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                   float current, float target, float maxTemp,
                   uint16_t accentColor, const char* label,
                   const uint8_t* icon, bool forceRedraw) {
  // 180-degree arc from 180 to 360 (top half)
  const uint16_t startAngle = 180;
  const uint16_t endAngle = 360;

  if (forceRedraw) {
    // Background track
    tft.drawSmoothArc(cx, cy, radius, radius - 6,
                      startAngle, endAngle, CLR_TRACK, CLR_BG, true);
  }

  // Temperature fill
  float ratio = (maxTemp > 0) ? (current / maxTemp) : 0;
  if (ratio > 1.0f) ratio = 1.0f;
  if (ratio < 0.0f) ratio = 0.0f;

  uint16_t fillEnd = startAngle + (uint16_t)(ratio * 180);

  // Color zones: cold (blue) -> warm (accent) -> hot (red)
  uint16_t tempColor;
  if (ratio < 0.3f) {
    tempColor = CLR_BLUE;
  } else if (ratio < 0.7f) {
    tempColor = accentColor;
  } else {
    tempColor = CLR_RED;
  }

  if (forceRedraw && ratio > 0) {
    tft.drawSmoothArc(cx, cy, radius, radius - 6,
                      startAngle, fillEnd, tempColor, CLR_BG, true);
  } else if (ratio > 0) {
    tft.drawSmoothArc(cx, cy, radius, radius - 6,
                      startAngle, fillEnd, tempColor, CLR_BG, true);
  }

  // Clear center area
  tft.fillRect(cx - radius + 8, cy - 14, (radius - 8) * 2, 28, CLR_BG);

  // Current temp (large number inside arc)
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(CLR_TEXT, CLR_BG);
  char tempBuf[12];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", current);
  tft.drawString(tempBuf, cx, cy - 6);

  // Target temp (small, below)
  tft.setTextFont(1);
  tft.setTextColor(CLR_TEXT_DIM, CLR_BG);
  snprintf(tempBuf, sizeof(tempBuf), "/%.0f%cC", target, 0xB0);
  tft.drawString(tempBuf, cx, cy + 8);

  // Label below gauge
  tft.setTextFont(1);
  tft.setTextColor(accentColor, CLR_BG);
  tft.drawString(label, cx, cy + radius + 4);

  // Icon to the left of gauge
  if (icon) {
    drawIcon16(tft, cx - radius - 18, cy - 8, icon, accentColor);
  }
}
