#pragma once
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

// Deklaracje funkcji
void setZoneState(int zone, bool state);
void saveSchedules();
void saveConfiguration(const char *filename, const Config &config);

String getSchedulesJson() {
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();
    for (int i = 0; i < NUM_ZONES; i++) {
        JsonObject obj = array.createNestedObject();
        obj["enabled"] = schedules[i].enabled;
        obj["zone"] = schedules[i].zone;
        obj["start_hour"] = schedules[i].start_hour;
        obj["start_minute"] = schedules[i].start_minute;
        obj["duration_minutes"] = schedules[i].duration_minutes;
        obj["days_of_week"] = schedules[i].days_of_week;
    }
    String output;
    serializeJson(doc, output);
    return output;
}

String getStatusJson() {
    DynamicJsonDocument doc(1024);
    struct tm timeinfo;
    getLocalTime(&timeinfo, 5000);
    doc["time"] = asctime(&timeinfo);

    JsonArray zones = doc.createNestedArray("zones");
    for(int i=0; i<NUM_ZONES; i++){
      zones.add(bitRead(zonesState, i));
    }
    
    doc["rain_forecast"] = forecasted_rain;
    doc["rain_threshold"] = config.rain_threshold;
    doc["is_rain_delay_active"] = isRainExpected();
    
    String output;
    serializeJson(doc, output);
    return output;
}


