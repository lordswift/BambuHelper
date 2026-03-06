#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void initWiFi();
void handleWiFi();
bool isWiFiConnected();
bool isAPMode();
String getAPSSID();

#endif // WIFI_MANAGER_H
