#include "button.h"
#include "settings.h"

#if defined(USE_XPT2046)
  #include <SPI.h>
  #include <XPT2046_Touchscreen.h>
  static SPIClass touchSPI(HSPI);
  static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
  static bool touchReady = false;
#elif defined(TOUCH_CS)
  #include "display_ui.h"  // extern tft for getTouch()
#endif

static bool lastRaw = false;
static bool stableState = false;
static unsigned long lastChangeMs = 0;
static const unsigned long DEBOUNCE_MS = 50;

void initButton() {
  if (buttonType == BTN_DISABLED) return;
#if defined(USE_XPT2046)
  if (buttonType == BTN_TOUCHSCREEN) {
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    touchReady = true;
    Serial.println("XPT2046 touch initialized (separate SPI)");
    return;
  }
#endif
  if (buttonType == BTN_TOUCHSCREEN) return;
  if (buttonPin == 0) return;
  if (buttonType == BTN_PUSH) {
    pinMode(buttonPin, INPUT_PULLUP);
  } else {  // BTN_TOUCH (TTP223)
    pinMode(buttonPin, INPUT);
  }
  lastRaw = false;
  stableState = false;
  lastChangeMs = 0;
}

bool wasButtonPressed() {
  if (buttonType == BTN_DISABLED) return false;

  bool raw;
  if (buttonType == BTN_TOUCHSCREEN) {
#if defined(USE_XPT2046)
    if (!touchReady) return false;
    raw = ts.touched();
#elif defined(TOUCH_CS)
    uint16_t tx, ty;
    raw = tft.getTouch(&tx, &ty);
#else
    return false;
#endif
  } else if (buttonType == BTN_PUSH) {
    if (buttonPin == 0) return false;
    raw = (digitalRead(buttonPin) == LOW);   // active LOW with pull-up
  } else {
    if (buttonPin == 0) return false;
    raw = (digitalRead(buttonPin) == HIGH);  // TTP223: active HIGH
  }

  // Debounce
  if (raw != lastRaw) {
    lastChangeMs = millis();
    lastRaw = raw;
  }
  if ((millis() - lastChangeMs) < DEBOUNCE_MS) return false;

  // Rising edge detection
  bool result = false;
  if (raw && !stableState) {
    result = true;
  }
  stableState = raw;

  return result;
}
