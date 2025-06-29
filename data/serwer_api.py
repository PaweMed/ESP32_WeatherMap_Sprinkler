from flask import Flask, jsonify, request, send_from_directory, make_response
from datetime import datetime, timedelta
import os
import threading
import time

app = Flask(__name__)

ZONE_COUNT = 8
zones = [{"id": i, "active": False, "time_left": 0, "off_time": None} for i in range(ZONE_COUNT)]
weather = {
    "humidity": 70,
    "rain": 0.3,
    "temp": 24.5,
    "watering_percent": 30,  # 100, 60 lub 20 (albo 0)
    "wind": 2.1
}
programs = []
logs = ["System uruchomiony."]
settings = {
    "ssid": "TwojaSiecWiFi",
    "pass": "haslo123",
    "owmApiKey": "",
    "owmLocation": "Warsaw,PL",
    "pushoverUser": "",
    "pushoverToken": "",
    "timezone": "Europe/Warsaw",
    "mqttServer": "",
    "mqttPort": 1883,
    "mqttUser": "",
    "mqttPass": "",
    "mqttClientId": ""
}

# ------------------- STATIC FILES -------------------

@app.route('/')
def root():
    return send_from_directory('.', 'index.html')

@app.route('/<path:path>')
def static_proxy(path):
    if os.path.exists(path):
        resp = make_response(send_from_directory('.', path))
        resp.headers['Cache-Control'] = 'no-store, no-cache, must-revalidate, max-age=0'
        resp.headers['Pragma'] = 'no-cache'
        resp.headers['Expires'] = '0'
        return resp
    else:
        return "Not found", 404

# ------------------- API -------------------

@app.route('/api/status')
def api_status():
    return jsonify({
        "wifi": "Połączono",
        "ip": "127.0.0.1",
        "time": datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    })

@app.route('/api/weather', methods=['GET', 'POST'])
def api_weather():
    if request.method == 'POST':
        data = request.get_json()
        for k in weather.keys():
            if k in data:
                weather[k] = data[k]
        logs.append(f"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}: Zmieniono pogodę: {weather}")
        return jsonify(success=True)
    return jsonify(weather)

def update_zone_states():
    now = datetime.now()
    for z in zones:
        if z["active"] and z["off_time"]:
            remaining = (z["off_time"] - now).total_seconds()
            if remaining > 0:
                z["time_left"] = int(remaining)
            else:
                z["active"] = False
                z["off_time"] = None
                z["time_left"] = 0
                logs.append(f"{now.strftime('%Y-%m-%d %H:%M:%S')}: Strefa {z['id']+1} wyłączona (koniec czasu)")

@app.route('/api/zones', methods=['GET', 'POST'])
def api_zones():
    update_zone_states()
    if request.method == 'GET':
        return jsonify([
            {"id": z["id"], "active": z["active"], "time_left": z["time_left"]} for z in zones
        ])
    if request.method == 'POST':
        data = request.get_json()
        idx = int(data.get('id', 0))
        toggle = data.get('toggle', False)
        duration = int(data.get('duration', 120))  # domyślnie 2 minuty
        now = datetime.now()
        if 0 <= idx < ZONE_COUNT:
            z = zones[idx]
            if toggle:
                if z["active"]:
                    z["active"] = False
                    z["off_time"] = None
                    z["time_left"] = 0
                    logs.append(f"{now.strftime('%Y-%m-%d %H:%M:%S')}: Strefa {idx+1} wyłączona (manualnie)")
                else:
                    z["active"] = True
                    z["off_time"] = now + timedelta(seconds=duration)
                    z["time_left"] = duration
                    logs.append(f"{now.strftime('%Y-%m-%d %H:%M:%S')}: Strefa {idx+1} włączona na {duration}s")
        return jsonify(success=True)

@app.route('/api/programs', methods=['GET', 'POST'])
def api_programs():
    if request.method == 'GET':
        return jsonify(programs)
    if request.method == 'POST':
        data = request.get_json()
        prog = {
            "zone": int(data.get('zone', 0)),
            "time": data.get('time', '06:00'),
            "duration": int(data.get('duration', 10)),
            "days": data.get('days', [])
        }
        # Zgodność z frontendem: days może być listą lub stringiem
        if isinstance(prog["days"], list):
            prog["days"] = ','.join(str(x) for x in prog["days"])
        programs.append(prog)
        logs.append(f"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}: Dodano harmonogram: strefa {prog['zone']+1} {prog['time']} dni {prog['days']}")
        return jsonify(success=True)

@app.route('/api/programs/<int:idx>', methods=['DELETE'])
def api_program_delete(idx):
    if 0 <= idx < len(programs):
        p = programs.pop(idx)
        logs.append(f"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}: Usunięto harmonogram: strefa {p['zone']+1} {p['time']}")
    return jsonify(success=True)

@app.route('/api/logs', methods=['GET', 'DELETE'])
def api_logs():
    if request.method == 'GET':
        return jsonify({"logs": logs[-100:]})
    if request.method == 'DELETE':
        logs.clear()
        logs.append(f"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}: Log wyczyszczony.")
        return jsonify(success=True)

@app.route('/api/settings', methods=['GET', 'POST'])
def api_settings():
    if request.method == 'GET':
        return jsonify(settings)
    if request.method == 'POST':
        data = request.get_json()
        for k in settings.keys():
            if k in data:
                settings[k] = data[k]
        logs.append(f"{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}: Zmieniono ustawienia.")
        return jsonify(success=True)

# --- DEBUG ONLY: RESET WSZYSTKICH STREF ---
@app.route('/api/debug/reset_zones', methods=['POST'])
def debug_reset_zones():
    for z in zones:
        z["active"] = False
        z["off_time"] = None
        z["time_left"] = 0
    return jsonify(success=True)

# ----------- AUTOMATYCZNE WŁĄCZANIE STREF Z HARMONOGRAMÓW -----------
def scheduler_loop():
    while True:
        now = datetime.now()
        now_day = now.weekday()  # Pn=0, Nd=6
        my_day = (now_day+1)%7   # 0=Nd

        now_str = now.strftime('%H:%M')
        for prog in programs:
            try:
                prog_days = [int(x) for x in (prog["days"].split(",") if isinstance(prog["days"], str) else prog["days"])]
            except:
                prog_days = []
            if my_day in prog_days and prog["time"] == now_str:
                zone_id = int(prog["zone"])
                z = zones[zone_id]
                percent = weather.get("watering_percent", 100)
                if percent <= 0 or percent == 20:
                    logs.append(f"{now.strftime('%Y-%m-%d %H:%M:%S')}: HARMONOGRAM: Strefa {zone_id+1} odwołana przez pogodę (watering_percent={percent})")
                    continue  # nie włączaj
                time_minutes = int(prog["duration"]) * percent // 100
                if time_minutes < 1: time_minutes = 1
                if not z["active"]:
                    z["active"] = True
                    z["off_time"] = now + timedelta(minutes=time_minutes)
                    z["time_left"] = time_minutes*60
                    logs.append(f"{now.strftime('%Y-%m-%d %H:%M:%S')}: HARMONOGRAM: Strefa {zone_id+1} ON na {time_minutes} min (watering_percent={percent})")
        time.sleep(1)

# ------------------- MAIN -------------------

if __name__ == '__main__':
    t = threading.Thread(target=scheduler_loop, daemon=True)
    t.start()
    app.run(host="0.0.0.0", port=8080, debug=True)
