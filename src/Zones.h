#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class Zones {
  int numZones;
  bool states[8];
  int pins[8] = {13,12,14,27,26,25,33,32};
  unsigned long endTime[8] = {0}; // kiedy wyłączyć
public:
  Zones(int n) : numZones(n) {}

  void begin() {
    for(int i=0; i<numZones; i++) {
      pinMode(pins[i], OUTPUT);
      digitalWrite(pins[i], LOW);
      states[i] = false;
      endTime[i] = 0;
    }
  }

  void toJson(JsonDocument& doc) {
    for(int i=0;i<numZones;i++) {
      JsonObject z = doc.add<JsonObject>();  // NOWA WERSJA
      z["id"] = i;
      z["active"] = states[i];
      z["remaining"] = states[i] ? max(0, (int)((endTime[i] - millis())/1000)) : 0;
    }
  }

  void startZone(int idx, int durationSec) {
    if(idx<0||idx>=numZones) return;
    states[idx] = true;
    digitalWrite(pins[idx], HIGH);
    endTime[idx] = millis() + durationSec*1000UL;
  }

  void stopZone(int idx) {
    if(idx<0||idx>=numZones) return;
    states[idx] = false;
    digitalWrite(pins[idx], LOW);
    endTime[idx] = 0;
  }

  void toggleZone(int idx) {
    if(idx<0||idx>=numZones) return;
    if (states[idx]) stopZone(idx);
    else startZone(idx, 600); // domyślnie 10 min w manualu
  }

  void loop() {
    unsigned long now = millis();
    for(int i=0; i<numZones; i++) {
      if(states[i] && now > endTime[i]) stopZone(i);
    }
  }

  bool getZoneState(int idx) { 
    if(idx<0||idx>=numZones) return false; 
    return states[idx]; 
  }
};
