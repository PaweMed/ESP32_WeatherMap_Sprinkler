#pragma once
#include <Arduino.h>

struct Schedule {
  bool enabled;
  int zone;
  int start_hour;
  int start_minute;
  int duration_minutes;
  byte days_of_week; // Bitmaska: 0b01111110 (Pon-Niedz)
};

Schedule schedules[NUM_ZONES];
bool isWateringActive[NUM_ZONES] = {false};
unsigned long wateringStartTime[NUM_ZONES] = {0};

// Deklaracje funkcji
void setZoneState(int zone, bool state); // Zadeklarowana w pliku głównym
bool isRainExpected(); // Zadeklarowana w weather.h

void saveSchedules() {
  File file = LittleFS.open(SCHEDULE_FILE, "w");
  if (!file) {
    Serial.println("Failed to create schedule file");
    return;
  }
  
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

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write schedules to file");
  }
  file.close();
}


void loadSchedules() {
  File file = LittleFS.open(SCHEDULE_FILE, "r");
  if (!file) {
    Serial.println("Schedule file not found. Creating defaults.");
    for (int i = 0; i < NUM_ZONES; i++) {
      schedules[i] = {false, i, 0, 0, 10, 0b01111110}; // Domyślnie wyłączone
    }
    saveSchedules();
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.println("Failed to parse schedule file");
    return;
  }

  JsonArray array = doc.as<JsonArray>();
  int i = 0;
  for (JsonObject obj : array) {
    if (i >= NUM_ZONES) break;
    schedules[i].enabled = obj["enabled"];
    schedules[i].zone = obj["zone"];
    schedules[i].start_hour = obj["start_hour"];
    schedules[i].start_minute = obj["start_minute"];
    schedules[i].duration_minutes = obj["duration_minutes"];
    schedules[i].days_of_week = obj["days_of_week"];
    i++;
  }
  file.close();
}


void checkSchedules() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  int current_day_of_week = timeinfo.tm_wday; // 0=Niedz, 1=Pon, ... 6=Sobota
  if (current_day_of_week == 0) current_day_of_week = 7; // Dopasowanie do naszej bitmaski (Niedz = bit 7)

  // Sprawdź, czy trzeba zakończyć podlewanie
  for (int i = 0; i < NUM_ZONES; i++) {
    if (isWateringActive[i]) {
      if (millis() - wateringStartTime[i] >= schedules[i].duration_minutes * 60000UL) {
        setZoneState(i, false);
        isWateringActive[i] = false;
        Serial.printf("Watering finished for zone %d\n", i + 1);
      }
    }
  }

  // Sprawdź, czy trzeba rozpocząć podlewanie
  for (int i = 0; i < NUM_ZONES; i++) {
    if (schedules[i].enabled && !isWateringActive[i]) {
      bool dayMatch = bitRead(schedules[i].days_of_week, current_day_of_week);
      if (dayMatch && schedules[i].start_hour == timeinfo.tm_hour && schedules[i].start_minute == timeinfo.tm_min) {
        if (isRainExpected()) {
          Serial.printf("Skipping watering for zone %d due to rain forecast.\n", i + 1);
        } else {
          Serial.printf("Starting watering for zone %d\n", i + 1);
          setZoneState(i, true);
          isWateringActive[i] = true;
          wateringStartTime[i] = millis();
        }
      }
    }
  }
}
