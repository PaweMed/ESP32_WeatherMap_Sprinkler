#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class Weather {
  String apiKey, location;
  float temp=0, humidity=0, rain1h=0, rain6h=0, wind=0;
  float forecast1h=0, forecast6h=0;
  unsigned long lastWeather = 0;
  unsigned long lastForecast = 0;
public:
  void begin(const String& key, const String& loc) {
    apiKey = key; location = loc;
  }
  void loop() {
    unsigned long now = millis();
    // Pogoda historyczna - co 30 min
    if (now - lastWeather > 1800000UL) {
      lastWeather = now;
      String url = "http://api.openweathermap.org/data/2.5/onecall?units=metric&exclude=minutely,daily,alerts,current&appid=" + apiKey + "&q=" + location;
      // Zmiana: onecall wymaga lat,lon - wyciągamy je najpierw
      float lat = 0, lon = 0;
      String urlGeo = "http://api.openweathermap.org/geo/1.0/direct?q=" + location + "&limit=1&appid=" + apiKey;
      HTTPClient httpGeo;
      httpGeo.begin(urlGeo);
      int codeGeo = httpGeo.GET();
      if (codeGeo > 0) {
        String respGeo = httpGeo.getString();
        DynamicJsonDocument docGeo(512);
        deserializeJson(docGeo, respGeo);
        if (docGeo.size() > 0) {
          lat = docGeo[0]["lat"] | 0;
          lon = docGeo[0]["lon"] | 0;
        }
      }
      httpGeo.end();
      if (lat == 0 && lon == 0) return;
      url = "http://api.openweathermap.org/data/2.5/onecall?lat=" + String(lat,6) + "&lon=" + String(lon,6) + "&units=metric&exclude=minutely,daily,alerts,current&appid=" + apiKey;
      HTTPClient http;
      http.begin(url);
      int code = http.GET();
      if (code > 0) {
        String resp = http.getString();
        DynamicJsonDocument doc(8192);
        deserializeJson(doc, resp);
        // temp/humidity z aktualnej godziny
        temp = doc["hourly"][0]["temp"] | 0;
        humidity = doc["hourly"][0]["humidity"] | 0;
        wind = doc["hourly"][0]["wind_speed"] | 0;
        // Opady z ostatnich 6 godzin
        rain6h = 0; rain1h = 0;
        for (int i = 0; i < 6; i++) {
          float r = doc["hourly"][i]["rain"]["1h"] | 0;
          if (i == 0) rain1h = r;
          rain6h += r;
        }
      }
      http.end();
    }

    // Prognoza - co 30 min
    if (now - lastForecast > 1800000UL) {
      lastForecast = now;
      // Znajdź współrzędne jak wyżej
      float lat = 0, lon = 0;
      String urlGeo = "http://api.openweathermap.org/geo/1.0/direct?q=" + location + "&limit=1&appid=" + apiKey;
      HTTPClient httpGeo;
      httpGeo.begin(urlGeo);
      int codeGeo = httpGeo.GET();
      if (codeGeo > 0) {
        String respGeo = httpGeo.getString();
        DynamicJsonDocument docGeo(512);
        deserializeJson(docGeo, respGeo);
        if (docGeo.size() > 0) {
          lat = docGeo[0]["lat"] | 0;
          lon = docGeo[0]["lon"] | 0;
        }
      }
      httpGeo.end();
      if (lat == 0 && lon == 0) return;
      // Prognoza 3h (forecast) - OpenWeatherMap
      String urlF = "http://api.openweathermap.org/data/2.5/forecast?lat=" + String(lat,6) + "&lon=" + String(lon,6) + "&appid=" + apiKey + "&units=metric";
      HTTPClient httpF;
      httpF.begin(urlF);
      int codeF = httpF.GET();
      if (codeF > 0) {
        String respF = httpF.getString();
        DynamicJsonDocument docF(16384);
        deserializeJson(docF, respF);
        forecast1h = 0; forecast6h = 0;
        int cnt = 0;
        // Każda prognoza to blok co 3 godziny - sumujemy pierwsze 2 (6h), wyciągamy max z pierwszego (1h)
        for (int i = 0; i < 2 && i < docF["list"].size(); i++) {
          float rainf = docF["list"][i]["rain"]["3h"] | 0;
          if (i == 0) forecast1h = rainf / 3.0; // zaokrąglamy do mm/h
          forecast6h += rainf;
        }
      }
      httpF.end();
    }
  }

  void toJson(JsonDocument& doc) {
    doc["temp"] = temp;
    doc["humidity"] = humidity;
    doc["rain1h"] = rain1h;
    doc["rain6h"] = rain6h;
    doc["wind"] = wind;
    doc["forecast1h"] = forecast1h;
    doc["forecast6h"] = forecast6h;
    doc["watering_percent"] = getWateringPercent();
    doc["watering_allowed"] = wateringAllowed();
  }

  // Algorytm do sterowania podlewaniem zgodnie z założeniami użytkownika
  int getWateringPercent() {
    if (forecast1h > 4.0) return 0; // nie podlewaj
    if (forecast6h > 2.0 && forecast6h <= 4.0) return 50; // podlewaj 50%
    return 100; // pełne podlewanie
  }
  bool wateringAllowed() {
    return getWateringPercent() > 0;
  }
};
