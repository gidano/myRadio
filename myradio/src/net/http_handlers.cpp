#include "http_handlers.h"
#include "../lang/lang.h"

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <SPIFFS.h>

#include "../core/station_store.h"
#include "../core/text_utils.h"
#include "../audio/playlist_meta.h"
#include "../audio/stream_core.h"
#include "../ui/ui_display.h"
#include "../hw/backlight.h"

#ifndef VOLUME_MIN
#define VOLUME_MIN 0
#endif
#ifndef VOLUME_MAX
#define VOLUME_MAX 21
#endif

extern WebServer server;

extern String g_stationName;
extern String g_stationUrl;
extern String g_artist;
extern String g_title;
extern String g_codec;
extern String g_pendingCodec;
extern String g_playUrl;

extern int g_Volume;
extern int g_bitrateK;
extern int g_pendingBitrateK;
extern int g_ch;
extern int g_sampleRate;
extern int g_bitsPerSample;
extern int g_pendingCh;
extern int g_pendingSampleRate;
extern int g_pendingBitsPerSample;
extern int g_stationCount;
extern int g_currentIndex;
extern int g_menuIndex;
extern int g_bufferPercent;

extern bool g_paused;
extern volatile bool g_restartRequested;
extern volatile uint32_t g_restartAtMs;

extern size_t g_bufferFilled;
extern size_t g_bufferFree;
extern size_t g_bufferTotal;

extern PlaylistMetaCtx g_playlistMetaCtx;
extern Station g_stations[MAX_STATIONS];

void togglePaused();
void updateStationNameUI();
void saveLastStationToNVS();
void saveLastStationToSPIFFS();
bool startPlaybackCurrent(bool allowReloadPlaylist);
void drawBottomBar();
bool app_isMenuMode();
void app_exitMenuRedrawPlayUI();

String g_uploadPath;

