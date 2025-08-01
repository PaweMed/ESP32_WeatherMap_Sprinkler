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
      JsonObject p = arr.add<JsonObject>();
      p["zone"] = progs[i].zone;
      p["time"] = progs[i].time;
      p["duration"] = progs[i].duration;

      // days jako TABLICA LICZB (zgodnie z frontendem)
      JsonArray daysArr = p["days"].to<JsonArray>();
      String daysStr = progs[i].days;
      int lastPos = 0, pos;
      while ((pos = daysStr.indexOf(',', lastPos)) != -1) {
        daysArr.add(daysStr.substring(lastPos, pos).toInt());
        lastPos = pos + 1;
      }
      if (lastPos < daysStr.length()) daysArr.add(daysStr.substring(lastPos).toInt());

      p["active"] = progs[i].active;
    }
  }

  void addFromJson(JsonDocument& doc) {
    Serial.println("==== addFromJson start ====");
    Serial.print("numProgs: "); Serial.println(numProgs);
    serializeJson(doc, Serial); Serial.println();

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
      Serial.print("Dodaję program: zone="); Serial.print(progs[numProgs].zone);
      Serial.print(", time="); Serial.print(progs[numProgs].time);
      Serial.print(", duration="); Serial.print(progs[numProgs].duration);
      Serial.print(", days="); Serial.print(progs[numProgs].days);
      Serial.print(", active="); Serial.println(progs[numProgs].active);
      numProgs++;
      saveToFS();
      //if (logs) logs->add("Dodano program strefy " + String(progs[numProgs-1].zone+1));
      //if (pushover) pushover->send("Dodano program strefy " + String(progs[numProgs-1].zone+1));
    } else {
      Serial.println("Osiągnięto maksymalną liczbę programów (32).");
      if (logs) logs->add("Nie dodano programu – maksymalna liczba programów");
      if (pushover) pushover->send("Nie dodano programu – maksymalna liczba programów");
    }

    Serial.println("==== addFromJson end ====");
  }

  void edit(int idx, JsonDocument& doc) {
    Serial.println("==== edit start ====");
    Serial.print("idx: "); Serial.println(idx);
    if(idx >= 0 && idx < numProgs) {
      if (doc["zone"].is<uint8_t>()) progs[idx].zone = doc["zone"].as<uint8_t>();
      if (doc["time"].is<const char*>()) progs[idx].time = doc["time"].as<const char*>();
      if (doc["duration"].is<uint16_t>()) progs[idx].duration = doc["duration"].as<uint16_t>();
      if (doc["days"].is<JsonArray>()) {
        String d = "";
        for(auto v : doc["days"].as<JsonArray>()) {
          if(d.length()>0) d+=","; d+=String((int)v);
        }
        progs[idx].days = d;
      }
      if (doc["active"].is<bool>()) progs[idx].active = doc["active"].as<bool>();
      saveToFS();
      if (logs) logs->add("Edytowano program strefy " + String(progs[idx].zone+1));
      if (pushover) pushover->send("Edytowano program strefy " + String(progs[idx].zone+1));
    }
    Serial.println("==== edit end ====");
  }

  void remove(int idx) {
    Serial.println("==== remove start ====");
    Serial.print("idx: "); Serial.println(idx);
    if(idx>=0 && idx<numProgs) {
      for(int i=idx;i<numProgs-1;i++) progs[i]=progs[i+1];
      numProgs--;
      saveToFS();
      //if (logs) logs->add("Usunięto program " + String(idx));
      //if (pushover) pushover->send("Usunięto program " + String(idx));
    }
    Serial.println("==== remove end ====");
  }

  void clear() {
    numProgs = 0;
    saveToFS();
    if (logs) logs->add("Wyczyszczono wszystkie programy");
    if (pushover) pushover->send("Wyczyszczono wszystkie programy");
  }

  // Zapis/odczyt do/z pliku
  void saveToFS() {
    Serial.println("==== saveToFS start ====");
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i=0; i<numProgs; i++) {
      JsonObject p = arr.add<JsonObject>();
      p["zone"] = progs[i].zone;
      p["time"] = progs[i].time;
      p["duration"] = progs[i].duration;
      p["days"] = progs[i].days;
      p["active"] = progs[i].active;
    }
    File f = LittleFS.open("/programs.json", "w");
    if (f) { serializeJson(doc, f); f.close(); }
    else { Serial.println("saveToFS: NIE MOŻNA OTWORZYĆ pliku do zapisu!"); }
    Serial.println("==== saveToFS end ====");
  }

  void loadFromFS() {
    Serial.println("==== loadFromFS start ====");
    numProgs = 0;
    if (!LittleFS.exists("/programs.json")) {
      Serial.println("Brak pliku /programs.json");
      return;
    }
    File f = LittleFS.open("/programs.json", "r");
    if (!f) {
      Serial.println("Nie można otworzyć pliku /programs.json");
      return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
      Serial.print("Błąd odczytu JSON: "); Serial.println(err.c_str());
      return;
    }
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
    Serial.print("Załadowano programów: "); Serial.println(numProgs);
    Serial.println("==== loadFromFS end ====");
  }

  // Import wszystkich programów przez API (np. przywrócenie backupu)
  void importFromJson(JsonDocument& doc) {
    Serial.println("==== importFromJson start ====");
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
    Serial.println("==== importFromJson end ====");
  }

  void loop() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < 10000) return; // co 10s
    lastCheck = millis();

    time_t now = ::time(nullptr);
    struct tm* t = localtime(&now);

    // UJEDNOLICONA LOGIKA: 0 = niedziela, 1 = poniedziałek, ..., 6 = sobota
    // (zgodnie z tm_wday i tym, co wysyła frontend)
    int today = t->tm_wday; // 0=niedziela, 1=poniedziałek, 2=wtorek, ..., 6=sobota

    int nowMins = t->tm_hour*60 + t->tm_min;

    for (int i = 0; i < numProgs; i++) {
      if (!progs[i].active) continue;  // Program nieaktywny

      // Sprawdź czy program dotyczy dzisiejszego dnia
      bool runToday = false;
      String daysStr = progs[i].days;
      int lastPos = 0, pos;
      while ((pos = daysStr.indexOf(',', lastPos)) != -1) {
        int dayNum = daysStr.substring(lastPos, pos).toInt();
        if (dayNum == today) runToday = true;
        lastPos = pos + 1;
      }
      if (lastPos < daysStr.length()) {
        int dayNum = daysStr.substring(lastPos).toInt();
        if (dayNum == today) runToday = true;
      }
      if (!runToday) continue;

      // sprawdź czy jest odpowiednia godzina
      int pHour = atoi(progs[i].time.substring(0,2).c_str());
      int pMin  = atoi(progs[i].time.substring(3,5).c_str());
      int progMins = pHour*60 + pMin;

      // POPRAWKA: >= zamiast ==, żeby nie przegapić uruchomienia przez zbyt rzadkie wywołanie loop
      // oraz progs[i].lastRun sprawdzamy po dniu
      if (nowMins >= progMins && (progs[i].lastRun == 0 || localtime(&progs[i].lastRun)->tm_yday != t->tm_yday)) {
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
