#include "wifi_manager.h"
#include "../lang/lang.h"
#include "../maint/serial_spiffs.h"

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <esp_wifi.h>
#include <ESP.h>
#include <vector>

static const char* WIFI_LAST_SSID_FILE = "/wifi.last";
static const uint32_t WIFI_CONNECT_TIMEOUT_STARTUP_MS   = 5000;
static const uint32_t WIFI_CONNECT_TIMEOUT_RECONNECT_MS = 5000;
static const uint32_t WIFI_RECONNECT_SETTLE_MS          = 1500;
static const uint32_t WIFI_RECONNECT_NEXT_SSID_MS       = 250;
static const uint32_t WIFI_RECONNECT_BACKOFF_MIN_MS     = 3000;
static const uint32_t WIFI_RECONNECT_BACKOFF_MAX_MS     = 30000;

static const char* WIFI_CRED_FILE = "/wifi.txt"; // ismétlődő SSID/PASS sorpárok

struct WifiCred {
  String ssid;
  String pass;
};

static WifiManagerPortalHelpFn g_portalHelpFn = nullptr;
static WifiManagerRestoredFn   g_restoredFn   = nullptr;
static WifiManagerAttemptFn    g_attemptFn    = nullptr;

static std::vector<WifiCred> g_wifiCreds;
static bool     g_haveWiFiCreds      = false;
static int      g_lastGoodCredIndex  = 0;
static String   g_currentAttemptSsid;
static String   g_activeSsid;
static String   g_preferredSsid;

static bool     g_wifiWasConnected    = false;
static uint32_t g_wifiDownSince       = 0;
static uint32_t g_wifiLastAttempt     = 0;
static uint32_t g_wifiAttemptInterval = WIFI_RECONNECT_BACKOFF_MIN_MS;
static uint32_t g_wifiAttemptCount    = 0;
static uint32_t g_wifiConnectedAt     = 0;

#define WIFI_FALLBACK_TO_PORTAL 0
static const uint32_t WIFI_RETRY_TO_PORTAL_MS = 120000;

enum class ReconnectState : uint8_t {
  Idle,
  WaitingBeforeRetry,
  Attempting
};

static ReconnectState g_reconnectState = ReconnectState::Idle;
static int      g_reconnectCredIndex   = 0;
static int      g_reconnectCycleCount  = 0;
static uint32_t g_reconnectStateAt     = 0;
static uint32_t g_reconnectAttemptFrom = 0;
static uint32_t g_reconnectWaitMs       = 0;
static size_t   g_reconnectTriedInCycle = 0;

static bool readNextNonEmptyLine(File& f, String& out) {
  while (f.available()) {
    out = f.readStringUntil('\n');
    out.trim();
    if (out.length() > 0) return true;
  }
  out = "";
  return false;
}

static bool loadWiFiCredsFromSPIFFS(std::vector<WifiCred>& outCreds) {
  outCreds.clear();
  if (!SPIFFS.exists(WIFI_CRED_FILE)) return false;

  File f = SPIFFS.open(WIFI_CRED_FILE, "r");
  if (!f) return false;

  while (true) {
    String ssid;
    if (!readNextNonEmptyLine(f, ssid)) break;

    String pass;
    if (!f.available()) {
      pass = "";
    } else {
      pass = f.readStringUntil('\n');
      pass.trim();
    }

    if (ssid.length() > 0) {
      outCreds.push_back({ssid, pass});
    }
  }

  f.close();
  return !outCreds.empty();
}

static bool loadPreferredSsidFromSPIFFS(String& outSsid) {
  outSsid = "";
  if (!SPIFFS.exists(WIFI_LAST_SSID_FILE)) return false;
  File f = SPIFFS.open(WIFI_LAST_SSID_FILE, "r");
  if (!f) return false;
  outSsid = f.readStringUntil('\n');
  outSsid.trim();
  f.close();
  return outSsid.length() > 0;
}

static void savePreferredSsidToSPIFFS(const String& ssid) {
  if (!ssid.length()) return;
  File f = SPIFFS.open(WIFI_LAST_SSID_FILE, "w");
  if (!f) return;
  f.println(ssid);
  f.close();
}

static void refreshPreferredCredIndex() {
  g_lastGoodCredIndex = 0;
  if (!g_preferredSsid.length()) return;
  for (size_t i = 0; i < g_wifiCreds.size(); ++i) {
    if (g_wifiCreds[i].ssid == g_preferredSsid) {
      g_lastGoodCredIndex = (int)i;
      return;
    }
  }
}

