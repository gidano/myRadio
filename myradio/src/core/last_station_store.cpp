#include "last_station_store.h"
#include <SPIFFS.h>

static const char* LAST_STATION_FILE = "/last_station.txt";

bool station_last_load_spiffs(String& outUrl, String& outName) {
  File f = SPIFFS.open(LAST_STATION_FILE, "r");
  if (!f) return false;
  outUrl = f.readStringUntil('\n'); outUrl.trim();
  outName = f.readStringUntil('\n'); outName.trim();
  f.close();
  return (outUrl.length() || outName.length());
}

void station_last_save_spiffs(const String& stationUrl, const String& stationName) {
  File f = SPIFFS.open(LAST_STATION_FILE, "w");
  if (!f) {
    Serial.println("[SPIFFS] Cannot open last_station.txt for write");
    return;
  }
  f.println(stationUrl);
  f.println(stationName);
  f.close();
  Serial.println("[SPIFFS] Last station saved to /last_station.txt");
}

void station_last_load_nvs(
  Preferences& prefs,
  const Station* stations,
  int stationCount,
  int& currentIndex,
  int& menuIndex,
  String& stationName,
  String& stationUrl,
  Print& logSerial
) {
  prefs.begin("myradio", true);
  String lastUrl  = prefs.getString("url", "");
  String lastName = prefs.getString("name", "");
  prefs.end();

  if (lastUrl.length() == 0 && lastName.length() == 0) {
    String u2, n2;
    if (station_last_load_spiffs(u2, n2)) {
      lastUrl = u2;
      lastName = n2;
      logSerial.println("[SPIFFS] Last station loaded from /last_station.txt");
    } else {
      return;
    }
  }

  if (stationCount > 0) {
    for (int i = 0; i < stationCount; i++) {
      if ((lastUrl.length() && stations[i].url == lastUrl) ||
          (lastName.length() && stations[i].name == lastName)) {
        currentIndex = i;
        menuIndex = i;
        stationName = stations[i].name;
        stationUrl  = stations[i].url;
        logSerial.printf("[NVS] Last station restored: %s\n", stationName.c_str());
        return;
      }
    }
  }

  if (lastUrl.length()) {
    stationUrl = lastUrl;
    if (lastName.length()) stationName = lastName;
    logSerial.printf("[NVS] Last station URL restored (no match in list): %s\n", stationUrl.c_str());
  }
}

void station_last_save_nvs(
  Preferences& prefs,
  const String& stationUrl,
  const String& stationName
) {
  prefs.begin("myradio", false);
  prefs.putString("url", stationUrl);
  prefs.putString("name", stationName);
  prefs.end();
}
