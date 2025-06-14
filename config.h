#pragma once
#include <Arduino.h>

#define NUM_ZONES 8
#define CONFIG_FILE "/config.json"
#define SCHEDULE_FILE "/schedules.json"

// Ustawienia czasu
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 3600; // Strefa czasowa dla Polski (UTC+1)
const int   DAYLIGHT_OFFSET_SEC = 3600; // Czas letni

// Struktura przechowująca konfigurację
struct Config {
  String wifi_ssid;
  String wifi_password;
  String owm_api_key;
  String owm_city;
  float rain_threshold; // Próg opadów w mm, powyżej którego podlewanie jest blokowane
};

Config config; // Globalna instancja konfiguracji

// Deklaracje funkcji (ciała w pliku głównym lub dedykowanym)
void loadConfiguration(const char *filename, Config &config);
void saveConfiguration(const char *filename, const Config &config);

// Implementacje funkcji do zapisu/odczytu konfiguracji
void loadConfiguration(const char *filename, Config &config) {
  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("Failed to open config file for reading. Using default values.");
    // Ustawienia domyślne, jeśli plik nie istnieje
    config.wifi_ssid = "iPhone Pawel";
    config.wifi_password = "12345678";
    config.owm_api_key = "82320fe08ec3b20cd622f5d846b1c4c2";
    config.owm_city = "Szczecin,PL";
    config.rain_threshold = 1.0; // 1 mm deszczu
    return;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.println("Failed to parse config file");
  } else {
    config.wifi_ssid = doc["wifi_ssid"].as<String>();
    config.wifi_password = doc["wifi_password"].as<String>();
    config.owm_api_key = doc["owm_api_key"].as<String>();
    config.owm_city = doc["owm_city"].as<String>();
    config.rain_threshold = doc["rain_threshold"] | 1.0;
  }
  file.close();
}

void saveConfiguration(const char *filename, const Config &config) {
  File file = LittleFS.open(filename, "w");
  if (!file) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  StaticJsonDocument<512> doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["owm_api_key"] = config.owm_api_key;
  doc["owm_city"] = config.owm_city;
  doc["rain_threshold"] = config.rain_threshold;

  if (serializeJson(doc, file) == 0) {
    Serial.println("Failed to write to config file");
  }
  file.close();
}