static bool saveWiFiCredsToSPIFFS(const String& ssid, const String& pass) {
  File f = SPIFFS.open(WIFI_CRED_FILE, "w");
  if (!f) return false;
  f.println(ssid);
  f.println(pass);
  f.close();
  return true;
}

static void resetReconnectState() {
  g_reconnectState = ReconnectState::Idle;
  g_reconnectCredIndex = g_lastGoodCredIndex;
  g_reconnectCycleCount = 0;
  g_reconnectStateAt = 0;
  g_reconnectAttemptFrom = 0;
  g_reconnectTriedInCycle = 0;
  g_reconnectWaitMs = 0;
  g_currentAttemptSsid = "";
}

static void markConnectedCommon(const String& ssid, int credIndex, bool restoredCallback) {
  g_activeSsid = ssid.length() ? ssid : WiFi.SSID();
  g_currentAttemptSsid = "";
  g_preferredSsid = g_activeSsid;
  savePreferredSsidToSPIFFS(g_activeSsid);
  if (credIndex >= 0 && credIndex < (int)g_wifiCreds.size()) {
    g_lastGoodCredIndex = credIndex;
  } else {
    refreshPreferredCredIndex();
  }

  g_wifiConnectedAt = millis();
  g_wifiWasConnected = true;
  g_wifiDownSince = 0;
  g_wifiLastAttempt = 0;
  g_wifiAttemptCount = 0;
  g_wifiAttemptInterval = WIFI_RECONNECT_BACKOFF_MIN_MS;
  resetReconnectState();

  Serial.printf("[WiFi] Connected. IP: %s  RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());

  if (restoredCallback && g_restoredFn) g_restoredFn();
}

static void beginConnectAttempt(const WifiCred& cred, int idx, int total, uint32_t now) {
  g_currentAttemptSsid = cred.ssid;
  if (g_attemptFn) g_attemptFn(cred.ssid.c_str(), idx + 1, total);

  Serial.printf("[WiFi] Attempt %d/%d -> %s\n", idx + 1, total, cred.ssid.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, false);
  delay(60);
  WiFi.begin(cred.ssid.c_str(), cred.pass.c_str());

  g_reconnectAttemptFrom = now;
  g_reconnectStateAt = now;
  g_reconnectState = ReconnectState::Attempting;
}

static bool connectWiFi(const String& ssid, const String& pass, uint32_t timeoutMs) {
  if (!ssid.length()) return false;

  g_currentAttemptSsid = ssid;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, false);
  delay(100);

  Serial.printf("[WiFi] Connecting to '%s'...\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      markConnectedCommon(ssid, -1, false);
      return true;
    }
    delay(150);
  }

  Serial.printf("[WiFi] Connect timeout. status=%d\n", (int)WiFi.status());
  return false;
}

static bool connectAnySavedWiFi(uint32_t timeoutMs) {
  if (g_wifiCreds.empty()) return false;

  const size_t n = g_wifiCreds.size();
  const size_t start = (g_lastGoodCredIndex >= 0 && g_lastGoodCredIndex < (int)n)
                         ? (size_t)g_lastGoodCredIndex
                         : 0;

  for (size_t offset = 0; offset < n; ++offset) {
    const size_t idx = (start + offset) % n;
    const WifiCred& cred = g_wifiCreds[idx];
    g_currentAttemptSsid = cred.ssid;
    if (g_attemptFn) g_attemptFn(cred.ssid.c_str(), (int)idx + 1, (int)n);
    Serial.printf("[WiFi] Attempt %d/%d -> %s\n", (int)idx + 1, (int)n, cred.ssid.c_str());
    if (connectWiFi(cred.ssid, cred.pass, timeoutMs)) {
      g_lastGoodCredIndex = (int)idx;
      return true;
    }
  }

  return false;
}

