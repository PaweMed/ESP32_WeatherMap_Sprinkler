#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class Weather {
  String apiKey, location;
  // Dane aktualne
  float temp = 0, feels_like = 0, temp_min = 0, temp_max = 0;
  float humidity = 0, pressure = 0, wind = 0, wind_deg = 0, clouds = 0, visibility = 0;
  String weather_desc = "", icon = "";
  float rain = 0;

  // Prognozy
  float rain_1h_forecast = 0, rain_6h_forecast = 0;
  float temp_min_tomorrow = 0, temp_max_tomorrow = 0;

  // Wschód/zachód słońca
  String sunrise = "", sunset = "";

  unsigned long lastWeather = 0;
  unsigned long lastForecast = 0;

public:
  void begin(const String& key, const String& loc) {
    apiKey = key;
    location = loc;
    lastWeather = 0;   // <<---- to powoduje natychmiastowe pobranie po starcie/resetcie
    lastForecast = 0;  // <<---- to samo dla prognozy
  }

  void loop() {
    unsigned long now = millis();
    // --- DANE AKTUALNE: co 30 min ---
    if (now - lastWeather > 60000UL) {
      lastWeather = now;
      Serial.println("[Weather] Pobieranie AKTUALNEJ pogody OWM...");

      // GEO
      float lat = 0, lon = 0;
      String urlGeo = "http://api.openweathermap.org/geo/1.0/direct?q=" + location + "&limit=1&appid=" + apiKey;
      HTTPClient httpGeo;
      httpGeo.begin(urlGeo);
int codeGeo = httpGeo.GET();
if (codeGeo > 0) {
  String respGeo = httpGeo.getString();
  Serial.print("[Weather] Odpowiedź OWM GEO: "); Serial.println(respGeo);
  JsonDocument docGeo;
  if (!deserializeJson(docGeo, respGeo)) {
    if (docGeo.is<JsonArray>() && docGeo.size() > 0) {
      JsonObject obj = docGeo[0];
      lat = obj["lat"].as<float>();
      lon = obj["lon"].as<float>();
    }
  }
} else {
  Serial.print("[Weather] Błąd pobierania GEO! Kod HTTP: "); Serial.println(codeGeo);
}
httpGeo.end();
if (lat == 0 && lon == 0) { Serial.println("[Weather] Błąd: Brak współrzędnych!"); return; }


      // Dane aktualne (weather)
      String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(lat, 6) + "&lon=" + String(lon, 6) + "&units=metric&appid=" + apiKey + "&lang=pl";
      HTTPClient http;
      http.begin(url);
      int code = http.GET();
      if (code > 0) {
        String resp = http.getString();
        JsonDocument doc;
        if (!deserializeJson(doc, resp)) {
          temp        = doc["main"]["temp"] | 0.0;
          feels_like  = doc["main"]["feels_like"] | 0.0;
          temp_min    = doc["main"]["temp_min"] | 0.0;
          temp_max    = doc["main"]["temp_max"] | 0.0;
          humidity    = doc["main"]["humidity"] | 0.0;
          pressure    = doc["main"]["pressure"] | 0.0;
          wind        = doc["wind"]["speed"] | 0.0;
          wind_deg    = doc["wind"]["deg"] | 0.0;
          clouds      = doc["clouds"]["all"] | 0.0;
          visibility  = doc["visibility"] | 0.0;
          rain        = doc["rain"]["1h"] | 0.0;

          weather_desc = "";
          icon = "";
          if (doc["weather"].is<JsonArray>() && doc["weather"].size() > 0) {
            weather_desc = doc["weather"][0]["description"].as<const char*>();
            icon         = doc["weather"][0]["icon"].as<const char*>();
          }
          // Wschód/zachód słońca w UNIX timestamp
          time_t sunrise_ts = doc["sys"]["sunrise"] | 0;
          time_t sunset_ts  = doc["sys"]["sunset"] | 0;
          char buf[8];
          struct tm t;
          if (sunrise_ts) {
            localtime_r(&sunrise_ts, &t);
            snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
            sunrise = String(buf);
          } else sunrise = "";
          if (sunset_ts) {
            localtime_r(&sunset_ts, &t);
            snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
            sunset = String(buf);
          } else sunset = "";
        }
      }
      http.end();
    }

    // --- PROGNOZA: co 30 min ---
    if (now - lastForecast > 90000UL) {
      lastForecast = now;
      Serial.println("[Weather] Pobieranie prognozy OWM...");

      // GEO
      float lat = 0, lon = 0;
      String urlGeo = "http://api.openweathermap.org/geo/1.0/direct?q=" + location + "&limit=1&appid=" + apiKey;
      HTTPClient httpGeo;
      httpGeo.begin(urlGeo);
int codeGeo = httpGeo.GET();
if (codeGeo > 0) {
  String respGeo = httpGeo.getString();
  Serial.print("[Weather] Odpowiedź OWM GEO: "); Serial.println(respGeo);
  JsonDocument docGeo;
  if (!deserializeJson(docGeo, respGeo)) {
    if (docGeo.is<JsonArray>() && docGeo.size() > 0) {
      JsonObject obj = docGeo[0];
      lat = obj["lat"].as<float>();
      lon = obj["lon"].as<float>();
    }
  }
} else {
  Serial.print("[Weather] Błąd pobierania GEO! Kod HTTP: "); Serial.println(codeGeo);
}
httpGeo.end();
if (lat == 0 && lon == 0) { Serial.println("[Weather] Błąd: Brak współrzędnych!"); return; }


      // Prognoza godzinowa i dzienna
      String urlF = "http://api.openweathermap.org/data/2.5/forecast?lat=" + String(lat, 6) + "&lon=" + String(lon, 6) + "&appid=" + apiKey + "&units=metric";
      HTTPClient httpF;
      httpF.begin(urlF);
      int codeF = httpF.GET();
      if (codeF > 0) {
        String respF = httpF.getString();
        JsonDocument docF;
        if (!deserializeJson(docF, respF)) {
          rain_1h_forecast = 0;
          rain_6h_forecast = 0;
          temp_min_tomorrow = 0;
          temp_max_tomorrow = 0;

          // 1h = 1/3 pierwszego bloku (3h)
          if (docF["list"].size() >= 2) {
            float rain3h_0 = docF["list"][0]["rain"]["3h"] | 0.0;
            float rain3h_1 = docF["list"][1]["rain"]["3h"] | 0.0;
            rain_1h_forecast = rain3h_0 / 3.0;
            rain_6h_forecast = rain3h_0 + rain3h_1;
          }
          // Szukaj danych min/max temp na jutro (z datą = dzisiaj+1)
          time_t now_ts = time(NULL);
          struct tm t_now;
          localtime_r(&now_ts, &t_now);
          int day_tomorrow = t_now.tm_mday + 1;
          float min_t = 1000.0, max_t = -1000.0;
          for (JsonVariant v : docF["list"].as<JsonArray>()) {
            time_t ts = v["dt"] | 0;
            struct tm t;
            localtime_r(&ts, &t);
            if (t.tm_mday == day_tomorrow) {
              float t_min = v["main"]["temp_min"] | 0.0;
              float t_max = v["main"]["temp_max"] | 0.0;
              if (t_min < min_t) min_t = t_min;
              if (t_max > max_t) max_t = t_max;
            }
          }
          temp_min_tomorrow = (min_t < 1000.0) ? min_t : 0.0;
          temp_max_tomorrow = (max_t > -1000.0) ? max_t : 0.0;
        }
      }
      httpF.end();
    }
  }

  void toJson(JsonDocument& doc) {
    doc["temp"] = temp;
    doc["feels_like"] = feels_like;
    doc["humidity"] = humidity;
    doc["pressure"] = pressure;
    doc["wind"] = wind;
    doc["wind_deg"] = wind_deg;
    doc["clouds"] = clouds;
    doc["visibility"] = (int)(visibility / 1000); // w km
    doc["weather_desc"] = weather_desc;
    doc["icon"] = icon;
    doc["rain"] = rain;
    doc["rain_1h_forecast"] = rain_1h_forecast;
    doc["rain_6h_forecast"] = rain_6h_forecast;
    doc["sunrise"] = sunrise;
    doc["sunset"] = sunset;
    doc["temp_min"] = temp_min;
    doc["temp_max"] = temp_max;
    doc["temp_min_tomorrow"] = temp_min_tomorrow;
    doc["temp_max_tomorrow"] = temp_max_tomorrow;
  }

  // Algorytm sterowania podlewaniem (zgodnie z Twoimi wcześniejszymi założeniami)
  int getWateringPercent() {
    if (rain_1h_forecast > 4.0) return 0; // nie podlewaj
    if (rain_6h_forecast > 2.0 && rain_6h_forecast <= 4.0) return 50; // podlewaj 50%
    return 100; // pełne podlewanie
  }
  bool wateringAllowed() {
    return getWateringPercent() > 0;
  }
};
