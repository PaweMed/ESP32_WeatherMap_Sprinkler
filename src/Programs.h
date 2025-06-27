#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "Zones.h"
#include "Weather.h"
#include "Logs.h"
#include "PushoverClient.h"

struct Program {
  uint8_t zone;
  String time;        // "HH:MM"
  uint16_t duration;  // minuty
  String days;        // np. "0,1,2"
  bool active;        // czy aktywny
  time_t lastRun = 0;
};

class Programs {
  Program progs[32];
  int numProgs = 0;
  Zones* zones = nullptr;
  Weather* weather = nullptr;
  Logs* logs = nullptr;
  PushoverClient* pushover = nullptr;

public:
  void begin(Zones* z, Weather* w, Logs* l, PushoverClient* p) {
    zones = z; weather = w; logs = l; pushover = p;
    loadFromFS();
  }

  void toJson(JsonDocument& doc) {
    JsonArray arr = doc.to<JsonArray>();
    for(int i=0;i<numProgs;i++) {
      JsonObject p = arr.createNestedObject();
      p["zone"] = progs[i].zone;
      p["time"] = progs[i].time;
      p["duration"] = progs[i].duration;
      p["days"] = progs[i].days;
      p["active"] = progs[i].active;
    }
  }

  void addFromJson(JsonDocument& doc) {
    if(numProgs<32) {
      progs[numProgs].zone = doc["zone"].as<uint8_t>();
      progs[numProgs].time = doc["time"].as<const char*>();
      progs[numProgs].duration = doc["duration"].as<uint16_t>();
      String d = "";
      for(auto v : doc["days"].as<JsonArray>()) {
        if(d.length()>0) d+=","; d+=String((int)v);
      }
      progs[numProgs].days = d;
      progs[numProgs].active = doc["active"].isNull() ? true : doc["active"].as<bool>();
      progs[numProgs].lastRun = 0;
      numProgs++;
      saveToFS();
      if (logs) logs->add("Dodano program strefy " + String(progs[numProgs-1].zone+1));
      if (pushover) pushover->send("Dodano program strefy " + String(progs[numProgs-1].zone+1));
    }
  }

  void edit(int idx, JsonDocument& doc) {
    if(idx >= 0 && idx < numProgs) {
      if (doc.containsKey("zone")) progs[idx].zone = doc["zone"].as<uint8_t>();
      if (doc.containsKey("time")) progs[idx].time = doc["time"].as<const char*>();
      if (doc.containsKey("duration")) progs[idx].duration = doc["duration"].as<uint16_t>();
      if (doc.containsKey("days")) {
        String d = "";
        for(auto v : doc["days"].as<JsonArray>()) {
          if(d.length()>0) d+=","; d+=String((int)v);
        }
        progs[idx].days = d;
      }
      if (doc.containsKey("active")) progs[idx].active = doc["active"].as<bool>();
      saveToFS();
      if (logs) logs->add("Edytowano program strefy " + String(progs[idx].zone+1));
      if (pushover) pushover->send("Edytowano program strefy " + String(progs[idx].zone+1));
    }
  }

  void remove(int idx) {
    if(idx>=0 && idx<numProgs) {
      for(int i=idx;i<numProgs-1;i++) progs[i]=progs[i+1];
      numProgs--;
      saveToFS();
      if (logs) logs->add("Usunięto program " + String(idx));
      if (pushover) pushover->send("Usunięto program " + String(idx));
    }
  }

  void clear() {
    numProgs = 0;
    saveToFS();
    if (logs) logs->add("Wyczyszczono wszystkie programy");
    if (pushover) pushover->send("Wyczyszczono wszystkie programy");
  }

