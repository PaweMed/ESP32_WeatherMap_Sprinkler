#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "LittleFS.h"

// Dołączanie modułów programu
#include "config.h"
#include "web_server.h"
#include "scheduler.h"
#include "weather.h"

// Piny dla rejestru przesuwnego 74HC595
const int latchPin = 13; // ST_CP
const int clockPin = 12; // SH_CP
const int dataPin = 14;  // DS

// Zmienna przechowująca stan wszystkich 8 stref (bitowo)
byte zonesState = 0;

void setup() {
  Serial.begin(115200);
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  // Inicjalizacja systemu plików
  if(!LittleFS.begin(true)){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  // Załaduj konfigurację z pliku
  loadConfiguration(CONFIG_FILE, config);

  // Inicjalizacja i połączenie z WiFi
  WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());
  Serial.print("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Synchronizacja czasu
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  // Załaduj harmonogramy
  loadSchedules();

  // Uruchom serwer WWW
  initWebServer();
  
  Serial.println("System ready.");
}

void loop() {
  // Sprawdź harmonogram i uruchom/zatrzymaj podlewanie
  checkSchedules();

  // Co godzinę pobierz nową prognozę pogody
  // (funkcja sama sprawdza, czy minął odpowiedni czas)
  updateWeatherIfNeeded();
}

// Funkcja do aktualizacji stanu przekaźników przez rejestr przesuwny
void updateShiftRegister() {
   digitalWrite(latchPin, LOW);
   shiftOut(dataPin, clockPin, MSBFIRST, zonesState);
   digitalWrite(latchPin, HIGH);
}

// Funkcja do sterowania pojedynczą strefą
void setZoneState(int zone, bool state) {
  if (zone < 0 || zone >= NUM_ZONES) return;

  if (state) {
    bitSet(zonesState, zone); // Włącz strefę
  } else {
    bitClear(zonesState, zone); // Wyłącz strefę
  }
  updateShiftRegister();
  Serial.printf("Zone %d set to %s. State: %B\n", zone + 1, state ? "ON" : "OFF", zonesState);
}