static void startWiFiConfigPortal() {
  const char* apSsid = "myRadio-Setup";
  const char* apPass = "";

  WiFi.disconnect(true, true);
  delay(100);

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(apSsid, apPass);
  IPAddress ip = WiFi.softAPIP();
  if (g_portalHelpFn) g_portalHelpFn(apSsid, ip);

  Serial.printf("[WiFi] Config portal AP: %s  IP: %s  (%s)\n",
                apSsid, ip.toString().c_str(), ok ? "OK" : "FAIL");

  WebServer server(80);

  auto page = []() -> String {
    String html;
    html += "<!doctype html><html><head><meta charset='utf-8'/>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'/>";
    html += "<title>";
    html += lang::portal_page_title;
    html += "</title></head><body style='font-family:sans-serif;max-width:520px;margin:24px auto;'>";
    html += "<h2>";
    html += lang::portal_heading;
    html += "</h2>";
    html += "<form method='POST' action='/save'>";
    html += "<label>";
    html += lang::portal_ssid_label;
    html += "<br><input name='s' style='width:100%;padding:10px' required></label><br><br>";
    html += "<label>";
    html += lang::portal_password_label;
    html += "<br><input name='p' type='password' style='width:100%;padding:10px'></label><br><br>";
    html += "<button style='padding:10px 16px'>";
    html += lang::portal_save_button;
    html += "</button>";
    html += "</form>";
    html += "<p style='opacity:.7'>";
    html += lang::portal_restart_notice;
    html += "</p>";
    html += "</body></html>";
    return html;
  };

  server.on("/", HTTP_GET, [&]() {
    server.send(200, "text/html; charset=utf-8", page());
  });

  server.on("/save", HTTP_POST, [&]() {
    String ssid = server.arg("s"); ssid.trim();
    String pass = server.arg("p"); pass.trim();

    if (!ssid.length()) {
      server.send(400, "text/plain; charset=utf-8", lang::portal_missing_ssid);
      return;
    }

    bool saved = saveWiFiCredsToSPIFFS(ssid, pass);
    if (saved) {
      server.send(200, "text/plain; charset=utf-8", lang::portal_save_ok);
      delay(400);
      ESP.restart();
    } else {
      server.send(500, "text/plain; charset=utf-8", lang::portal_save_failed);
    }
  });

  server.begin();

  while (true) {
    serial_spiffs_poll();
    if (serial_spiffs_is_active()) {
      server.stop();
      WiFi.softAPdisconnect(true);
      Serial.println("[MRSPIFS] WiFi config portal overridden by serial maintenance");
      return;
    }
    server.handleClient();
    delay(2);
  }
}

void wifi_manager_init(WifiManagerPortalHelpFn portalHelpFn,
                       WifiManagerRestoredFn restoredFn,
                       WifiManagerAttemptFn attemptFn) {
  g_portalHelpFn = portalHelpFn;
  g_restoredFn = restoredFn;
  g_attemptFn = attemptFn;
  g_wifiCreds.clear();
  g_wifiCreds.reserve(4);
  g_currentAttemptSsid.reserve(64);
  g_activeSsid.reserve(64);
  g_preferredSsid.reserve(64);
  resetReconnectState();
}

bool wifi_manager_begin_or_portal() {
  WiFi.persistent(false);
  esp_wifi_clear_fast_connect();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(50);

  g_haveWiFiCreds = loadWiFiCredsFromSPIFFS(g_wifiCreds);
  loadPreferredSsidFromSPIFFS(g_preferredSsid);
  refreshPreferredCredIndex();
  resetReconnectState();

  bool ok = false;
  if (g_haveWiFiCreds) {
    ok = connectAnySavedWiFi(WIFI_CONNECT_TIMEOUT_STARTUP_MS);
  }

  if (!ok) {
    Serial.println("[WiFi] No saved creds or connect failed -> starting config portal");
    startWiFiConfigPortal();
    return false;
  }

  g_wifiWasConnected = (WiFi.status() == WL_CONNECTED);
  return true;
}

