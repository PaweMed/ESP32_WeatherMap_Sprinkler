#pragma once
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "Zones.h"
#include "Weather.h"
#include "PushoverClient.h"
#include "Programs.h"
#include "Logs.h"
#include "MQTTClient.h"

// --- Awaryjny HTML strony głównej (gdy nie ma index.html) ---
const char MAIN_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>WeatherMap Sprinkler – Panel awaryjny</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; background: #f3f3f3 !important; margin:0; }
    #header { background: #3685c9; color:#fff; padding:20px 0; text-align:center; box-shadow:0 2px 6px #bbb;}
    .card { max-width: 500px; margin: 40px auto; background: #fff; border-radius: 12px; box-shadow:0 2px 10px #bbb; padding: 30px 24px;}
    h2 { color: #3685c9;}
    .warn { color: #c93c36; font-weight: bold; }
    .info { color: #555; }
    .zone { margin: 12px 0; padding: 10px; border-radius: 6px; background: #f7fafc; display: flex; justify-content: space-between; align-items: center;}
    .zone.on { background: #d1edc1; }
    .zone.off { background: #fae3e3; }
    .zone button { min-width:90px; padding: 7px 0; border-radius: 6px; border: none; font-size:1em; cursor:pointer; }
    .zone.on button { background: #c93c36; color: #fff; }
    .zone.off button { background: #3685c9; color: #fff; }
    .weather-box { margin: 24px 0 12px 0; background: #e7f2fa; border-radius: 8px; padding: 14px;}
    .weather-row { margin: 5px 0;}
    .status-row { margin: 6px 0;}
    @media (max-width:550px) {
      .card { max-width: 98vw; padding: 10vw 2vw;}
    }
  </style>
</head>
<body>
  <div id="header">
    <h1>WeatherMap Sprinkler</h1>
  </div>
  <div class="card">
    <h2>Panel awaryjny</h2>
    <div class="warn" style="margin-bottom:12px;">Brak pliku <b>index.html</b> w pamięci LittleFS.<br>Wyświetlana jest wersja zapasowa panelu!</div>
    <div class="status-row"><b>Status WiFi:</b> <span id="wifi"></span></div>
    <div class="status-row"><b>Adres IP:</b> <span id="ip"></span></div>
    <div class="status-row"><b>Czas:</b> <span id="czas"></span></div>
    <hr>
    <div id="weather" class="weather-box">
      <b>Dane pogodowe:</b>
      <div class="weather-row">Temperatura: <span id="temp">?</span> &deg;C</div>
      <div class="weather-row">Wilgotność: <span id="humidity">?</span> %</div>
      <div class="weather-row">Deszcz (1h): <span id="rain">?</span> mm</div>
      <div class="weather-row">Wiatr: <span id="wind">?</span> m/s</div>
    </div>
    <hr>
    <b>Sterowanie strefami:</b>
    <div id="zones"></div>
    <hr>
    <div class="info">
      <a href="/wifi">Konfiguracja WiFi</a>
      <br><br>
      <button onclick="location.reload()">Odśwież</button>
    </div>
  </div>
  <script>
    function loadStatus() {
      fetch('/api/status').then(r=>r.json()).then(d=>{
        document.getElementById('wifi').textContent = d.wifi;
        document.getElementById('ip').textContent = d.ip;
        document.getElementById('czas').textContent = d.time;
      });
    }
    function loadWeather() {
      fetch('/api/weather').then(r=>r.json()).then(d=>{
        document.getElementById('temp').textContent = d.temp !== undefined ? d.temp : '?';
        document.getElementById('humidity').textContent = d.humidity !== undefined ? d.humidity : '?';
        document.getElementById('rain').textContent = d.rain !== undefined ? d.rain : '?';
        document.getElementById('wind').textContent = d.wind !== undefined ? d.wind : '?';
      });
    }
    function loadZones() {
      fetch('/api/zones').then(r=>r.json()).then(zs=>{
        let html = '';
        for (let z of zs) {
          html += `<div class="zone ${z.active?'on':'off'}">
            <span>Strefa #${z.id+1} ${z.active?'(WŁ)':'(WYŁ)'}</span>
            <button onclick="toggleZone(${z.id}, this)">${z.active?'Wyłącz':'Włącz'}</button>
          </div>`;
        }
        document.getElementById('zones').innerHTML = html;
      });
    }
    function toggleZone(id, btn) {
      btn.disabled = true;
      fetch('/api/zones', {
        method: 'POST',
        headers: {'Content-Type':'application/json'},
        body: JSON.stringify({id: id, toggle: true})
      }).then(r=>r.json()).then(()=>{ setTimeout(loadZones, 400); btn.disabled = false; });
    }
    loadStatus();
    loadWeather();
    loadZones();
  </script>
</body>
</html>
)rawliteral";

// --- NOWA STRONA DO KONFIGURACJI WiFi ---
const char WIFI_CONFIG_PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Konfiguracja WiFi – WeatherMap Sprinkler</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html, body { height: 100%; margin: 0; padding: 0; font-family: Arial, sans-serif; background: #f3f3f3 !important; }
    body { min-height: 100vh; display: flex; flex-direction: column; background: #f3f3f3 !important; }
    #header { background: #3685c9; color: #fff; padding: 20px 0; text-align: center; box-shadow: 0 2px 6px #bbb; flex-shrink: 0; width: 100%; }
    .centerbox-outer { flex: 1 1 auto; display: flex; justify-content: center; align-items: center; min-height: 0; min-width: 0; }
    .card { width: 100%; max-width: 380px; background: #fff; border-radius: 12px; box-shadow: 0 2px 10px #bbb; padding: 32px 24px 24px 24px; margin: 0 auto; display: flex; flex-direction: column; justify-content: center; align-items: stretch; }
    label { font-size: 1.1em; display:block; margin-top: 1em; }
    input[type=text], input[type=password] { width: 100%; padding: 10px; margin-top:6px; margin-bottom:12px; border: 1px solid #bbb; border-radius: 6px; font-size: 1em; }
    button { width: 100%; background: #3685c9; color:#fff; font-size:1.1em; padding:10px 0; border:none; border-radius:6px; cursor:pointer; margin-top: 16px; }
    .msg { color: #3685c9; margin-bottom: 16px; min-height: 1.3em; }
  </style>
</head>
<body>
  <div id="header">
    <h1>WeatherMap Sprinkler</h1>
  </div>
  <div class="centerbox-outer">
    <div class="card">
      <form id="wifiForm" autocomplete="off">
        <div class="msg" id="msg"></div>
        <label for="ssid">Nazwa sieci WiFi (SSID):</label>
        <input type="text" id="ssid" name="ssid" required maxlength="32" autocomplete="off">
        <label for="pass">Hasło WiFi:</label>
        <input type="password" id="pass" name="pass" maxlength="64" autocomplete="off">
        <button type="submit">Zapisz i połącz</button>
      </form>
    </div>
  </div>
  <script>
    document.getElementById('wifiForm').addEventListener('submit', async function(e){
      e.preventDefault();
      const msg = document.getElementById('msg');
      msg.textContent = '';
      const ssid = document.getElementById('ssid').value.trim();
      const pass = document.getElementById('pass').value;
      if (!ssid) { msg.textContent = 'Wpisz nazwę sieci!'; return; }
      msg.textContent = 'Zapisywanie...';
      const resp = await fetch('/api/wifi', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid: ssid, pass: pass })
      });
      const data = await resp.json();
      if (data.ok) {
        msg.textContent = 'Dane zapisane! Urządzenie połączy się z nową siecią WiFi i uruchomi ponownie za chwilę.';
        setTimeout(function(){ location.reload(); }, 4000);
      } else {
        msg.textContent = 'Błąd: ' + (data.error || 'nieznany');
      }
    });
  </script>
</body>
</html>
)rawliteral";

// --- GŁÓWNA PRZESTRZEŃ NAZW DLA SERWERA WEB ---
namespace WebServerUI {
  AsyncWebServer* server = nullptr;

  void begin(
    Config* config,
    void* /*Scheduler* scheduler,*/  // zostawione dla zgodności, obecnie nieużywane
    Zones* relays,                   // Zones pełni funkcję RelayBoard
    Weather* weather,
    PushoverClient* pushover,
    Programs* programs,
    Logs* logs
  ) {
    server = new AsyncWebServer(80);

    // --- Strona główna: index.html z LittleFS lub awaryjny panel ---
    server->on("/", HTTP_GET, [config](AsyncWebServerRequest *req) {
      if (config->isInAPMode()) {
        req->redirect("/wifi");
        return;
      }
      if (LittleFS.exists("/index.html")) {
        req->send(LittleFS, "/index.html", "text/html");
      } else {
        req->send_P(200, "text/html", MAIN_PAGE_HTML);
      }
    });

    // --- Strona konfiguracji WiFi ---
    server->on("/wifi", HTTP_GET, [config](AsyncWebServerRequest *req) {
      if (config->isInAPMode()) {
        req->send_P(200, "text/html", WIFI_CONFIG_PAGE_HTML);
      } else {
        req->redirect("/");
      }
    });

    // --- API: Zapisz WiFi (POST) ---
    server->on("/api/wifi", HTTP_POST, [config](AsyncWebServerRequest *request){
      request->onBody([request, config](const uint8_t *data, size_t len, size_t, size_t) {
        String body = String((const char*)data, len);
        DynamicJsonDocument doc(256);
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
          request->send(400, "application/json", "{\"ok\":false,\"error\":\"Błąd JSON\"}");
          return;
        }
        String ssid = doc["ssid"] | "";
        String pass = doc["pass"] | "";
        if (ssid == "") {
          request->send(400, "application/json", "{\"ok\":false,\"error\":\"Brak SSID\"}");
          return;
        }
        DynamicJsonDocument cfg(256);
        cfg["ssid"] = ssid;
        cfg["pass"] = pass;
        config->saveFromJson(cfg);
        request->send(200, "application/json", "{\"ok\":true}");
        delay(800);
        ESP.restart();
      });
    });

    // --- Endpoint statusu (API) ---
    server->on("/api/status", HTTP_GET, [config](AsyncWebServerRequest *req) {
      String json = "{";
      json += "\"wifi\":\"";
      json += (WiFi.status() == WL_CONNECTED) ? "Połączono" : "Brak połączenia";
      json += "\",";
      json += "\"ip\":\"";
      json += (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "-";
      json += "\",";
      json += "\"time\":\"";
      time_t now = time(nullptr);
      struct tm t;
      localtime_r(&now, &t);
      char buf[32];
      snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
               t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min);
      json += buf;
      json += "\"";
      json += "}";
      req->send(200, "application/json", json);
    });

    // --- Dane pogodowe ---
    server->on("/api/weather", HTTP_GET, [weather](AsyncWebServerRequest *req) {
      DynamicJsonDocument doc(512);
      weather->toJson(doc);
      String out;
      serializeJson(doc, out);
      req->send(200, "application/json", out);
    });

    // --- Lista stref (status przekaźników) ---
    server->on("/api/zones", HTTP_GET, [relays](AsyncWebServerRequest *req) {
      DynamicJsonDocument doc(512);
      JsonArray arr = doc.to<JsonArray>();
      for (int i = 0; i < 8; ++i) {
        JsonObject z = arr.createNestedObject();
        z["id"] = i;
        z["active"] = relays->getZoneState(i);
      }
      String out;
      serializeJson(doc, out);
      req->send(200, "application/json", out);
    });

    // --- Włącz/Wyłącz strefę ---
    server->on("/api/zones", HTTP_POST, [relays](AsyncWebServerRequest *req) {
      req->onBody([req, relays](const uint8_t *data, size_t len, size_t, size_t) {
        DynamicJsonDocument doc(128);
        DeserializationError err = deserializeJson(doc, (const char*)data, len);
        if (err) {
          req->send(400, "application/json", "{\"ok\":false}");
          return;
        }
        int id = doc["id"] | -1;
        bool toggle = doc["toggle"] | false;
        if (id < 0 || id >= 8) {
          req->send(400, "application/json", "{\"ok\":false}");
          return;
        }
        if (toggle) {
          relays->toggleZone(id);
        }
        req->send(200, "application/json", "{\"ok\":true}");
      });
    });

    // === API HARMONOGRAMÓW ===

    // Pobranie wszystkich programów
    server->on("/api/programs", HTTP_GET, [programs](AsyncWebServerRequest *req){
      DynamicJsonDocument doc(4096);
      programs->toJson(doc);
      String json;
      serializeJson(doc, json);
      req->send(200, "application/json", json);
    });

    // Dodanie nowego programu
    server->on("/api/programs", HTTP_POST, [programs](AsyncWebServerRequest *request){
      request->onBody([request, programs](const uint8_t *data, size_t len, size_t, size_t){
        String body = String((const char*)data, len);
        DynamicJsonDocument doc(1024);
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
          request->send(400, "application/json", "{\"ok\":false}");
          return;
        }
        programs->addFromJson(doc);
        request->send(200, "application/json", "{\"ok\":true}");
      });
    });

    // Edycja programu (PUT)
    server->on("^/api/programs/([0-9]+)$", HTTP_PUT, [programs](AsyncWebServerRequest *request){
      int idx = request->pathArg(0).toInt();
      request->onBody([request, programs, idx](const uint8_t *data, size_t len, size_t, size_t){
        String body = String((const char*)data, len);
        DynamicJsonDocument doc(1024);
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
          request->send(400, "application/json", "{\"ok\":false}");
          return;
        }
        programs->edit(idx, doc);
        request->send(200, "application/json", "{\"ok\":true}");
      });
    });

    // Usunięcie programu
    server->on("^/api/programs/([0-9]+)$", HTTP_DELETE, [programs](AsyncWebServerRequest *req){
      int idx = req->pathArg(0).toInt();
      programs->remove(idx);
      req->send(200, "application/json", "{\"ok\":true}");
    });

    // Wyczyść wszystkie programy
    server->on("/api/programs", HTTP_DELETE, [programs](AsyncWebServerRequest *req){
      programs->clear();
      req->send(200, "application/json", "{\"ok\":true}");
    });

    // Eksport programów (backup)
    server->on("/api/programs/export", HTTP_GET, [programs](AsyncWebServerRequest *req){
      DynamicJsonDocument doc(4096);
      programs->toJson(doc);
      String json;
      serializeJson(doc, json);
      req->send(200, "application/json", json);
    });

    // Import programów (przywrócenie backupu)
    server->on("/api/programs/import", HTTP_POST, [programs](AsyncWebServerRequest *request){
      request->onBody([request, programs](const uint8_t *data, size_t len, size_t, size_t){
        String body = String((const char*)data, len);
        DynamicJsonDocument doc(4096);
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
          request->send(400, "application/json", "{\"ok\":false}");
          return;
        }
        programs->importFromJson(doc);
        request->send(200, "application/json", "{\"ok\":true}");
      });
    });

    // --- LOGI: pobierz lub usuń ---
    if (logs) {
      server->on("/api/logs", HTTP_GET, [logs](AsyncWebServerRequest *req){
        DynamicJsonDocument doc(2048);
        logs->toJson(doc);
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
      });
      server->on("/api/logs", HTTP_DELETE, [logs](AsyncWebServerRequest *req){
        logs->clear();
        req->send(200, "application/json", "{\"ok\":true}");
      });
    }

    // --- USTAWIENIA: pobierz lub zapisz ---
    server->on("/api/settings", HTTP_GET, [config](AsyncWebServerRequest *req){
      DynamicJsonDocument doc(512);
      config->toJson(doc);
      String json;
      serializeJson(doc, json);
      req->send(200, "application/json", json);
    });
    server->on("/api/settings", HTTP_POST, [config](AsyncWebServerRequest *request){
      request->onBody([request, config](const uint8_t *data, size_t len, size_t, size_t){
        String body = String((const char*)data, len);
        DynamicJsonDocument doc(512);
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
          request->send(400, "application/json", "{\"ok\":false}");
          return;
        }
        config->saveFromJson(doc);
        request->send(200, "application/json", "{\"ok\":true}");
        // Dynamiczna zmiana MQTT bez resetu (wymaga globalnego mqtt)
        mqtt.updateConfig();
      });
    });

    server->begin();
  }

  void loop() {
    // tymczasowo puste, jeśli trzeba odświeżać web z poziomu ESP – dodaj tutaj
  }
}
