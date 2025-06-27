#define MAIN_CPP
#include <Arduino.h>
#include "FS.h"
#include "LittleFS.h"
#include <time.h>

#include "Config.h"           // Zarządzanie ustawieniami i WiFi
#include "Zones.h"
#include "Programs.h"
#include "Weather.h"
#include "Logs.h"
#include "PushoverClient.h"
#include "WebServerUI.h"      // Nowoczesny WebUI + API

#include "MQTTClient.h"       // MQTT (pełna obsługa, dynamiczna konfiguracja przez www)

// --- Obiekty globalne ---
Config config;                // Zawiera Settings i obsługę WiFi
Zones zones(8);               // 8 stref przekaźników
Weather weather;              // Pogoda z OWM
Logs logs;                    // Logi operacji
PushoverClient pushover;      // Powiadomienia Pushover (konstruktor bez argumentów)
Programs programs;            // Harmonogramy
MQTTClient mqtt;              // Nowy klient MQTT (globalny, patrz extern w MQTTClient.h)

// --- Inicjalizacja systemu ---
void setup() {
  Serial.begin(115200);
  delay(100);
  LittleFS.begin();

  // --- Wczytaj konfigurację (WiFi, API itp.) ---
  config.load();

  // --- Start WiFi, w zależności od ustawień przechodzi w tryb AP lub łączy się do sieci ---
  config.initWiFi(&pushover);

  // --- Inicjalizacja stref, pogody, logów, pushovera, harmonogramów ---
  zones.begin();
  weather.begin(config.getOwmApiKey(), config.getOwmLocation());
  pushover.begin(
    config.getPushoverUser(),
    config.getPushoverToken()
  );
  programs.begin(&zones, &weather, &logs, &pushover);

  // --- Synchronizacja czasu z NTP ---
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  // --- Rejestracja serwera www oraz endpointów API ---
  WebServerUI::begin(
    &config,
    nullptr,      // Jeśli masz klasę Scheduler, podstaw ją tutaj (możesz dodać Scheduler w przyszłości)
    &zones,       // Zones jako RelayBoard
    &weather,
    &pushover,
    &programs,
    &logs
  );

  // --- Start MQTT (ustawienia pobierane z config) ---
  mqtt.begin(
    &zones,
    &programs,
    &weather,
    &logs,
    &config
  );

  Serial.println("[MAIN] System uruchomiony.");
}

// --- Pętla główna ---
void loop() {
  // Sprawdzanie połączenia WiFi i ew. zmiana trybu (co kilka sekund)
  config.wifiLoop();

  // Logika stref, harmonogramów, pogody, powiadomień
  zones.loop();
  programs.loop();
  weather.loop();
  // pushover.loop(); // Dodaj jeśli implementujesz tryb cykliczny dla powiadomień

  // Obsługa MQTT
  mqtt.loop();

  // Możesz dorzucić obsługę watchdog, MQTT itd.
}
