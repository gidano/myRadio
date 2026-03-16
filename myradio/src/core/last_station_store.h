#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "station_store.h"

bool station_last_load_spiffs(String& outUrl, String& outName);
void station_last_save_spiffs(const String& stationUrl, const String& stationName);

void station_last_load_nvs(
  Preferences& prefs,
  const Station* stations,
  int stationCount,
  int& currentIndex,
  int& menuIndex,
  String& stationName,
  String& stationUrl,
  Print& logSerial
);

void station_last_save_nvs(
  Preferences& prefs,
  const String& stationUrl,
  const String& stationName
);