namespace {

File g_uploadFile;

String sanitizePath(String p) {
  p.trim();
  if (p.length() == 0) return String("/");
  if (!p.startsWith("/")) p = "/" + p;
  p.replace("\\", "/");
  while (p.indexOf("..") >= 0) p.replace("..", "");
  while (p.indexOf("//") >= 0) p.replace("//", "/");
  return p;
}

void ensureParentDirs(const String& fullPath) {
  int slash = fullPath.lastIndexOf('/');
  if (slash <= 0) return;
  String dir = fullPath.substring(0, slash);
  if (dir.length() == 0) return;

  String cur = "";
  int from = 0;
  while (from < (int)dir.length()) {
    int to = dir.indexOf('/', from);
    if (to < 0) to = dir.length();
    String part = dir.substring(from, to);
    if (part.length()) {
      cur += "/" + part;
      if (!SPIFFS.exists(cur)) SPIFFS.mkdir(cur);
    }
    from = to + 1;
  }
}

#if MYRADIO_LANG == MYRADIO_LANG_HU
static constexpr const char* WEB_INDEX_FILE = "/web/index_hu.html";
static constexpr const char* WEB_SEARCH_FILE = "/web/search_hu.html";
#elif MYRADIO_LANG == MYRADIO_LANG_EN
static constexpr const char* WEB_INDEX_FILE = "/web/index_en.html";
static constexpr const char* WEB_SEARCH_FILE = "/web/search_en.html";
#elif MYRADIO_LANG == MYRADIO_LANG_DE
static constexpr const char* WEB_INDEX_FILE = "/web/index_de.html";
static constexpr const char* WEB_SEARCH_FILE = "/web/search_de.html";
#elif MYRADIO_LANG == MYRADIO_LANG_PL
static constexpr const char* WEB_INDEX_FILE = "/web/index_pl.html";
static constexpr const char* WEB_SEARCH_FILE = "/web/search_pl.html";
#else
static constexpr const char* WEB_INDEX_FILE = "/web/index_en.html";
static constexpr const char* WEB_SEARCH_FILE = "/web/search_en.html";
#endif

String jsonMsg(bool ok, const char* msg) {
  String out = "{\"ok\":";
  out += ok ? "true" : "false";
  out += ",\"msg\":\"";
  out += msg;
  out += "\"}";
  return out;
}


String applyCommonHtmlPlaceholders(String html) {
  html.replace("{{MYRADIO_VERSION}}", MYRADIO_VERSION);
  return html;
}

String buildUploadPageHtml() {
  String html;
  html.reserve(1800);
  html += F("<!doctype html><html><head><meta charset=\"utf-8\"/>");
  html += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>");
  html += F("<title>"); html += lang::web_upload_html_title; html += F("</title>");
  html += F("<style>body{font-family:system-ui,Arial;margin:16px;background:#111;color:#eee}.card{max-width:640px;margin:0 auto;padding:16px;border:1px solid #333;border-radius:12px;background:#161616}input,button{width:100%;padding:10px;margin:8px 0;border-radius:10px;border:1px solid #333;background:#0f0f0f;color:#eee}button{background:#2b6cff;border:0;font-weight:600}small{color:#aaa}a{color:#7db1ff}</style>");
  html += F("</head><body><div class=\"card\">");
  html += F("<h2>"); html += lang::web_upload_title; html += F(" (SPIFFS)</h2>");
  html += F("<p><small>"); html += lang::web_upload_intro; html += F(" <b>"); html += WEB_INDEX_FILE; html += F("</b></small></p>");
  html += F("<form method=\"POST\" action=\"/upload\" enctype=\"multipart/form-data\">");
  html += F("<label>"); html += lang::web_upload_path_label; html += F(" (pl. "); html += WEB_INDEX_FILE; html += F(", /fonts/test_24.vlw)</label>");
  html += F("<input name=\"path\" placeholder=\""); html += WEB_INDEX_FILE; html += F("\" required />");
  html += F("<label>"); html += lang::web_upload_file_label; html += F("</label>");
  html += F("<input type=\"file\" name=\"file\" required />");
  html += F("<button type=\"submit\">"); html += lang::web_upload_button; html += F("</button></form>");
  html += F("<p><a href=\"/api/fs/list\">"); html += lang::web_filelist_json; html += F("</a></p>");
  html += F("<p><a href=\"/\">"); html += lang::web_back_to_radio; html += F("</a></p>");
  html += F("</div></body></html>");
  return html;
}


String getBodyPlain() {
  if (server.hasArg("plain")) return server.arg("plain");
  return String();
}

bool jsonFindString(const String& body, const char* key, String& out) {
  String k = String("\"") + key + "\"";
  int p = body.indexOf(k);
  if (p < 0) return false;
  p = body.indexOf(':', p + k.length());
  if (p < 0) return false;
  while (p < (int)body.length() && (body[p] == ':' || body[p] == ' ' || body[p] == '\t' || body[p] == '\r' || body[p] == '\n')) p++;
  if (p >= (int)body.length() || body[p] != '"') return false;
  p++;
  String v;
  for (int i = p; i < (int)body.length(); i++) {
    char c = body[i];
    if (c == '\\') {
      if (i + 1 < (int)body.length()) {
        char n = body[i + 1];
        if (n == '"' || n == '\\' || n == '/') { v += n; i++; continue; }
        if (n == 'n') { v += '\n'; i++; continue; }
        if (n == 'r') { v += '\r'; i++; continue; }
        if (n == 't') { v += '\t'; i++; continue; }
      }
      v += c;
      continue;
    }
    if (c == '"') { out = v; return true; }
    v += c;
  }
  return false;
}

bool jsonFindInt(const String& body, const char* key, int& out) {
  String k = String("\"") + key + "\"";
  int p = body.indexOf(k);
  if (p < 0) return false;
  p = body.indexOf(':', p + k.length());
  if (p < 0) return false;
  while (p < (int)body.length() && (body[p] == ':' || body[p] == ' ' || body[p] == '\t' || body[p] == '\r' || body[p] == '\n')) p++;
  if (p >= (int)body.length()) return false;
  if (body[p] == '"') p++;

  int end = p;
  while (end < (int)body.length()) {
    char c = body[end];
    if ((c >= '0' && c <= '9') || c == '-' || c == '+') { end++; continue; }
    break;
  }
  if (end == p) return false;

  out = body.substring(p, end).toInt();
  return true;
}

bool getParamString(const char* key, String& out) {
  if (server.hasArg(key)) { out = server.arg(key); return true; }
  String body = getBodyPlain();
  if (!body.length()) return false;
  return jsonFindString(body, key, out);
}

bool getParamInt(const char* key, int& out) {
  if (server.hasArg(key)) { out = server.arg(key).toInt(); return true; }
  String body = getBodyPlain();
  if (!body.length()) return false;
  return jsonFindInt(body, key, out);
}

void resetPlaybackMeta() {
  g_codec = "";
  g_bitrateK = 0;
  g_pendingCodec = "";
  g_pendingBitrateK = 0;
  g_ch = 0;
  g_sampleRate = 0;
  g_bitsPerSample = 0;
  g_pendingCh = 0;
  g_pendingSampleRate = 0;
  g_pendingBitsPerSample = 0;
  g_ch = 0;
  g_sampleRate = 0;
  g_bitsPerSample = 0;
  g_pendingCh = 0;
  g_pendingSampleRate = 0;
  g_pendingBitsPerSample = 0;
}

} // namespace

