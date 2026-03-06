#include "bambu_mqtt.h"
#include "bambu_state.h"
#include "settings.h"
#include "display_ui.h"
#include "config.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

// MQTT client objects — lazily allocated on first connect
static WiFiClientSecure* tlsClient = nullptr;
static PubSubClient* mqttClient = nullptr;
static bool initialized = false;
static unsigned long lastReconnectAttempt = 0;
static unsigned long lastPushallRequest = 0;
static uint32_t pushallSeqId = 0;
static unsigned long connectTime = 0;
static bool initialPushallSent = false;

static void mqttCallback(char* topic, byte* payload, unsigned int length);

// ---------------------------------------------------------------------------
//  Lazy allocation of TLS + MQTT clients
// ---------------------------------------------------------------------------
static bool ensureClients() {
  if (tlsClient && mqttClient) return true;

  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < BAMBU_MIN_FREE_HEAP) {
    Serial.printf("Bambu: Not enough heap (%u < %u)\n",
                  freeHeap, BAMBU_MIN_FREE_HEAP);
    return false;
  }

  if (!tlsClient) {
    tlsClient = new (std::nothrow) WiFiClientSecure();
    if (!tlsClient) return false;
  }
  tlsClient->setInsecure();  // self-signed cert on local network

  if (!mqttClient) {
    mqttClient = new (std::nothrow) PubSubClient(*tlsClient);
    if (!mqttClient) {
      delete tlsClient;
      tlsClient = nullptr;
      return false;
    }
  }

  PrinterConfig& cfg = activePrinter().config;
  mqttClient->setServer(cfg.ip, BAMBU_PORT);
  mqttClient->setBufferSize(BAMBU_BUFFER_SIZE);
  mqttClient->setCallback(mqttCallback);
  mqttClient->setKeepAlive(BAMBU_KEEPALIVE);

  initialized = true;
  return true;
}

// ---------------------------------------------------------------------------
//  Request pushall
// ---------------------------------------------------------------------------
static void requestPushall() {
  if (!mqttClient) return;

  PrinterConfig& cfg = activePrinter().config;
  char topic[64];
  snprintf(topic, sizeof(topic), "device/%s/request", cfg.serial);

  char payload[128];
  snprintf(payload, sizeof(payload),
           "{\"pushing\":{\"sequence_id\":\"%u\",\"command\":\"pushall\"}}",
           pushallSeqId++);

  mqttClient->publish(topic, payload);
  lastPushallRequest = millis();
}

// ---------------------------------------------------------------------------
//  Reconnect
// ---------------------------------------------------------------------------
static void reconnect() {
  PrinterConfig& cfg = activePrinter().config;
  if (!cfg.enabled || strlen(cfg.ip) == 0) return;
  if (millis() - lastReconnectAttempt < BAMBU_RECONNECT_INTERVAL) return;

  lastReconnectAttempt = millis();

  if (!ensureClients()) return;
  if (mqttClient->connected()) return;

  char clientId[32];
  snprintf(clientId, sizeof(clientId), "bambu_%08x",
           (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF));

  Serial.printf("Bambu: Connecting to %s...\n", cfg.ip);
  esp_task_wdt_reset();

  if (mqttClient->connect(clientId, BAMBU_USERNAME, cfg.accessCode)) {
    char topic[64];
    snprintf(topic, sizeof(topic), "device/%s/report", cfg.serial);
    mqttClient->subscribe(topic);

    activePrinter().state.connected = true;
    connectTime = millis();
    initialPushallSent = false;
    Serial.println("Bambu: MQTT connected!");
  } else {
    activePrinter().state.connected = false;
    Serial.printf("Bambu: MQTT connect failed (rc=%d)\n",
                  mqttClient->state());
  }
}

