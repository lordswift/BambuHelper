#include "web_server.h"
#include "settings.h"
#include "bambu_state.h"
#include "wifi_manager.h"
#include "config.h"
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer server(80);

// ---------------------------------------------------------------------------
//  HTML page (embedded PROGMEM)
// ---------------------------------------------------------------------------
static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>BambuHelper Setup</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
    background: #0D1117; color: #E6EDF3; padding: 16px;
    max-width: 480px; margin: 0 auto;
  }
  h1 { color: #58A6FF; font-size: 22px; margin-bottom: 6px; }
  .subtitle { color: #8B949E; font-size: 13px; margin-bottom: 20px; }
  .card {
    background: #161B22; border: 1px solid #30363D; border-radius: 8px;
    padding: 16px; margin-bottom: 16px;
  }
  .card h2 { color: #58A6FF; font-size: 16px; margin-bottom: 12px; }
  label { display: block; color: #8B949E; font-size: 13px; margin-bottom: 4px; margin-top: 10px; }
  input[type=text], input[type=password], input[type=number] {
    width: 100%; padding: 8px 10px; border: 1px solid #30363D;
    border-radius: 6px; background: #0D1117; color: #E6EDF3;
    font-size: 14px; outline: none;
  }
  input:focus { border-color: #58A6FF; }
  input[type=range] { width: 100%; margin-top: 6px; }
  .check-row { display: flex; align-items: center; gap: 8px; margin-top: 10px; }
  .check-row input { width: auto; }
  .check-row label { margin: 0; }
  .btn {
    display: block; width: 100%; padding: 10px; border: none;
    border-radius: 6px; font-size: 15px; font-weight: 600;
    cursor: pointer; margin-top: 16px; text-align: center;
  }
  .btn-primary { background: #238636; color: #fff; }
  .btn-primary:hover { background: #2EA043; }
  .btn-danger { background: #DA3633; color: #fff; font-size: 13px; padding: 8px; }
  .btn-danger:hover { background: #F85149; }
  .status {
    padding: 8px 12px; border-radius: 6px; margin-bottom: 12px;
    font-size: 13px; font-weight: 600;
  }
  .status-ok { background: #0D1117; border-left: 3px solid #3FB950; color: #3FB950; }
  .status-off { background: #0D1117; border-left: 3px solid #F85149; color: #F85149; }
  .status-na { background: #0D1117; border-left: 3px solid #8B949E; color: #8B949E; }
  #liveStats { margin-top: 10px; font-size: 12px; color: #8B949E; }
  .stat-row { display: flex; justify-content: space-between; padding: 2px 0; }
  .stat-val { color: #E6EDF3; }
</style>
</head>
<body>
<h1>BambuHelper</h1>
<p class="subtitle">Bambu Lab Printer Monitor</p>

<form method="POST" action="/save">

<div class="card">
  <h2>WiFi Settings</h2>
  <label for="ssid">WiFi SSID</label>
  <input type="text" name="ssid" id="ssid" value="%SSID%" placeholder="Your WiFi name">
  <label for="pass">WiFi Password</label>
  <input type="password" name="pass" id="pass" value="%PASS%" placeholder="WiFi password">
</div>

<div class="card">
  <h2>Printer Settings</h2>
  <div id="printerStatus" class="%STATUS_CLASS%">%STATUS_TEXT%</div>
  <div class="check-row">
    <input type="checkbox" name="enabled" id="enabled" value="1" %ENABLED%>
    <label for="enabled">Enable Monitoring</label>
  </div>
  <label for="pname">Printer Name</label>
  <input type="text" name="pname" id="pname" value="%PNAME%" placeholder="My P1S" maxlength="23">
  <label for="ip">Printer IP Address</label>
  <input type="text" name="ip" id="ip" value="%IP%" placeholder="192.168.1.xxx">
  <label for="serial">Serial Number</label>
  <input type="text" name="serial" id="serial" value="%SERIAL%" placeholder="01P00A000000000" maxlength="19">
  <label for="code">LAN Access Code</label>
  <input type="text" name="code" id="code" value="%CODE%" placeholder="12345678" maxlength="8">
  <div id="liveStats"></div>
</div>

<div class="card">
  <h2>Display</h2>
  <label for="bright">Brightness: <span id="brightVal">%BRIGHT%</span></label>
  <input type="range" name="bright" id="bright" min="10" max="255" value="%BRIGHT%"
         oninput="document.getElementById('brightVal').textContent=this.value">
</div>

<button type="submit" class="btn btn-primary">Save &amp; Restart</button>
</form>

<button class="btn btn-danger" style="margin-top:10px"
        onclick="if(confirm('Reset all settings?'))location='/reset'">Factory Reset</button>

<script>
setInterval(function(){
  fetch('/status').then(r=>r.json()).then(d=>{
    var h='';
    if(d.connected){
      h+='<div class="stat-row"><span>State:</span><span class="stat-val">'+d.state+'</span></div>';
      h+='<div class="stat-row"><span>Nozzle:</span><span class="stat-val">'+d.nozzle+'/'+d.nozzle_t+'&deg;C</span></div>';
      h+='<div class="stat-row"><span>Bed:</span><span class="stat-val">'+d.bed+'/'+d.bed_t+'&deg;C</span></div>';
      if(d.progress>0) h+='<div class="stat-row"><span>Progress:</span><span class="stat-val">'+d.progress+'%</span></div>';
      if(d.fan>0) h+='<div class="stat-row"><span>Fan:</span><span class="stat-val">'+d.fan+'%</span></div>';
    } else {
      h='<span style="color:#8B949E">Not connected</span>';
    }
    document.getElementById('liveStats').innerHTML=h;
    var ps=document.getElementById('printerStatus');
    ps.className='status '+(d.connected?'status-ok':(d.enabled?'status-off':'status-na'));
    ps.textContent=d.connected?'Connected':(d.enabled?'Disconnected':'Disabled');
  }).catch(function(){});
}, 3000);
</script>
</body>
</html>
)rawliteral";

// ---------------------------------------------------------------------------
//  Template processor
// ---------------------------------------------------------------------------
static String processTemplate(const String& html) {
  PrinterConfig& cfg = printers[0].config;
  BambuState& st = printers[0].state;

  String page = html;
  page.replace("%SSID%", wifiSSID);
  page.replace("%PASS%", wifiPass);
  page.replace("%ENABLED%", cfg.enabled ? "checked" : "");
  page.replace("%PNAME%", cfg.name);
  page.replace("%IP%", cfg.ip);
  page.replace("%SERIAL%", cfg.serial);
  page.replace("%CODE%", cfg.accessCode);
  page.replace("%BRIGHT%", String(brightness));

  if (cfg.enabled && st.connected) {
    page.replace("%STATUS_CLASS%", "status status-ok");
    page.replace("%STATUS_TEXT%", "Connected");
  } else if (cfg.enabled) {
    page.replace("%STATUS_CLASS%", "status status-off");
    page.replace("%STATUS_TEXT%", "Disconnected");
  } else {
    page.replace("%STATUS_CLASS%", "status status-na");
    page.replace("%STATUS_TEXT%", "Disabled");
  }

  return page;
}

// ---------------------------------------------------------------------------
//  Route handlers
// ---------------------------------------------------------------------------
static void handleRoot() {
  String html = FPSTR(PAGE_HTML);
  server.send(200, "text/html", processTemplate(html));
}

static void handleSave() {
  // WiFi
  if (server.hasArg("ssid")) {
    strlcpy(wifiSSID, server.arg("ssid").c_str(), sizeof(wifiSSID));
  }
  if (server.hasArg("pass")) {
    strlcpy(wifiPass, server.arg("pass").c_str(), sizeof(wifiPass));
  }

  // Printer 0
  PrinterConfig& cfg = printers[0].config;
  cfg.enabled = server.hasArg("enabled");

  if (server.hasArg("pname")) {
    strlcpy(cfg.name, server.arg("pname").c_str(), sizeof(cfg.name));
  }
  if (server.hasArg("ip")) {
    strlcpy(cfg.ip, server.arg("ip").c_str(), sizeof(cfg.ip));
  }
  if (server.hasArg("serial")) {
    strlcpy(cfg.serial, server.arg("serial").c_str(), sizeof(cfg.serial));
  }
  if (server.hasArg("code")) {
    strlcpy(cfg.accessCode, server.arg("code").c_str(), sizeof(cfg.accessCode));
  }

  // Brightness
  if (server.hasArg("bright")) {
    brightness = server.arg("bright").toInt();
  }

  saveSettings();

  server.send(200, "text/html",
    "<html><body style='background:#0D1117;color:#E6EDF3;text-align:center;padding-top:80px;font-family:sans-serif'>"
    "<h2 style='color:#3FB950'>Settings Saved!</h2>"
    "<p>Restarting...</p></body></html>");

  delay(1000);
  ESP.restart();
}

static void handleStatus() {
  BambuState& st = printers[0].state;
  PrinterConfig& cfg = printers[0].config;

  JsonDocument doc;
  doc["connected"] = st.connected;
  doc["enabled"] = cfg.enabled;
  doc["state"] = st.gcodeState;
  doc["progress"] = st.progress;
  doc["nozzle"] = (int)st.nozzleTemp;
  doc["nozzle_t"] = (int)st.nozzleTarget;
  doc["bed"] = (int)st.bedTemp;
  doc["bed_t"] = (int)st.bedTarget;
  doc["fan"] = st.coolingFanPct;
  doc["layer"] = st.layerNum;
  doc["layers"] = st.totalLayers;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

static void handleReset() {
  server.send(200, "text/html",
    "<html><body style='background:#0D1117;color:#E6EDF3;text-align:center;padding-top:80px;font-family:sans-serif'>"
    "<h2 style='color:#F85149'>Factory Reset</h2>"
    "<p>Restarting...</p></body></html>");
  delay(1000);
  resetSettings();
}

// Captive portal: redirect any unknown request to root
static void handleNotFound() {
  if (isAPMode()) {
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "Not Found");
  }
}

// ---------------------------------------------------------------------------
//  Init & handle
// ---------------------------------------------------------------------------
void initWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/reset", HTTP_GET, handleReset);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Web server started on port 80");
}

void handleWebServer() {
  server.handleClient();
}
