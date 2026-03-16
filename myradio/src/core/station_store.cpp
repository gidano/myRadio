#include "station_store.h"
#include <SPIFFS.h>

bool station_parseLine(const String& lineRaw, String& name, String& url) {
  String line = lineRaw;
  line.trim();
  if (line.length() == 0) return false;
  if (line[0] == '#') return false;

  int sep = line.indexOf('\t');
  if (sep < 0) sep = line.indexOf('|');
  if (sep < 0) return false;

  name = line.substring(0, sep);
  url = line.substring(sep + 1);
  name.trim();
  url.trim();
  return (name.length() && url.length());
}

int station_findByUrlOrName(const Station* stations, int stationCount, const String& stationUrl, const String& stationName) {
  for (int i = 0; i < stationCount; i++) {
    if (stations[i].url == stationUrl || stations[i].name == stationName) {
      return i;
    }
  }
  return -1;
}

void station_loadFromSPIFFS(
  Station* stations,
  int maxStations,
  int& stationCount,
  int& currentIndex,
  int& menuIndex,
  String& stationName,
  String& stationUrl
) {
  stationCount = 0;

  File f = SPIFFS.open("/stations.txt", "r");
  if (!f) {
    Serial.println("Nincs /stations.txt (SPIFFS). Default station marad.");
    return;
  }

  while (f.available() && stationCount < maxStations) {
    String line = f.readStringUntil('\n');
    String name, url;
    if (station_parseLine(line, name, url)) {
      stations[stationCount].name = name;
      stations[stationCount].url = url;
      stationCount++;
    }
  }
  f.close();

  Serial.printf("Stations loaded: %d\n", stationCount);

  int foundIndex = station_findByUrlOrName(stations, stationCount, stationUrl, stationName);
  if (foundIndex >= 0) {
    currentIndex = foundIndex;
  }

  if (currentIndex < 0) currentIndex = 0;
  if (stationCount > 0 && currentIndex >= stationCount) currentIndex = 0;

  menuIndex = currentIndex;

  if (stationCount > 0) {
    stationName = stations[currentIndex].name;
    stationUrl = stations[currentIndex].url;
  }
}

bool station_saveToSPIFFS(const Station* stations, int stationCount) {
  File f = SPIFFS.open("/stations.txt", "w");
  if (!f) {
    Serial.println("[SPIFFS] Cannot open /stations.txt for write");
    return false;
  }

  for (int i = 0; i < stationCount; i++) {
    String name = stations[i].name;
    name.replace('\t', ' ');
    f.print(name);
    f.print("\t");
    f.println(stations[i].url);
  }

  f.close();
  Serial.println("[SPIFFS] Stations saved to /stations.txt");
  return true;
}