void wifi_manager_handle_reconnect() {
  wl_status_t st = WiFi.status();
  uint32_t now = millis();

  if (st == WL_CONNECTED) {
    if (!g_wifiWasConnected || g_reconnectState != ReconnectState::Idle) {
      int idx = -1;
      String ssid = WiFi.SSID();
      for (size_t i = 0; i < g_wifiCreds.size(); ++i) {
        if (g_wifiCreds[i].ssid == ssid) {
          idx = (int)i;
          break;
        }
      }
      markConnectedCommon(ssid, idx, !g_wifiWasConnected);
    }
    return;
  }

  if (g_wifiWasConnected) {
    g_wifiWasConnected = false;
    g_wifiDownSince = now;
    g_wifiLastAttempt = 0;
    g_wifiAttemptCount = 0;
    g_wifiAttemptInterval = WIFI_RECONNECT_BACKOFF_MIN_MS;
    g_activeSsid = "";
    g_currentAttemptSsid = "";
    g_reconnectCredIndex = g_lastGoodCredIndex;
    g_reconnectCycleCount = 0;
    g_reconnectTriedInCycle = 0;
    g_reconnectState = ReconnectState::WaitingBeforeRetry;
    g_reconnectStateAt = now;
    g_reconnectWaitMs = WIFI_RECONNECT_SETTLE_MS;
    Serial.println("[WiFi] Disconnected. Reconnect state machine started...");
  }

  if (!g_haveWiFiCreds) return;

#if WIFI_FALLBACK_TO_PORTAL
  if (g_wifiDownSince != 0 && (now - g_wifiDownSince) > WIFI_RETRY_TO_PORTAL_MS) {
    Serial.println("[WiFi] Down too long -> opening config portal.");
    startWiFiConfigPortal();
    return;
  }
#endif

  if (g_reconnectState == ReconnectState::Idle) {
    g_reconnectCredIndex = g_lastGoodCredIndex;
    g_reconnectCycleCount = 0;
    g_reconnectTriedInCycle = 0;
    g_reconnectState = ReconnectState::WaitingBeforeRetry;
    g_reconnectStateAt = now;
    g_reconnectWaitMs = WIFI_RECONNECT_SETTLE_MS;
  }

  if (g_reconnectState == ReconnectState::WaitingBeforeRetry) {
    uint32_t waitMs = g_reconnectWaitMs ? g_reconnectWaitMs
                                         : ((g_reconnectTriedInCycle == 0 && g_reconnectCycleCount == 0)
                                              ? WIFI_RECONNECT_SETTLE_MS
                                              : g_wifiAttemptInterval);

    if ((now - g_reconnectStateAt) < waitMs) return;

    const int total = (int)g_wifiCreds.size();
    if (total <= 0) return;

    if (g_reconnectCredIndex < 0 || g_reconnectCredIndex >= total) {
      g_reconnectCredIndex = g_lastGoodCredIndex;
      if (g_reconnectCredIndex < 0 || g_reconnectCredIndex >= total) g_reconnectCredIndex = 0;
    }

    g_wifiLastAttempt = now;
    g_reconnectWaitMs = 0;
    beginConnectAttempt(g_wifiCreds[(size_t)g_reconnectCredIndex], g_reconnectCredIndex, total, now);
    return;
  }

  if (g_reconnectState == ReconnectState::Attempting) {
    if ((now - g_reconnectAttemptFrom) < WIFI_CONNECT_TIMEOUT_RECONNECT_MS) {
      return;
    }

    Serial.printf("[WiFi] Reconnect timeout on '%s'. status=%d\n",
                  g_currentAttemptSsid.c_str(), (int)WiFi.status());

    const int total = (int)g_wifiCreds.size();
    if (total <= 0) {
      resetReconnectState();
      return;
    }

    g_reconnectCredIndex = (g_reconnectCredIndex + 1) % total;
    g_reconnectTriedInCycle++;

    if (g_reconnectTriedInCycle >= (size_t)total) {
      g_reconnectTriedInCycle = 0;
      g_reconnectCycleCount++;
      g_wifiAttemptCount++;
      g_wifiAttemptInterval = min((uint32_t)WIFI_RECONNECT_BACKOFF_MAX_MS,
                                  max((uint32_t)WIFI_RECONNECT_BACKOFF_MIN_MS,
                                      (uint32_t)(g_wifiAttemptInterval * 2)));
      Serial.printf("[WiFi] Reconnect cycle #%u failed. Next retry in %lu ms.\n",
                    (unsigned)g_wifiAttemptCount,
                    (unsigned long)g_wifiAttemptInterval);
      g_reconnectStateAt = now;
      g_reconnectWaitMs = g_wifiAttemptInterval;
    } else {
      g_reconnectStateAt = now;
      g_reconnectWaitMs = WIFI_RECONNECT_NEXT_SSID_MS;
    }

    g_currentAttemptSsid = "";
    g_reconnectState = ReconnectState::WaitingBeforeRetry;
  }
}

uint32_t* wifi_manager_connected_at_ptr() {
  return &g_wifiConnectedAt;
}

const char* wifi_manager_current_attempt_ssid() {
  return g_currentAttemptSsid.c_str();
}

const char* wifi_manager_active_ssid() {
  return g_activeSsid.c_str();
}

int wifi_manager_active_rssi_dbm() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  return (int)WiFi.RSSI();
}