  // Zapis/odczyt do/z pliku
  void saveToFS() {
    DynamicJsonDocument doc(4096);
    JsonArray arr = doc.to<JsonArray>();
    for (int i=0; i<numProgs; i++) {
      JsonObject p = arr.createNestedObject();
      p["zone"] = progs[i].zone;
      p["time"] = progs[i].time;
      p["duration"] = progs[i].duration;
      p["days"] = progs[i].days;
      p["active"] = progs[i].active;
    }
    File f = LittleFS.open("/programs.json", "w");
    if (f) { serializeJson(doc, f); f.close(); }
  }

  void loadFromFS() {
    numProgs = 0;
    if (!LittleFS.exists("/programs.json")) return;
    File f = LittleFS.open("/programs.json", "r");
    if (!f) return;
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return;
    JsonArray arr = doc.as<JsonArray>();
    for (auto el : arr) {
      if(numProgs>=32) break;
      progs[numProgs].zone = el["zone"] | 0;
      progs[numProgs].time = el["time"] | "06:00";
      progs[numProgs].duration = el["duration"] | 10;
      progs[numProgs].days = el["days"] | "0,1,2,3,4,5,6";
      progs[numProgs].active = el["active"].isNull() ? true : el["active"].as<bool>();
      progs[numProgs].lastRun = 0;
      numProgs++;
    }
  }

  // Import wszystkich programów przez API (np. przywrócenie backupu)
  void importFromJson(JsonDocument& doc) {
    JsonArray arr = doc.as<JsonArray>();
    numProgs = 0;
    for (auto el : arr) {
      if(numProgs>=32) break;
      progs[numProgs].zone = el["zone"] | 0;
      progs[numProgs].time = el["time"] | "06:00";
      progs[numProgs].duration = el["duration"] | 10;
      progs[numProgs].days = el["days"] | "0,1,2,3,4,5,6";
      progs[numProgs].active = el["active"].isNull() ? true : el["active"].as<bool>();
      progs[numProgs].lastRun = 0;
      numProgs++;
    }
    saveToFS();
    if (logs) logs->add("Zaimportowano programy");
    if (pushover) pushover->send("Zaimportowano programy");
  }

  void loop() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < 10000) return; // co 10s
    lastCheck = millis();

    time_t now = ::time(nullptr);
    struct tm* t = localtime(&now);
    int today = t->tm_wday == 0 ? 6 : t->tm_wday-1; // 0=pon, 6=ndz
    int nowMins = t->tm_hour*60 + t->tm_min;

    for (int i = 0; i < numProgs; i++) {
      if (!progs[i].active) continue;  // Program nieaktywny
      // sprawdź czy program dotyczy dzisiejszego dnia
      bool runToday = false;
      for (int d = 0; d < 7; d++) {
        if (progs[i].days.indexOf(String(today)) >= 0) runToday = true;
      }
      if (!runToday) continue;

      // sprawdź czy jest odpowiednia godzina
      int pHour = atoi(progs[i].time.substring(0,2).c_str());
      int pMin  = atoi(progs[i].time.substring(3,5).c_str());
      int progMins = pHour*60 + pMin;
      if (nowMins == progMins && (progs[i].lastRun == 0 || localtime(&progs[i].lastRun)->tm_yday != t->tm_yday)) {
        int actualDuration = progs[i].duration;
        if (weather) {
          int percent = weather->getWateringPercent();
          if (percent == 0) {
            if (logs) logs->add("Podlewanie odwołane - prognoza opadów!");
            if (pushover) pushover->send("Podlewanie odwołane - prognoza opadów!");
            continue; // nie podlewaj
          } else if (percent == 50) {
            actualDuration = (actualDuration * 50) / 100;
            if (logs) logs->add("Podlewanie skrócone do 50% przez prognozę.");
            if (pushover) pushover->send("Podlewanie skrócone do 50% przez prognozę.");
          }
        }
        zones->startZone(progs[i].zone, actualDuration*60);
        progs[i].lastRun = now;
        if (logs) logs->add("Automat: Start strefy " + String(progs[i].zone+1));
        if (pushover) pushover->send("Start strefy " + String(progs[i].zone+1));
      }
    }
  }
};