void initWebServer() {
    // Serwowanie głównej strony
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Sprinkler</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f4f4f4; }
        .container { max-width: 800px; margin: auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        h1, h2 { color: #333; }
        .zone, .schedule, .status-item { border: 1px solid #ddd; padding: 15px; margin-bottom: 15px; border-radius: 5px; }
        .zone-name { font-weight: bold; font-size: 1.2em; }
        button { padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; background: #007bff; color: white; margin-right: 10px; }
        button.on { background: #28a745; }
        button.off { background: #dc3545; }
        input, select { padding: 8px; width: calc(100% - 18px); margin-bottom: 10px; border-radius: 4px; border: 1px solid #ccc; }
        .day-selector label { margin-right: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Sprinkler Control</h1>
        
        <div class="status">
            <h2>Status</h2>
            <div id="status-content" class="status-item"></div>
        </div>

        <div class="manual-control">
            <h2>Manual Control</h2>
            <div id="zones-container"></div>
        </div>

        <div class="schedules">
            <h2>Schedules</h2>
            <div id="schedules-container"></div>
            <button onclick="saveSchedules()">Save Schedules</button>
        </div>

        <div class="settings">
            <h2>Settings</h2>
            <form id="settings-form">
                SSID: <input type="text" name="wifi_ssid"><br>
                Password: <input type="password" name="wifi_password"><br>
                OpenWeatherMap API Key: <input type="text" name="owm_api_key"><br>
                City (e.g., Warsaw,PL): <input type="text" name="owm_city"><br>
                Rain Threshold (mm): <input type="number" step="0.1" name="rain_threshold"><br>
                <button type="button" onclick="saveSettings()">Save Settings</button>
            </form>
        </div>
    </div>

<script>
function updateStatus() {
    fetch('/status').then(r => r.json()).then(data => {
        const content = `
            <p><strong>Current Time:</strong> ${data.time}</p>
            <p><strong>Rain Forecast (12h):</strong> ${data.rain_forecast.toFixed(2)} mm</p>
            <p><strong>Watering disabled if rain ></strong> ${data.rain_threshold.toFixed(2)} mm</p>
            <p><strong>Watering Blocked by Weather:</strong> ${data.is_rain_delay_active ? 'YES' : 'NO'}</p>
            <p><strong>Active Zones:</strong> ${data.zones.map((s, i) => s ? `Zone ${i+1}` : '').filter(Boolean).join(', ') || 'None'}</p>
        `;
        document.getElementById('status-content').innerHTML = content;
    });
}

function loadZones() {
    fetch('/status').then(r => r.json()).then(data => {
        const container = document.getElementById('zones-container');
        container.innerHTML = '';
        data.zones.forEach((isOn, i) => {
            const zoneDiv = document.createElement('div');
            zoneDiv.className = 'zone';
            zoneDiv.innerHTML = `
                <span class="zone-name">Zone ${i + 1}</span>
                <button class="${isOn ? 'on' : 'off'}" onclick="toggleZone(${i}, ${!isOn})">
                    ${isOn ? 'Turn OFF' : 'Turn ON'}
                </button>
            `;
            container.appendChild(zoneDiv);
        });
    });
}

function toggleZone(zone, state) {
    fetch(`/control?zone=${zone}&state=${state ? 1 : 0}`).then(() => {
        setTimeout(loadZones, 500); // Daj czas na aktualizację stanu
    });
}

function loadSchedules() {
    fetch('/schedules').then(r => r.json()).then(data => {
        const container = document.getElementById('schedules-container');
        container.innerHTML = '';
        data.forEach((s, i) => {
            const scheduleDiv = document.createElement('div');
            scheduleDiv.className = 'schedule';
            scheduleDiv.dataset.zone = i;
            scheduleDiv.innerHTML = \`
                <h3>Zone ${i + 1}</h3>
                <input type="checkbox" id="enabled_${i}" ${s.enabled ? 'checked' : ''}> Enable<br>
                Start Time: <input type="number" id="hour_${i}" value="${s.start_hour}" min="0" max="23"> : <input type="number" id="minute_${i}" value="${s.start_minute}" min="0" max="59"><br>
                Duration (minutes): <input type="number" id="duration_${i}" value="${s.duration_minutes}" min="1"><br>
                Days: 
                <div class="day-selector">
                    <label><input type="checkbox" data-day="1" ${s.days_of_week & 1<<1 ? 'checked' : ''}> Mon</label>
                    <label><input type="checkbox" data-day="2" ${s.days_of_week & 1<<2 ? 'checked' : ''}> Tue</label>
                    <label><input type="checkbox" data-day="3" ${s.days_of_week & 1<<3 ? 'checked' : ''}> Wed</label>
                    <label><input type="checkbox" data-day="4" ${s.days_of_week & 1<<4 ? 'checked' : ''}> Thu</label>
                    <label><input type="checkbox" data-day="5" ${s.days_of_week & 1<<5 ? 'checked' : ''}> Fri</label>
                    <label><input type="checkbox" data-day="6" ${s.days_of_week & 1<<6 ? 'checked' : ''}> Sat</label>
                    <label><input type="checkbox" data-day="7" ${s.days_of_week & 1<<7 ? 'checked' : ''}> Sun</label>
                </div>
            \`;
            container.appendChild(scheduleDiv);
        });
    });
}

function saveSchedules() {
    const schedules = [];
    document.querySelectorAll('.schedule').forEach(div => {
        const i = div.dataset.zone;
        let days_of_week = 0;
        div.querySelectorAll('.day-selector input:checked').forEach(d => {
            days_of_week |= (1 << d.dataset.day);
        });
        schedules.push({
            zone: i,
            enabled: document.getElementById(\`enabled_${i}\`).checked,
            start_hour: parseInt(document.getElementById(\`hour_${i}\`).value),
            start_minute: parseInt(document.getElementById(\`minute_${i}\`).value),
            duration_minutes: parseInt(document.getElementById(\`duration_${i}\`).value),
            days_of_week: days_of_week
        });
    });

    fetch('/schedules', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(schedules)
    }).then(res => alert('Schedules saved!'));
}

function saveSettings() {
    const form = document.getElementById('settings-form');
    const data = {
        wifi_ssid: form.wifi_ssid.value,
        wifi_password: form.wifi_password.value,
        owm_api_key: form.owm_api_key.value,
        owm_city: form.owm_city.value,
        rain_threshold: parseFloat(form.rain_threshold.value)
    };
    fetch('/settings', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
    }).then(res => alert('Settings saved! The device will now reboot to apply WiFi changes if any.'));
}

function loadSettings() {
    fetch('/settings').then(r => r.json()).then(data => {
        const form = document.getElementById('settings-form');
        form.wifi_ssid.value = data.wifi_ssid;
        form.owm_api_key.value = data.owm_api_key;
        form.owm_city.value = data.owm_city;
        form.rain_threshold.value = data.rain_threshold;
    });
}

window.onload = () => {
    updateStatus();
    loadZones();
    loadSchedules();
    loadSettings();
    setInterval(updateStatus, 5000);
};
</script>
</body>
</html>
        )rawliteral";
        request->send(200, "text/html", html);
    });

    // API endpoints
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", getStatusJson());
    });

    server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request){
        int zone = request->getParam("zone")->value().toInt();
        bool state = request->getParam("state")->value().toInt() == 1;
        setZoneState(zone, state);
        request->send(200, "text/plain", "OK");
    });
    
    server.on("/schedules", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "application/json", getSchedulesJson());
    });
    
    AsyncCallbackJsonWebHandler* scheduleHandler = new AsyncCallbackJsonWebHandler("/schedules", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonArray array = json.as<JsonArray>();
        if (array.isNull()) {
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }

        int i = 0;
        for (JsonObject obj : array) {
            if (i >= NUM_ZONES) break;
            schedules[i].enabled = obj["enabled"];
            schedules[i].start_hour = obj["start_hour"];
            schedules[i].start_minute = obj["start_minute"];
            schedules[i].duration_minutes = obj["duration_minutes"];
            schedules[i].days_of_week = obj["days_of_week"];
            i++;
        }
        saveSchedules();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    server.addHandler(scheduleHandler);

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonDocument doc(512);
        doc["wifi_ssid"] = config.wifi_ssid;
        doc["owm_api_key"] = config.owm_api_key;
        doc["owm_city"] = config.owm_city;
        doc["rain_threshold"] = config.rain_threshold;
        String output;
        serializeJson(doc, output);
        request->send(200, "application/json", output);
    });

    AsyncCallbackJsonWebHandler* settingsHandler = new AsyncCallbackJsonWebHandler("/settings", [](AsyncWebServerRequest *request, JsonVariant &json) {
        JsonObject obj = json.as<JsonObject>();
        bool wifiChanged = false;
        if (config.wifi_ssid != obj["wifi_ssid"].as<String>()) wifiChanged = true;

        config.wifi_ssid = obj["wifi_ssid"].as<String>();
        config.wifi_password = obj["wifi_password"].as<String>();
        config.owm_api_key = obj["owm_api_key"].as<String>();
        config.owm_city = obj["owm_city"].as<String>();
        config.rain_threshold = obj["rain_threshold"].as<float>();

        saveConfiguration(CONFIG_FILE, config);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        
        if(wifiChanged) {
          delay(1000);
          ESP.restart();
        }
    });
    server.addHandler(settingsHandler);

    server.begin();
}
