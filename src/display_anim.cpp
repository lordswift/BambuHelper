#include "display_anim.h"
#include "config.h"
#include "icons.h"

// ---------------------------------------------------------------------------
//  Rotating arc spinner
// ---------------------------------------------------------------------------
static uint16_t spinnerAngle = 0;

void drawSpinner(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                 uint16_t color) {
  // Erase previous arc segment
  uint16_t prevStart = (spinnerAngle + 360 - 12) % 360;
  uint16_t prevEnd = (prevStart + 60) % 360;
  if (prevEnd > prevStart) {
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      prevStart, prevEnd, CLR_BG, CLR_BG, true);
  }

  // Advance angle
  spinnerAngle = (spinnerAngle + 12) % 360;
  uint16_t arcStart = spinnerAngle;
  uint16_t arcEnd = (spinnerAngle + 60) % 360;

  // Draw arc segment (handle wrap-around)
  if (arcEnd > arcStart) {
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      arcStart, arcEnd, color, CLR_BG, true);
  } else {
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      arcStart, 360, color, CLR_BG, true);
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      0, arcEnd, color, CLR_BG, true);
  }
}

// ---------------------------------------------------------------------------
//  Animated dots "..."
// ---------------------------------------------------------------------------
void drawAnimDots(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color) {
  unsigned long ms = millis();
  int phase = (ms / 400) % 4;  // 0, 1, 2, 3

  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);

  for (int i = 0; i < 3; i++) {
    uint16_t dotColor = (i < phase) ? color : CLR_TEXT_DARK;
    tft.setTextColor(dotColor, CLR_BG);
    tft.drawString(".", x + i * 8, y);
  }
}

// ---------------------------------------------------------------------------
//  Pulse factor for glow effects
// ---------------------------------------------------------------------------
float getPulseFactor() {
  unsigned long ms = millis();
  // Sine wave oscillation between 0.5 and 1.0, period ~2 seconds
  float t = (ms % 2000) / 2000.0f;
  float sine = sinf(t * 2.0f * PI);
  return 0.75f + 0.25f * sine;
}

// ---------------------------------------------------------------------------
//  Completion animation
// ---------------------------------------------------------------------------
static unsigned long completionStart = 0;
static bool completionDone = false;

void drawCompletionAnim(TFT_eSPI& tft, int16_t cx, int16_t cy, bool reset) {
  if (reset) {
    completionStart = millis();
    completionDone = false;
    return;
  }

  if (completionDone) return;

  unsigned long elapsed = millis() - completionStart;

  // Phase 1 (0-500ms): expanding green ring
  if (elapsed < 500) {
    int16_t r = 10 + (elapsed * 40) / 500;
    tft.drawSmoothArc(cx, cy, r, r - 3, 0, 360, CLR_GREEN, CLR_BG, true);
  }
  // Phase 2 (500-1000ms): checkmark appears
  else if (elapsed < 1000) {
    tft.drawSmoothArc(cx, cy, 50, 47, 0, 360, CLR_GREEN, CLR_BG, true);
    drawIcon16(tft, cx - 8, cy - 8, icon_check, CLR_GREEN);
  }
  // Phase 3 (1000ms+): static, done
  else {
    completionDone = true;
  }
}
