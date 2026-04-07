#pragma once
#include <stdint.h>

void  tasmotaInit();
float tasmotaGetWatts();                     // current watts, -1 if unavailable
bool  tasmotaIsOnline();                     // true if data received within last 90s
bool  tasmotaIsActiveForSlot(uint8_t slot);  // online AND assigned to given display slot
void  tasmotaMarkPrintStart();               // call when print begins; records Today kWh baseline
void  tasmotaMarkPrintEnd();                 // call when print ends; freezes session kWh
float tasmotaGetPrintKwhUsed();              // frozen session kWh (Today delta), -1 if unavailable
bool  tasmotaKwhChanged();                   // true (once) when kWh value has updated
