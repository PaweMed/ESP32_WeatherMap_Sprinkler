#pragma once
#include <Arduino.h>
#include <HTTPClient.h>

float forecasted_rain = 0.0;
unsigned long lastWeatherCheck = 0;
const unsigned long weatherCheckInterval = 3600000; // 1 godzina

bool isRainExpected() {
  return forecasted_rain > config.rain_threshold;
}

void updateWeatherIfNeeded() {
  if (millis() - lastWeatherCheck > weatherCheckInterval) {
    Serial.println("Updating weather forecast...");
    
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/forecast?q=" + config.owm_city + "&appid=" + config.owm_api_key + "&units=metric";
    http.begin(url);
    
    int httpCode = http.GET();
    if (httpCode > 0) {
      String payload = http.getString();
      DynamicJsonDocument doc(4096);
      deserializeJson(doc, payload);

      // Sprawdzamy prognozę na najbliższe 12 godzin
      float max_rain_12h = 0.0;
      JsonArray list = doc["list"];

      // OWM daje prognozę co 3 godziny, więc sprawdzamy 4 okresy (12h)
      for(int i=0; i<4 && i < list.size(); i++){
        float rain_3h = list[i]["rain"]["3h"] | 0.0;
        if(rain_3h > max_rain_12h){
          max_rain_12h = rain_3h;
        }
      }
      forecasted_rain = max_rain_12h;
      Serial.printf("Weather updated. Forecasted rain in next 12h: %.2f mm\n", forecasted_rain);

    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
    lastWeatherCheck = millis();
  }
}