void handleUploadPage() {
  server.send(200, "text/html; charset=utf-8", buildUploadPageHtml());
}

void handleFsList() {
  String json = "[";
  bool first = true;

  File root = SPIFFS.open("/");
  if (!root) { server.send(500, "text/plain; charset=utf-8", lang::web_fs_open_failed); return; }
  File f = root.openNextFile();
  while (f) {
    if (!first) json += ",";
    first = false;
    json += "{\"name\":\"" + String(f.name()) + "\",\"size\":" + String((uint32_t)f.size()) + "}";
    f = root.openNextFile();
  }
  json += "]";
  server.send(200, "application/json; charset=utf-8", json);
}

void handleUploadDone() {
  server.send(200, "text/plain; charset=utf-8", "OK");
}

void handleFileUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    g_uploadPath = sanitizePath(server.arg("path"));
    ensureParentDirs(g_uploadPath);
    if (SPIFFS.exists(g_uploadPath)) SPIFFS.remove(g_uploadPath);
    g_uploadFile = SPIFFS.open(g_uploadPath, "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (g_uploadFile) g_uploadFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (g_uploadFile) g_uploadFile.close();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (g_uploadFile) { g_uploadFile.close(); SPIFFS.remove(g_uploadPath); }
  }
}

void handleRoot() {
  if (!SPIFFS.exists(WEB_INDEX_FILE)) {
    handleUploadPage();
    return;
  }
  File file = SPIFFS.open(WEB_INDEX_FILE, "r");
  if (!file) {
    server.send(500, "text/plain; charset=utf-8", lang::web_interface_not_found);
    return;
  }
  String html = file.readString();
  file.close();
  html = applyCommonHtmlPlaceholders(html);
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSearch() {
  if (!SPIFFS.exists(WEB_SEARCH_FILE)) {
    server.send(404, "text/plain; charset=utf-8", lang::web_search_not_found);
    return;
  }
  File file = SPIFFS.open(WEB_SEARCH_FILE, "r");
  if (!file) {
    server.send(500, "text/plain; charset=utf-8", lang::web_search_open_failed);
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

void handleGetStations() {
  Serial.println("handleGetStations() called");
  Serial.printf("g_stationCount: %d\n", g_stationCount);

  if (g_stationCount == 0) {
    Serial.println("No stations loaded!");
    server.send(200, "application/json", "{\"stations\":[],\"currentIndex\":0,\"volume\":0,\"paused\":false,\"artist\":\"\",\"title\":\"\",\"codec\":\"\",\"bitrate\":0}");
    return;
  }

  String json = "{\"stations\":[";
  for (int i = 0; i < g_stationCount; i++) {
    if (i > 0) json += ",";
    json += "{\"index\":" + String(i) + ",";

    String escapedName = g_stations[i].name;
    escapedName.replace("\\", "\\\\");
    escapedName.replace("\"", "\\\"");
    escapedName.replace("\n", "\\n");
    escapedName.replace("\r", "\\r");
    escapedName.replace("\t", "\\t");

    json += "\"name\":\"" + escapedName + "\",";

    String escapedUrl = g_stations[i].url;
    escapedUrl.replace("\\", "\\\\");
    escapedUrl.replace("\"", "\\\"");

    json += "\"url\":\"" + escapedUrl + "\"}";

    if (i < 5) {
      Serial.printf("Station %d: %s\n", i, g_stations[i].name.c_str());
    }
  }
  json += "],";
  json += "\"currentIndex\":" + String(g_currentIndex) + ",";
  json += "\"volume\":" + String(g_Volume) + ",";
  json += "\"paused\":" + String(g_paused ? "true" : "false") + ",";

  String escapedArtist = g_artist;
  escapedArtist.replace("\\", "\\\\");
  escapedArtist.replace("\"", "\\\"");

  String escapedTitle = g_title;
  escapedTitle.replace("\\", "\\\\");
  escapedTitle.replace("\"", "\\\"");

  String escapedCodec = g_codec;
  escapedCodec.replace("\\", "\\\\");
  escapedCodec.replace("\"", "\\\"");

  json += "\"artist\":\"" + escapedArtist + "\",";
  json += "\"title\":\"" + escapedTitle + "\",";
  json += "\"codec\":\"" + escapedCodec + "\",";
  json += "\"bitrate\":" + String(g_bitrateK) + "}";

  Serial.println("Sending JSON response");
  server.send(200, "application/json", json);
}

void handleSetStation() {
  if (!server.hasArg("index")) {
    server.send(400, "text/plain; charset=utf-8", lang::api_missing_index);
    return;
  }

  int index = server.arg("index").toInt();
  if (index < 0 || index >= g_stationCount) {
    server.send(400, "text/plain; charset=utf-8", lang::api_invalid_index);
    return;
  }

  g_currentIndex = index;
  g_stationName = g_stations[g_currentIndex].name;
  g_stationUrl = g_stations[g_currentIndex].url;

  updateStationNameUI();
  saveLastStationToNVS();
  saveLastStationToSPIFFS();

  startPlaybackCurrent(true);
  if (g_playUrl.length()) stream_core_sendConnect(g_playUrl);
  resetPlaybackMeta();

  if (app_isMenuMode()) {
    app_exitMenuRedrawPlayUI();
  }

  server.send(200, "text/plain", "OK");
}

void handleAddStation() {
  String name, url;
  if (!getParamString("name", name) || !getParamString("url", url)) {
    server.send(400, "text/plain; charset=utf-8", lang::api_missing_name_or_url);
    return;
  }

  if (g_stationCount >= MAX_STATIONS) {
    server.send(400, "text/plain; charset=utf-8", lang::api_station_list_full);
    return;
  }

  name.trim();
  url.trim();

  if (name.length() == 0 || url.length() == 0) {
    server.send(400, "text/plain; charset=utf-8", lang::api_missing_name_or_url);
    return;
  }

  name.replace("\t", " ");
  name.replace("\n", " ");
  name.replace("\r", " ");

  g_stations[g_stationCount].name = name;
  g_stations[g_stationCount].url  = url;
  g_stationCount++;

  if (!station_saveToSPIFFS(g_stations, g_stationCount)) {
    server.send(500, "text/plain; charset=utf-8", lang::api_save_failed_spiffs);
    return;
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeleteStation() {
  int del = -1;
  if (!getParamInt("index", del)) {
    server.send(400, "text/plain; charset=utf-8", lang::api_missing_index);
    return;
  }

  if (del < 0 || del >= g_stationCount) {
    server.send(400, "text/plain; charset=utf-8", lang::api_invalid_index);
    return;
  }

  for (int i = del; i < g_stationCount - 1; i++) {
    g_stations[i] = g_stations[i + 1];
  }
  g_stationCount--;

  if (g_stationCount <= 0) {
    g_stationCount = 0;
    g_currentIndex = 0;
    g_menuIndex = 0;
  } else {
    if (del < g_currentIndex) g_currentIndex--;
    if (g_currentIndex >= g_stationCount) g_currentIndex = g_stationCount - 1;
    g_menuIndex = g_currentIndex;

    g_stationName = g_stations[g_currentIndex].name;
    g_stationUrl = g_stations[g_currentIndex].url;

    updateStationNameUI();
    saveLastStationToNVS();
    saveLastStationToSPIFFS();

    if (!g_paused) {
      startPlaybackCurrent(true);
      if (g_playUrl.length()) stream_core_sendConnect(g_playUrl);
    }
  }

  if (!station_saveToSPIFFS(g_stations, g_stationCount)) {
    server.send(500, "text/plain; charset=utf-8", lang::api_save_failed_spiffs);
    return;
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleUpdateStation() {
  int idx = -1;
  if (!getParamInt("index", idx) && !getParamInt("idx", idx)) {
    server.send(400, "application/json", jsonMsg(false, lang::api_missing_index));
    return;
  }
  if (idx < 0 || idx >= g_stationCount) {
    server.send(400, "application/json", jsonMsg(false, lang::api_invalid_index));
    return;
  }

  String name, url;
  bool hasName = getParamString("name", name);
  bool hasUrl  = getParamString("url", url);
  if (!hasName) hasName = getParamString("title", name);

  if (!hasName && !hasUrl) {
    server.send(400, "application/json", jsonMsg(false, lang::api_missing_name_or_url));
    return;
  }

  if (hasName) {
    name.trim();
    if (!name.length()) {
      server.send(400, "application/json", jsonMsg(false, lang::api_empty_name));
      return;
    }
    name.replace("\t", " ");
    name.replace("\n", " ");
    name.replace("\r", " ");
    g_stations[idx].name = name;
  }

  if (hasUrl) {
    url.trim();
    if (!url.length()) {
      server.send(400, "application/json", jsonMsg(false, lang::api_empty_url));
      return;
    }
    g_stations[idx].url = url;
  }

  if (idx == g_currentIndex) {
    g_stationName = g_stations[g_currentIndex].name;
    g_stationUrl  = g_stations[g_currentIndex].url;
    updateStationNameUI();
    saveLastStationToNVS();
    saveLastStationToSPIFFS();
  }

  if (!station_saveToSPIFFS(g_stations, g_stationCount)) {
    server.send(500, "application/json", jsonMsg(false, lang::api_save_failed_spiffs));
    return;
  }

  String out = "{\"ok\":true,\"msg\":\"" + String(lang::api_saved) + "\",\"index\":" + String(idx) + "}";
  server.send(200, "application/json", out);
}

void handleMoveStation() {
  int from = -1, to = -1;

  bool okFrom =
      getParamInt("from", from) ||
      getParamInt("fromIndex", from) ||
      getParamInt("src", from) ||
      getParamInt("oldIndex", from) ||
      getParamInt("index", from);

  bool okTo =
      getParamInt("to", to) ||
      getParamInt("toIndex", to) ||
      getParamInt("dst", to) ||
      getParamInt("newIndex", to) ||
      getParamInt("target", to);

  if (!okFrom || !okTo) {
    server.send(400, "application/json", jsonMsg(false, lang::api_missing_from_to));
    return;
  }
  if (from < 0 || from >= g_stationCount || to < 0 || to >= g_stationCount) {
    server.send(400, "application/json", jsonMsg(false, lang::api_invalid_from_to));
    return;
  }
  if (from == to) {
    server.send(200, "application/json", jsonMsg(true, lang::api_no_change));
    return;
  }

  Station tmp = g_stations[from];
  if (from < to) {
    for (int i = from; i < to; i++) g_stations[i] = g_stations[i + 1];
    g_stations[to] = tmp;
  } else {
    for (int i = from; i > to; i--) g_stations[i] = g_stations[i - 1];
    g_stations[to] = tmp;
  }

  if (g_currentIndex == from) g_currentIndex = to;
  else if (from < to && g_currentIndex > from && g_currentIndex <= to) g_currentIndex--;
  else if (from > to && g_currentIndex >= to && g_currentIndex < from) g_currentIndex++;

  g_menuIndex = g_currentIndex;

  if (g_stationCount > 0) {
    g_stationName = g_stations[g_currentIndex].name;
    g_stationUrl = g_stations[g_currentIndex].url;
  }

  if (!station_saveToSPIFFS(g_stations, g_stationCount)) {
    server.send(500, "application/json", jsonMsg(false, lang::api_save_failed_spiffs));
    return;
  }

  String out = "{\"ok\":true,\"msg\":\"" + String(lang::api_order_saved) + "\",\"from\":" + String(from) + ",\"to\":" + String(to) +
               ",\"currentIndex\":" + String(g_currentIndex) + "}";
  server.send(200, "application/json", out);
}

void handleGetBrightness() {
  String out = "{\"value\":" + String((int)g_brightness) + "}";
  server.send(200, "application/json", out);
}

void handleSetBrightness() {
  int v = -1;
  if (server.hasArg("val")) v = server.arg("val").toInt();
  else if (server.hasArg("value")) v = server.arg("value").toInt();
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  hw_backlight_set((uint8_t)v);
  handleGetBrightness();
}

void handleSetVolume() {
  if (!server.hasArg("volume")) {
    server.send(400, "text/plain; charset=utf-8", lang::api_missing_volume);
    return;
  }

  int vol = server.arg("volume").toInt();
  if (vol < VOLUME_MIN || vol > VOLUME_MAX) {
    server.send(400, "text/plain; charset=utf-8", lang::api_invalid_volume);
    return;
  }

  g_Volume = vol;
  stream_core_sendVolume(g_Volume);
  drawBottomBar();

  server.send(200, "text/plain", "OK");
}

void handleTogglePause() {
  togglePaused();
  server.send(200, "text/plain", g_paused ? "paused" : "playing");
}

void handleGetStatus() {
  String json = "{";
  json += "\"currentIndex\":" + String(g_currentIndex) + ",";
  json += "\"stationName\":\"" + g_stationName + "\",";
  json += "\"volume\":" + String(g_Volume) + ",";
  json += "\"paused\":" + String(g_paused ? "true" : "false") + ",";
  json += "\"artist\":\"" + g_artist + "\",";
  json += "\"title\":\"" + g_title + "\",";
  json += "\"codec\":\"" + g_codec + "\",";
  json += "\"bitrate\":" + String(g_bitrateK) + "}";

  server.send(200, "application/json", json);
}

void handleTrackNext() {
  if (!text_endsWithIgnoreCase(g_stationUrl, ".m3u")) {
    server.send(400, "text/plain; charset=utf-8", lang::api_not_playlist_station);
    return;
  }
  if (!playlist_meta_stepPlaylist(g_playlistMetaCtx, +1, true)) {
    server.send(500, "text/plain; charset=utf-8", lang::api_advance_playlist_failed);
    return;
  }

  server.send(200, "application/json",
    String("{\"trackIndex\":") + playlist_meta_trackIndex(g_playlistMetaCtx) + ",\"trackCount\":" + playlist_meta_trackCount(g_playlistMetaCtx) + "}"
  );
}

void handleTrackPrev() {
  if (!text_endsWithIgnoreCase(g_stationUrl, ".m3u")) {
    server.send(400, "text/plain; charset=utf-8", lang::api_not_playlist_station);
    return;
  }
  if (!playlist_meta_stepPlaylist(g_playlistMetaCtx, -1, true)) {
    server.send(500, "text/plain; charset=utf-8", lang::api_rewind_playlist_failed);
    return;
  }

  server.send(200, "application/json",
    String("{\"trackIndex\":") + playlist_meta_trackIndex(g_playlistMetaCtx) + ",\"trackCount\":" + playlist_meta_trackCount(g_playlistMetaCtx) + "}"
  );
}

void handleNextStation() {
  if (g_stationCount == 0) {
    server.send(400, "text/plain; charset=utf-8", lang::api_no_stations);
    return;
  }

  g_currentIndex = (g_currentIndex + 1) % g_stationCount;
  g_stationName = g_stations[g_currentIndex].name;
  g_stationUrl = g_stations[g_currentIndex].url;

  updateStationNameUI();
  saveLastStationToNVS();
  saveLastStationToSPIFFS();

  startPlaybackCurrent(true);
  if (g_playUrl.length()) stream_core_sendConnect(g_playUrl);
  resetPlaybackMeta();

  server.send(200, "application/json", "{\"index\":" + String(g_currentIndex) + ",\"name\":\"" + g_stationName + "\"}");
}

void handlePrevStation() {
  if (g_stationCount == 0) {
    server.send(400, "text/plain; charset=utf-8", lang::api_no_stations);
    return;
  }

  g_currentIndex = (g_currentIndex - 1 + g_stationCount) % g_stationCount;
  g_stationName = g_stations[g_currentIndex].name;
  g_stationUrl = g_stations[g_currentIndex].url;

  updateStationNameUI();
  saveLastStationToNVS();
  saveLastStationToSPIFFS();

  startPlaybackCurrent(true);
  if (g_playUrl.length()) stream_core_sendConnect(g_playUrl);
  resetPlaybackMeta();

  server.send(200, "application/json", "{\"index\":" + String(g_currentIndex) + ",\"name\":\"" + g_stationName + "\"}");
}

void handleReset() {
  server.send(200, "application/json", "{\"ok\":true}");
  g_restartRequested = true;
  g_restartAtMs = millis() + 300;
}

void handleGetBuffer() {
  size_t filled = 0;
  size_t freeb  = 0;

  size_t total = 0;
  int percent = 0;
  stream_core_readBuffer(filled, freeb, total, percent);

  g_bufferFilled  = filled;
  g_bufferFree    = freeb;
  g_bufferTotal   = total;
  g_bufferPercent = percent;

  String json = "{";
  json += "\"percent\":" + String(percent) + ",";
  json += "\"filled\":"  + String((uint32_t)filled) + ",";
  json += "\"free\":"    + String((uint32_t)freeb) + ",";
  json += "\"total\":"   + String((uint32_t)total);
  json += "}";

  server.send(200, "application/json", json);
}
