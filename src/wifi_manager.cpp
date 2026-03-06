#include "wifi_manager.h"
#include "settings.h"
#include "display_ui.h"
#include "config.h"
#include <WiFi.h>
#include <DNSServer.h>

static bool apMode = false;
static DNSServer* dnsServer = nullptr;
static unsigned long disconnectTime = 0;
static String apSSID;

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool isAPMode() {
  return apMode;
}

String getAPSSID() {
  return apSSID;
}

static void startAP() {
  // Build SSID from MAC
  uint32_t mac = (uint32_t)(ESP.getEfuseMac() & 0xFFFF);
  char ssidBuf[32];
  snprintf(ssidBuf, sizeof(ssidBuf), "%s%04X", WIFI_AP_PREFIX, mac);
  apSSID = ssidBuf;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssidBuf, WIFI_AP_PASSWORD);

  // Captive portal: redirect all DNS to our IP
  if (!dnsServer) {
    dnsServer = new DNSServer();
  }
  dnsServer->start(53, "*", WiFi.softAPIP());

  apMode = true;
  disconnectTime = 0;

  Serial.printf("AP started: %s (IP: %s)\n", ssidBuf,
                WiFi.softAPIP().toString().c_str());
  setScreenState(SCREEN_AP_MODE);
}

void initWiFi() {
  // If we have stored credentials, try STA mode
  if (strlen(wifiSSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPass);
    setScreenState(SCREEN_CONNECTING_WIFI);

    Serial.printf("Connecting to WiFi: %s\n", wifiSSID);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_CONNECT_TIMEOUT) {
      delay(100);
      updateDisplay();
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("WiFi connected! IP: %s\n",
                    WiFi.localIP().toString().c_str());
      apMode = false;
      disconnectTime = 0;
      return;
    }

    Serial.println("WiFi connection failed, starting AP");
  }

  // No credentials or connection failed — start AP
  startAP();
}

void handleWiFi() {
  if (apMode) {
    if (dnsServer) dnsServer->processNextRequest();
    return;
  }

  // In STA mode: check for disconnection
  if (WiFi.status() != WL_CONNECTED) {
    if (disconnectTime == 0) {
      disconnectTime = millis();
      Serial.println("WiFi disconnected, waiting for reconnect...");
    } else if (millis() - disconnectTime > WIFI_RECONNECT_TIMEOUT) {
      Serial.println("WiFi timeout, switching to AP mode");
      startAP();
    }
  } else {
    disconnectTime = 0;
  }
}
