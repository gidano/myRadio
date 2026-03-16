#pragma once
#include <Arduino.h>

struct Station {
  String name;
  String url;
};

static const int MAX_STATIONS = 120;

bool station_parseLine(const String& lineRaw, String& name, String& url);
int station_findByUrlOrName(const Station* stations, int stationCount, const String& stationUrl, const String& stationName);
void station_loadFromSPIFFS(
  Station* stations,
  int maxStations,
  int& stationCount,
  int& currentIndex,
  int& menuIndex,
  String& stationName,
  String& stationUrl
);
bool station_saveToSPIFFS(const Station* stations, int stationCount);
