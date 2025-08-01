#define MAIN_CPP
#include <Arduino.h>
#include "FS.h"
#include "LittleFS.h"
#include <time.h>

#include "Config.h"
#include "Zones.h"
#include "Programs.h"
#include "Weather.h"
#include "Logs.h"
#include "PushoverClient.h"
#include "WebServerUI.h"
#include "MQTTClient.h"

// --- Obiekty globalne ---
Config config;
Zones zones(8);
Weather weather;
Logs logs;
PushoverClient pushover(config.getSettingsPtr());
Programs programs;

// --- Funkcja do ustawiania strefy czasowej ---
void setTimezone() {
  String tz = config.getTimezone();
  Serial.print("Strefa czasowa ustawiana na: ");
  Serial.println(tz);

  // Zamiana popularnych nazw IANA na string offsetowy
  if (tz == "" || tz == "Europe/Warsaw") {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  } else if (tz == "America/New_York") {
    setenv("TZ", "EST+5EDT,M3.2.0/2,M11.1.0/2", 1);
  } else if (tz == "UTC" || tz == "Etc/UTC") {
    setenv("TZ", "UTC0", 1);
  } else if (tz == "Europe/London") {
    setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0", 1);
  } else if (tz == "Asia/Tokyo") {
    setenv("TZ", "JST-9", 1);
  } else {
    setenv("TZ", tz.c_str(), 1); // pozwól na custom stringi
  }
  tzset();

  Serial.print("Aktualny TZ z getenv: ");
  Serial.println(getenv("TZ"));

  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char buf[64];
  snprintf(buf, sizeof(buf), "Czas lokalny po zmianie strefy: %04d-%02d-%02d %02d:%02d:%02d",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  Serial.println(buf);
}

// --- Funkcja synchronizująca czas NTP (tylko na starcie) ---
void syncNtp() {
  Serial.println("Synchronizacja czasu NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  unsigned long t0 = millis();
  while (now < 8 * 3600 * 2 && millis() - t0 < 20000) { // czekaj max 20 sekund na NTP
    delay(500);
    now = time(nullptr);
    Serial.print("[NTP] Synchronizacja czasu... ");
    Serial.println(now);
  }
  struct tm t;
  localtime_r(&now, &t);
  char buf[64];
  snprintf(buf, sizeof(buf), "Czas lokalny po synchronizacji: %04d-%02d-%02d %02d:%02d:%02d",
           t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  Serial.println(buf);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  LittleFS.begin();

  // --- Wypisz listę plików w LittleFS ---
  Serial.println("Pliki w LittleFS:");
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  while(file){
      Serial.print("  ");
      Serial.print(file.name());
      Serial.print(" (");
      Serial.print(file.size());
      Serial.println(" bajtów)");
      file = root.openNextFile();
  }

  config.load();
  config.initWiFi(&pushover);

  // Poczekaj chwilę, aż WiFi się zestawi w pełni, zanim rozpoczniesz NTP
  delay(2000);

  zones.begin();
  weather.begin(config.getOwmApiKey(), config.getOwmLocation());
  pushover.begin();
  programs.begin(&zones, &weather, &logs, &pushover);

  syncNtp();      // NTP tylko raz na starcie!
  setTimezone();  // Ustaw strefę

  WebServerUI::begin(
    &config,
    nullptr,
    &zones,
    &weather,
    &pushover,
    &programs,
    &logs
  );

  mqtt.begin(
    &zones,
    &programs,
    &weather,
    &logs,
    &config
  );

  Serial.println("[MAIN] System uruchomiony.");
}

// Funkcja udostępniana do WebServerUI.h (dla POST /api/settings)
extern "C" void setTimezoneFromWeb() { setTimezone(); }

void loop() {
  config.wifiLoop();
  zones.loop();
  programs.loop();
  weather.loop();
  mqtt.loop();
}