// ---------------------------------------------------------------------------
//  MQTT callback — delta merge
// ---------------------------------------------------------------------------
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  esp_task_wdt_reset();

  // Filter document to reduce parse memory
  JsonDocument filter;
  JsonObject pf = filter["print"].to<JsonObject>();
  pf["gcode_state"] = true;
  pf["mc_percent"] = true;
  pf["mc_remaining_time"] = true;
  pf["nozzle_temper"] = true;
  pf["nozzle_target_temper"] = true;
  pf["bed_temper"] = true;
  pf["bed_target_temper"] = true;
  pf["chamber_temper"] = true;
  pf["subtask_name"] = true;
  pf["layer_num"] = true;
  pf["total_layer_num"] = true;
  pf["cooling_fan_speed"] = true;
  pf["big_fan1_speed"] = true;
  pf["big_fan2_speed"] = true;
  pf["heatbreak_fan_speed"] = true;
  pf["wifi_signal"] = true;
  pf["spd_lvl"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length,
                                              DeserializationOption::Filter(filter));
  if (err) return;

  JsonObject print = doc["print"];
  if (print.isNull()) return;

  BambuState& s = activePrinter().state;

  // Delta merge: only update fields present in this message

  if (print["gcode_state"].is<const char*>()) {
    const char* state = print["gcode_state"];
    strncpy(s.gcodeState, state, 15);
    s.gcodeState[15] = '\0';
    s.printing = (strcmp(state, "RUNNING") == 0 ||
                  strcmp(state, "PAUSE") == 0 ||
                  strcmp(state, "PREPARE") == 0);
  }

  if (print["mc_percent"].is<int>())
    s.progress = print["mc_percent"].as<int>();

  if (print["mc_remaining_time"].is<int>())
    s.remainingMinutes = print["mc_remaining_time"].as<int>();

  // Temperature fields (can arrive as int or float)
  if (print["nozzle_temper"].is<float>())
    s.nozzleTemp = print["nozzle_temper"].as<float>();
  else if (print["nozzle_temper"].is<int>())
    s.nozzleTemp = print["nozzle_temper"].as<int>();

  if (print["nozzle_target_temper"].is<float>())
    s.nozzleTarget = print["nozzle_target_temper"].as<float>();
  else if (print["nozzle_target_temper"].is<int>())
    s.nozzleTarget = print["nozzle_target_temper"].as<int>();

  if (print["bed_temper"].is<float>())
    s.bedTemp = print["bed_temper"].as<float>();
  else if (print["bed_temper"].is<int>())
    s.bedTemp = print["bed_temper"].as<int>();

  if (print["bed_target_temper"].is<float>())
    s.bedTarget = print["bed_target_temper"].as<float>();
  else if (print["bed_target_temper"].is<int>())
    s.bedTarget = print["bed_target_temper"].as<int>();

  if (print["chamber_temper"].is<float>())
    s.chamberTemp = print["chamber_temper"].as<float>();
  else if (print["chamber_temper"].is<int>())
    s.chamberTemp = print["chamber_temper"].as<int>();

  if (print["subtask_name"].is<const char*>()) {
    const char* name = print["subtask_name"];
    strncpy(s.subtaskName, name, sizeof(s.subtaskName) - 1);
    s.subtaskName[sizeof(s.subtaskName) - 1] = '\0';
  }

  if (print["layer_num"].is<int>())
    s.layerNum = print["layer_num"].as<int>();

  if (print["total_layer_num"].is<int>())
    s.totalLayers = print["total_layer_num"].as<int>();

  // Fan speeds (Bambu sends 0-15, map to 0-100%)
  if (print["cooling_fan_speed"].is<int>()) {
    int v = print["cooling_fan_speed"].as<int>();
    s.coolingFanPct = (v * 100) / 15;
  }
  if (print["big_fan1_speed"].is<int>()) {
    int v = print["big_fan1_speed"].as<int>();
    s.auxFanPct = (v * 100) / 15;
  }
  if (print["big_fan2_speed"].is<int>()) {
    int v = print["big_fan2_speed"].as<int>();
    s.chamberFanPct = (v * 100) / 15;
  }
  if (print["heatbreak_fan_speed"].is<int>()) {
    int v = print["heatbreak_fan_speed"].as<int>();
    s.heatbreakFanPct = (v * 100) / 15;
  }

  // WiFi signal
  if (print["wifi_signal"].is<int>())
    s.wifiSignal = print["wifi_signal"].as<int>();

  // Speed level (1-4)
  if (print["spd_lvl"].is<int>())
    s.speedLevel = print["spd_lvl"].as<int>();

  s.lastUpdate = millis();
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
void initBambuMqtt() {
  BambuState& s = activePrinter().state;
  memset(&s, 0, sizeof(BambuState));
  s.connected = false;
  s.printing = false;
  strcpy(s.gcodeState, "UNKNOWN");

  PrinterConfig& cfg = activePrinter().config;
  if (!cfg.enabled || strlen(cfg.ip) == 0) {
    if (mqttClient) { delete mqttClient; mqttClient = nullptr; }
    if (tlsClient) { delete tlsClient; tlsClient = nullptr; }
    initialized = false;
    return;
  }

  initialPushallSent = false;
  connectTime = 0;
  lastReconnectAttempt = millis();
}

void handleBambuMqtt() {
  PrinterConfig& cfg = activePrinter().config;
  if (!cfg.enabled) return;

  BambuState& s = activePrinter().state;

  if (!mqttClient || !mqttClient->connected()) {
    s.connected = false;
    reconnect();
  } else {
    mqttClient->loop();

    // Delayed initial pushall
    if (!initialPushallSent && connectTime > 0 &&
        millis() - connectTime > BAMBU_PUSHALL_INITIAL_DELAY) {
      esp_task_wdt_reset();
      requestPushall();
      initialPushallSent = true;
    }

    // Periodic pushall
    if (initialPushallSent &&
        millis() - lastPushallRequest > BAMBU_PUSHALL_INTERVAL) {
      esp_task_wdt_reset();
      requestPushall();
    }
  }

  // Stale timeout
  if (s.lastUpdate > 0 && millis() - s.lastUpdate > BAMBU_STALE_TIMEOUT) {
    if (s.printing) {
      s.printing = false;
    }
  }
}

void disconnectBambuMqtt() {
  if (mqttClient && mqttClient->connected()) {
    mqttClient->disconnect();
  }
  activePrinter().state.connected = false;
}
