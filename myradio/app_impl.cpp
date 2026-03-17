// app_impl.cpp
#include <Arduino.h>
#include <WiFi.h>
#include "src/net/net_server.h"
#include <esp_wifi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <vector>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdarg.h>
#include "Audio.h"

#include "Lovyan_config.h"
#include <LovyanGFX.hpp>
#include "conf/display_profile.h"
#include <SPIFFS.h>
#include "Rotary.h"
#include "logo_rgb565_60x60.h"
#include "audio_icons/aac_60.h"
#include "audio_icons/flac_60.h"
#include "audio_icons/mp3_60.h"
#include "audio_icons/ogg_60.h"
#include "audio_icons/opus_60.h"
#include "audio_icons/vor_60.h"
#include "src/input/input_encoder.h"
#include "src/input/input_rotary.h"
#include "src/core/state_meta.h"
#include "src/core/text_utils.h"
#include "src/core/station_store.h"
#include "src/core/last_station_store.h"
#include "src/net/wifi_manager.h"
#include "src/net/http_api.h"
#include "src/net/http_handlers.h"
#include "src/audio/stream_core.h"
#include "src/audio/playlist_meta.h"
#include "src/audio/audio_control.h"
#include "src/audio/playlist_runtime.h"
#include "src/audio/stream_lifecycle.h"
#include "src/audio/stream_watchdog.h"
#include "src/ui/ui_display.h"
#include "src/ui/ui_station_selector.h"
#include "src/hw/backlight.h"
#include "src/input/input_encoder_isr.h"
#include "src/input/input_button.h"
#include "src/myradiologo_240.h"

// VU meter hooks (audio_process_i2s -> vu_feedStereoISR)
#include "src/ui/vu_meter.h"
#include "src/lang/lang.h"


// Fallback színek, ha a platform nem definiálná ezeket a neveket
#ifndef TFT_LIGHTGREY
  #define TFT_LIGHTGREY 0xC618
#endif
#ifndef TFT_DARKGREY
  #define TFT_DARKGREY  0x7BEF
#endif

using LGFX_Sprite = lgfx::LGFX_Sprite;

// ------------------ Forward declarations (needed because some helpers are static) ------------------
void saveLastStationToNVS();
void saveLastStationToSPIFFS();
static void renderLine(LGFX_Sprite& spr, const String& text, int32_t x);
static void recalcTextMetrics();
static void reserveHotStrings();
static void logMemorySnapshot(const char* tag);
static void serialLogf(const char* fmt, ...);
static void serialLogln(const String& s);
static void serialLogln(const char* s);
static String clipTextKeepRight(LGFX_Device* dev, const String& s, int maxW);
bool app_isMenuMode();
void app_exitMenuRedrawPlayUI();
bool startPlaybackCurrent(bool allowReloadPlaylist);
void drawStartupScreen(uint8_t phase);


static SemaphoreHandle_t g_logMutex = nullptr;

static inline void ensureLogMutex() {
  if (!g_logMutex) g_logMutex = xSemaphoreCreateMutex();
}

static void serialLogf(const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (g_logMutex && xSemaphoreTake(g_logMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    Serial.print(buf);
    xSemaphoreGive(g_logMutex);
  } else {
    Serial.print(buf);
  }
}

static void serialLogln(const String& s) {
  if (g_logMutex && xSemaphoreTake(g_logMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    Serial.println(s);
    xSemaphoreGive(g_logMutex);
  } else {
    Serial.println(s);
  }
}

static void serialLogln(const char* s) {
  if (g_logMutex && xSemaphoreTake(g_logMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    Serial.println(s);
    xSemaphoreGive(g_logMutex);
  } else {
    Serial.println(s);
  }
}

#ifndef TFT_MAGENTA
  #define TFT_MAGENTA 0xF81F
#endif





#define VOLUME_MIN 0
#define VOLUME_MAX 21

// Audio watchdog (soft recovery when input buffer stays empty while running)
#define AUDIO_WATCHDOG 1

// Kíméletes futás két magon:
// - Arduino loop / UI / web szerver marad az alapértelmezett loopTask magon
// - az audio dekódolás külön taskon fut, fixen a másik magon
#ifndef AUDIO_TASK_CORE
#define AUDIO_TASK_CORE 0
#endif
#ifndef AUDIO_TASK_STACK
#define AUDIO_TASK_STACK 10240
#endif
#ifndef AUDIO_TASK_PRIORITY
#define AUDIO_TASK_PRIORITY 6
#endif

static uint32_t g_connectRequestedAt = 0;
static bool     g_needStreamReconnect = false;

// NVS (flash) - utolsó állomás megjegyzése
Preferences prefs;

// Default station (ha nincs file / üres)
String g_stationName = "Zebrádió";
String g_stationUrl  = "https://stream.zebradio.hu:8443/zebradio";


// ---- Playlist (M3U) support for local PC server ----
static std::vector<String> g_playlistUrls;
static int g_playlistIndex = -1;
static String g_playlistSourceUrl = "";   // the .m3u URL that produced g_playlistUrls
String g_playUrl = "";             
PlaylistMetaCtx g_playlistMetaCtx;
// Last URL we asked the audio task to connect to (for watchdog recovery)
static String g_lastConnectUrl = "";
// the actual currently-playing URL (track or stream)


// Auto-advance for M3U playlists (when a track ends)
static volatile bool g_autoNextRequested = false;
static volatile uint32_t g_autoNextRequestedAt = 0;
int g_Volume = 5;

// lejátszás szüneteltetése (enkóder rövid nyomás)
bool g_paused = false;

// Web server
WebServer server(80);

// ------------------ SPIFFS Web Uploader (always available) ------------------ //
static bool g_webClientConnected = false;

// Web reboot (reset) request flag (so the HTTP response can be sent before reboot)
volatile bool g_restartRequested = false;
volatile uint32_t g_restartAtMs = 0;
static String g_webResponseBuffer = "";

static bool    g_startupConnectScreenActive = false;
static uint8_t g_startupConnectPhase = 1;
static int     g_startupAttemptIndex = 0;
static int     g_startupAttemptTotal = 0;

static void onWiFiAttempt(const char* ssid, int index, int total) {
  g_startupAttemptIndex = index;
  g_startupAttemptTotal = total;
  if (!g_startupConnectScreenActive) return;
  drawStartupScreen(g_startupConnectPhase ? g_startupConnectPhase : 1);
}

LGFX tft;
static bool g_tftReady = false;

static void initDisplayBasic() {
  if (g_tftReady) return;
  // Kijelző init (ne maradjon fehér / üres a háttérvilágítás alatt)
  tft.init();
  tft.setRotation(TFT_ROTATION);      // a Lovyan_config.h-ban állítod be
  tft.setBrightness(255);
  tft.fillScreen(TFT_BLACK);
  g_tftReady = true;
}

static void drawWiFiPortalHelp(const char* apSsid, const IPAddress& ip) {
  initDisplayBasic();

  // Fekete háttér + fehér keretes "ablak"
  tft.fillScreen(TFT_BLACK);

  const int pad = 8;
  const int w = tft.width()  - pad * 2;
  const int h = tft.height() - pad * 2;
  tft.drawRoundRect(pad, pad, w, h, 12, TFT_WHITE);

  // Ha a 20-as VLW font hiányzik a SPIFFS-ből, visszaesünk 24-re.
  const char* fs20    = "/fonts/test_20.vlw";
  const char* fsSB20  = "/fonts/test_sb_20.vlw";
  const char* fs24    = "/fonts/test_24.vlw";
  const char* fsSB24  = "/fonts/test_sb_24.vlw";

  const bool have20 = SPIFFS.exists(fs20) && SPIFFS.exists(fsSB20);

  // LovyanGFX loadFont SPIFFS-hez általában /spiffs/... útvonalat vár.
  const char* fontTitle = (have20 ? "/spiffs/fonts/test_sb_20.vlw" : "/spiffs/fonts/test_sb_24.vlw");
  const char* fontBody  = (have20 ? "/spiffs/fonts/test_20.vlw"    : "/spiffs/fonts/test_24.vlw");

  const int x = pad + 10;
  int y = pad + 10;
  const bool compactWifiSetup = (tft.width() <= 320 || tft.height() <= 240);
  const int titleGap = compactWifiSetup ? 4 : 6;
  const int lineGap  = 1;
  const int blockGap = compactWifiSetup ? 1 : 3;

  // Címsor: SemiBold 20
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.loadFont(fontTitle);
  tft.setCursor(x + 60, y);
  tft.println(lang::wifi_setup_about_title);
  y += tft.fontHeight() + titleGap;

  // Törzsszöveg: 20
  tft.loadFont(fontBody);

  // 1)
  tft.setCursor(x, y);
  tft.println(lang::wifi_setup_step1);
  y += tft.fontHeight() + lineGap;
  tft.setCursor(x + 18, y);
  tft.print(" ");
  tft.println(apSsid);
  y += tft.fontHeight() + blockGap;

  // 2)
  tft.setCursor(x, y);
  tft.println(lang::wifi_setup_step2);
  y += tft.fontHeight() + lineGap;
  tft.setCursor(x + 22, y);
  tft.print("http://");
  tft.println(ip.toString());
  y += tft.fontHeight() + blockGap;

  // 3)
  tft.setCursor(x, y);
  tft.println(lang::wifi_setup_step3);
  y += tft.fontHeight() + lineGap;
  tft.setCursor(x + 22, y);
  tft.println(lang::wifi_setup_save_hint);
  y += tft.fontHeight() + blockGap;

  // Footer
  tft.setCursor(x, y);
  tft.setCursor(x + 68, y);
  tft.println(lang::wifi_setup_restart_hint);
  y += tft.fontHeight() + lineGap;

  // Ha nem kell tovább, vissza lehet később váltani másik fontra máshol.
  // (Nem unloadFont-olunk, mert a UI is VLW-t használ.)
}

Audio audio;


Rotary EncL = Rotary(ENC_A, ENC_B);

// ------------------ Sprites ------------------ //

LGFX_Sprite sprStation(&tft);
LGFX_Sprite sprArtist(&tft);
LGFX_Sprite sprTitle(&tft);
LGFX_Sprite sprMenu(&tft);

// ------------------ Fontok (SPIFFS) ------------------ //
// FONT FÁJLOK A SPIFFS-BEN:
//  - /fonts/test_XX.vlw  (és /fonts/test_sb_XX.vlw)

// SPIFFS útvonal (SPIFFS API-hoz: exists/open)
static String FS_20, FS_24, FS_28;
static String FS_SB_20, FS_SB_24, FS_SB_28;

// LGFX útvonal (LGFX loadFont-hoz: /spiffs/...)
static String FP_20, FP_24, FP_28;
static String FP_SB_20, FP_SB_24, FP_SB_28;
static String* uiRegularFontPtr(int preferredSize);
static String* uiSemiboldFontPtr(int preferredSize);
static const String& uiRegularFont(int preferredSize);
static const String& uiSemiboldFont(int preferredSize);


static String resolveFSPath(const char* filename) {
  String p1 = String("/fonts/") + filename;
  if (SPIFFS.exists(p1)) return p1;
  String p2 = String("/") + filename;
  if (SPIFFS.exists(p2)) return p2;
  return p1; // fallback
}

static String toLGFXPath(const String& fsPath) {
  // "/fonts/xxx.vlw" -> "/spiffs/fonts/xxx.vlw"
  // "/xxx.vlw"       -> "/spiffs/xxx.vlw"
  if (fsPath.startsWith("/")) return String("/spiffs") + fsPath;
  return String("/spiffs/") + fsPath;
}

static void initFontPaths() {
  // Regular
  FS_20 = resolveFSPath("test_20.vlw");
  FS_24 = resolveFSPath("test_24.vlw");
  FS_28 = resolveFSPath("test_28.vlw");

  // SemiBold
  FS_SB_20 = resolveFSPath("test_sb_20.vlw");
  FS_SB_24 = resolveFSPath("test_sb_24.vlw");
  FS_SB_28 = resolveFSPath("test_sb_28.vlw");

  // LGFX pathok
  FP_20 = toLGFXPath(FS_20);
  FP_24 = toLGFXPath(FS_24);
  FP_28 = toLGFXPath(FS_28);

  FP_SB_20 = toLGFXPath(FS_SB_20);
  FP_SB_24 = toLGFXPath(FS_SB_24);
  FP_SB_28 = toLGFXPath(FS_SB_28);

  Serial.println("[FONT] SPIFFS files:");
  Serial.printf("  24: %s (%s)\n", FS_24.c_str(), SPIFFS.exists(FS_24) ? "OK" : "MISSING");
  Serial.printf("  SB24: %s (%s)\n", FS_SB_24.c_str(), SPIFFS.exists(FS_SB_24) ? "OK" : "MISSING");
  Serial.println("[FONT] LGFX loadFont paths:");
  Serial.printf("  24: %s\n", FP_24.c_str());
  Serial.printf("  SB24: %s\n", FP_SB_24.c_str());
}

static String* pickAvailableFontPtr(
    int preferredSize,
    String& fp20, String& fp24, String& fp28,
    String& fs20, String& fs24, String& fs28) {
  struct FontChoice { int size; String* fp; String* fs; };
  FontChoice choices[] = {
    {20, &fp20, &fs20},
    {24, &fp24, &fs24},
    {28, &fp28, &fs28},
  };

  String* best = nullptr;
  int bestSize = -1;
  for (auto& c : choices) {
    if (!c.fs->length() || !SPIFFS.exists(*c.fs)) continue;
    if (c.size <= preferredSize && c.size > bestSize) {
      best = c.fp;
      bestSize = c.size;
    }
  }
  if (best) return best;

  int nextSize = 9999;
  for (auto& c : choices) {
    if (!c.fs->length() || !SPIFFS.exists(*c.fs)) continue;
    if (c.size >= preferredSize && c.size < nextSize) {
      best = c.fp;
      nextSize = c.size;
    }
  }
  if (best) return best;

  return &fp24;
}

static String* uiRegularFontPtr(int preferredSize) {
  return pickAvailableFontPtr(preferredSize, FP_20, FP_24, FP_28,
                              FS_20, FS_24, FS_28);
}

static String* uiSemiboldFontPtr(int preferredSize) {
  return pickAvailableFontPtr(preferredSize, FP_SB_20, FP_SB_24, FP_SB_28,
                              FS_SB_20, FS_SB_24, FS_SB_28);
}

static const String& uiRegularFont(int preferredSize)  { return *uiRegularFontPtr(preferredSize); }
static const String& uiSemiboldFont(int preferredSize) { return *uiSemiboldFontPtr(preferredSize); }

// ------------------ Encoding fix (UTF-8 / Latin-2) ------------------ //
// ------------------ Station list ------------------ //
Station g_stations[MAX_STATIONS];
int g_stationCount = 0;
int g_currentIndex = 0;
int g_menuIndex = 0;
static int g_menuNameY = 0; // active menu row Y (auto-layout)
static int g_menuListTop = 0;
static int g_menuListHeight = 0;
static int g_menuItemH = UI_MENU_H;
static int g_menuHeaderY = 0;
static int g_menuCounterY = 0;
static int g_menuOkY = 0;
static int g_menuIpY = 0;
static int g_menuTextH = 0;
static int g_menuGap = 0;

static void loadStationsFromSPIFFS() {
  station_loadFromSPIFFS(
    g_stations,
    MAX_STATIONS,
    g_stationCount,
    g_currentIndex,
    g_menuIndex,
    g_stationName,
    g_stationUrl
  );

  if (g_stationCount > 0) {
    saveLastStationToNVS();
    saveLastStationToSPIFFS();
  }
}

static bool saveStationsToSPIFFS() {
  return station_saveToSPIFFS(g_stations, g_stationCount);
}


// ------------------ Last station persistence (NVS) ------------------ //
static void loadLastStationFromNVS() {
  station_last_load_nvs(
    prefs,
    g_stations,
    g_stationCount,
    g_currentIndex,
    g_menuIndex,
    g_stationName,
    g_stationUrl,
    Serial
  );
}

void saveLastStationToNVS() {
  station_last_save_nvs(prefs, g_stationUrl, g_stationName);
}

// ------------------ Last station persistence (SPIFFS) ------------------ //
void saveLastStationToSPIFFS() {
  station_last_save_spiffs(g_stationUrl, g_stationName);
}
// ------------------ UI helpers ------------------ //
static void clearRect(int x, int y, int w, int h) { tft.fillRect(x, y, w, h, TFT_BLACK); }



static void splitArtistTitle(const String& in, String& artist, String& title) {
  int idx = in.indexOf(" - ");
  if (idx < 0) idx = in.indexOf(" – ");
  if (idx >= 0) { artist = in.substring(0, idx); title = in.substring(idx + 3); }
  else { artist = ""; title = in; }
  artist.trim();
  title.trim();
}

// ---- CODEC + bitrate (callback csak eltárol, UI rajzol) ----
static volatile bool g_newTitleFlag = false;
static String g_pendingTitle;

static volatile bool g_newStatusFlag = false;
String g_codec = "";          // "FLAC" / "MP3" / "OGG" / "VORBIS" / "AAC" / "OPUS"
int    g_bitrateK = 0;        // kbps (kbit/s)
String g_pendingCodec = "";
int    g_pendingBitrateK = 0;

enum UIMode { MODE_PLAY, MODE_MENU };
static UIMode g_mode = MODE_PLAY;

// ------------------ Codec ikon (60x60 RGB565) ------------------ //
// Várjuk, hogy az ikon .h fájlokban *egyedi* tömbnevek legyenek.
// Ha a Marlin konverter mindegyikben ugyanazt a nevet generálta (pl. image_data_60x60x16),
// akkor nevezd át őket ezekre:
//   image_data_aac_60x60x16, image_data_flac_60x60x16, image_data_mp3_60x60x16,
//   image_data_ogg_60x60x16, image_data_vor_60x60x16, image_data_opus_60x60x16
#ifndef CODEC_ICON_W
  #define CODEC_ICON_W 60
#endif
#ifndef CODEC_ICON_H
  #define CODEC_ICON_H 60
#endif

static const uint16_t* codecIconPtrFromCodec(const String& codec) {
  String u = codec; u.toUpperCase();
  if (u.indexOf("FLAC") >= 0) return image_data_flac_60x60x16;
  if (u.indexOf("OPUS") >= 0) return image_data_opus_60x60x16;
  if (u.indexOf("VORBIS") >= 0) return image_data_vor_60x60x16;
  if (u.indexOf("OGG") >= 0) return image_data_ogg_60x60x16;
  if (u.indexOf("AAC") >= 0) return image_data_aac_60x60x16;
  if (u.indexOf("MP3") >= 0) return image_data_mp3_60x60x16;
  return nullptr;
}

static void drawCodecIconTopLeft() {
  if (g_mode != MODE_PLAY || ui_stationSelectorActive()) return;
  // Bal felső sarok: 4,4
  const int x = 4;
  const int y = 4;
  clearRect(x, y, CODEC_ICON_W, CODEC_ICON_H);

  const uint16_t* img = codecIconPtrFromCodec(g_codec);
  if (!img) return;

  // LovyanGFX: RGB565 képtömb kirajzolás
  tft.pushImage(x, y, CODEC_ICON_W, CODEC_ICON_H, img);
}

// ---- Audio format (channels / sample rate / bit depth) ----
int g_ch = 0;                 // 1=mono, 2=stereo, ...
int g_sampleRate = 0;         // Hz (pl. 44100)
int g_bitsPerSample = 0;      // bit (pl. 16)
int g_pendingCh = 0;
int g_pendingSampleRate = 0;
int g_pendingBitsPerSample = 0;

// ------------------ Buffer kijelző ------------------ //
size_t g_bufferFilled = 0;
size_t g_bufferFree = 0;
size_t g_bufferTotal = 0;
int g_bufferPercent = 0;

// UI-callback wrappers (input_rotary / net_server expects void() callbacks)
// Keep these thin wrappers in app_impl to avoid modifying callback signatures.
static void updateVolumeOnly() {
  if (g_mode != MODE_PLAY || ui_stationSelectorActive()) return;
  ui_drawBottomBar(g_Volume, g_bufferPercent, (WiFi.status() == WL_CONNECTED));
}

static void updateBufferIndicatorOnly() {
  if (g_mode != MODE_PLAY || ui_stationSelectorActive()) return;
  ui_updateBufferIndicatorOnly(g_bufferPercent);
}
static uint32_t lastBufferCheck = 0;

// ------------------ Layout ------------------ //
static int W, H;
static bool g_uiReady = false;
static int yHeader, yStationLabel, yStationName, yStreamLabel, yArtist, yTitle, yVol;
static int hStationLine, hArtistLine, hTitleLine;

static int wifiX, wifiY, wifiW, wifiH;
static uint32_t lastWifiDraw = 0;
static const uint32_t WIFI_DRAW_MS = 2000;

static const uint32_t MARQUEE_MS = 80;   // marquee tick (smaller=faster)
static const int32_t  SCROLL_STEP = 3;    // pixels per tick
static const int32_t  SCROLL_GAP  = 40;   // blank gap between repeats
static const uint32_t MENU_MS    = 60;

String g_artist = "";
String g_title  = "";


// Marquee "content" (to show: text end * text start)
static const char* MARQ_SEP = " * ";
static String g_mStation = "";
static String g_mArtist  = "";
static String g_mTitle   = "";
static int g_wStationMarq = 0, g_wArtistMarq = 0, g_wTitleMarq = 0;

// ---- UI render cache (to avoid unnecessary redraws) ----
static String g_lastStationDrawn = "";
static String g_lastArtistDrawn  = "";
static String g_lastTitleDrawn   = "";
static int32_t g_lastStationX    = INT32_MIN;
static int32_t g_lastArtistX     = INT32_MIN;
static int32_t g_lastTitleX      = INT32_MIN;
static bool g_forceRedrawText    = true;
static String g_id3Artist = "";
static String g_id3Title  = "";

static void reserveHotStrings() {
  // Csak kapacitást foglalunk előre, működést nem változtatunk.
  // Cél: kevesebb heap-fragmentáció hosszabb üzem alatt.
  g_stationName.reserve(128);
  g_stationUrl.reserve(320);
  g_playlistSourceUrl.reserve(320);
  g_playUrl.reserve(320);
  g_lastConnectUrl.reserve(320);
  g_uploadPath.reserve(128);
  g_pendingTitle.reserve(256);
  g_codec.reserve(16);
  g_pendingCodec.reserve(16);
  g_artist.reserve(160);
  g_title.reserve(160);
  g_mStation.reserve(256);
  g_mArtist.reserve(256);
  g_mTitle.reserve(256);
  g_lastStationDrawn.reserve(256);
  g_lastArtistDrawn.reserve(256);
  g_lastTitleDrawn.reserve(256);
  g_id3Artist.reserve(160);
  g_id3Title.reserve(160);
}

static void logMemorySnapshot(const char* tag) {
  serialLogf("[MEM] %s | heap free=%u, min=%u, max=%u",
             tag,
             (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(),
             (unsigned)ESP.getMaxAllocHeap());

  if (psramFound()) {
    serialLogf(" | PSRAM free=%u, size=%u",
               (unsigned)ESP.getFreePsram(),
               (unsigned)ESP.getPsramSize());
  } else {
    serialLogf(" | PSRAM: not found");
  }

  serialLogln("");
}
static uint32_t g_id3SeenAt = 0;
static int32_t xStation = 0, xArtist = 0, xTitle = 0;
static uint32_t lastMarquee = 0;

static uint32_t trackChangedAt = 0;
static const uint32_t HOLD_MS = 1000;
static bool holdPhase = false;

// ---- Cached text metrics (avoid calling textWidth() repeatedly) ----
static int g_wStation = 0, g_wArtist = 0, g_wTitle = 0;
static bool g_scrollStation = false, g_scrollArtist = false, g_scrollTitle = false;
static bool g_anyScrollActive = false;
static int32_t g_centerXStation = 0, g_centerXArtist = 0, g_centerXTitle = 0;

static void recalcTextMetrics() {
  if (!g_uiReady) return;

  // Base widths (used for centering when NOT scrolling)
  g_wStation = sprStation.textWidth(g_stationName.c_str());
  g_wArtist  = sprArtist.textWidth(g_artist.c_str());
  g_wTitle   = sprTitle.textWidth(g_title.c_str());

  g_scrollStation = (g_stationName.length() && g_wStation > (int)sprStation.width());
  g_scrollArtist  = (g_artist.length()      && g_wArtist  > (int)sprArtist.width());
  g_scrollTitle   = (g_title.length()       && g_wTitle   > (int)sprTitle.width());
  g_anyScrollActive = g_scrollStation || g_scrollArtist || g_scrollTitle;

  // Build marquee strings so the loop becomes: "...text end * text start..."
  // We only append the separator when a line actually scrolls.
  g_mStation = g_scrollStation ? (g_stationName + MARQ_SEP) : g_stationName;
  g_mArtist  = g_scrollArtist  ? (g_artist      + MARQ_SEP) : g_artist;
  g_mTitle   = g_scrollTitle   ? (g_title       + MARQ_SEP) : g_title;

  // Marquee widths (used for seamless wrap)
  g_wStationMarq = g_scrollStation ? sprStation.textWidth(g_mStation.c_str()) : g_wStation;
  g_wArtistMarq  = g_scrollArtist  ? sprArtist .textWidth(g_mArtist .c_str()) : g_wArtist;
  g_wTitleMarq   = g_scrollTitle   ? sprTitle  .textWidth(g_mTitle  .c_str()) : g_wTitle;

  g_centerXStation = (!g_scrollStation && g_stationName.length() && g_wStation <= (int)sprStation.width()) ? (((int)sprStation.width() - g_wStation) / 2) : 0;
  g_centerXArtist  = (!g_scrollArtist  && g_artist.length()      && g_wArtist  <= (int)sprArtist.width()) ? (((int)sprArtist.width() - g_wArtist) / 2) : 0;
  g_centerXTitle   = (!g_scrollTitle   && g_title.length()       && g_wTitle   <= (int)sprTitle.width()) ? (((int)sprTitle.width() - g_wTitle) / 2) : 0;
}

static bool menuScroll = false;
static int32_t xMenu = 0;
static uint32_t lastMenuTick = 0;

static String menuClipText(const String& s, int maxW) {
  if (maxW <= 0) return "";
  if (tft.textWidth(s.c_str()) <= maxW) return s;

  const char* dots = "...";
  const int dotsW = tft.textWidth(dots);
  if (dotsW >= maxW) return "";

  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); ++i) {
    String trial = out + s[i];
    if (tft.textWidth((trial + dots).c_str()) > maxW) break;
    out += s[i];
  }
  return out + dots;
}

static int menuVisibleRows() {
  if (g_menuItemH <= 0) return 1;
  int rows = g_menuListHeight / g_menuItemH;
  if (rows < 1) rows = 1;
  if (rows > 7) rows = 7;
  if ((rows % 2) == 0) rows -= 1;
  if (rows < 1) rows = 1;
  return rows;
}

static void drawMenuListArea();

static int textWidthMain(const String& s) { tft.loadFont(uiRegularFont(UI_FONT_TITLE).c_str()); return tft.textWidth(s.c_str()); }

static void recomputeLayout() {
  W = tft.width();
  H = tft.height();

  // Header font
  tft.loadFont(uiRegularFont(UI_FONT_HEADER).c_str());
  int hHeader = tft.fontHeight();
  yHeader = UI_HEADER_Y;

  // Small label font
  tft.loadFont(uiRegularFont(UI_FONT_LABEL).c_str());
  int hLabel = tft.fontHeight();

  // Stream line font
  tft.loadFont(uiRegularFont(UI_FONT_STREAM).c_str());
  int hStream = tft.fontHeight();

  // Main text fonts / sprite heights
  tft.loadFont(uiRegularFont(UI_FONT_ARTIST).c_str());
  int hArtistText = tft.fontHeight();
  hArtistLine = hArtistText + UI_TEXT_LINE_EXTRA;

  tft.loadFont(uiRegularFont(UI_FONT_TITLE).c_str());
  int hTitleText = tft.fontHeight();
  hTitleLine  = hTitleText + UI_TEXT_LINE_EXTRA;

  tft.loadFont(uiSemiboldFont(UI_FONT_STATION).c_str());
  int hStationText = tft.fontHeight();
  hStationLine = hStationText + UI_TEXT_LINE_EXTRA;

  // Layout positions
  yStationLabel = yHeader + hHeader + UI_GAP_AFTER_HEADER;
  yStationName  = yStationLabel + hLabel + UI_GAP_LABEL_TO_TEXT + UI_LABEL_TEXT_OFFSET;

  yStreamLabel  = yStationName + hStationLine + UI_GAP_AFTER_STATION_LINE;

  yArtist = yStreamLabel + hLabel + UI_GAP_STREAMLABEL_TO_TEXT + UI_LABEL_TEXT_OFFSET + UI_ARTIST_TITLE_Y_SHIFT;
  yTitle  = yArtist + hArtistLine + UI_GAP_ARTIST_TO_TITLE;

  yVol = H - hStream - UI_MARGIN_BOTTOM;

  wifiW = UI_WIFI_W; wifiH = UI_WIFI_H;
  wifiX = W - wifiW - UI_WIFI_MARGIN;
  wifiY = H - wifiH - UI_WIFI_MARGIN;
}

//
// ------------------ Sprites ------------------ //

static void initSprites() {
  const bool havePsram = psramFound();

  sprStation.setColorDepth(16);
  if (havePsram) sprStation.setPsram(true);
  sprStation.createSprite(W, hStationLine);
  sprStation.fillScreen(TFT_BLACK);
  sprStation.loadFont(uiSemiboldFont(UI_FONT_STATION).c_str());
  sprStation.setTextWrap(false);
  sprStation.setTextColor(TFT_ORANGE, TFT_BLACK); // ÁLLOMÁS SZÍNE

  sprArtist.setColorDepth(16);
  if (havePsram) sprArtist.setPsram(true);
  sprArtist.createSprite(W, hArtistLine);
  sprArtist.fillScreen(TFT_BLACK);
  sprArtist.loadFont(uiSemiboldFont(UI_FONT_ARTIST).c_str());
  sprArtist.setTextWrap(false);
  sprArtist.setTextColor(TFT_CYAN, TFT_BLACK); // ELŐADÓ SZÍNE

  sprTitle.setColorDepth(16);
  if (havePsram) sprTitle.setPsram(true);
  sprTitle.createSprite(W, hTitleLine);
  sprTitle.fillScreen(TFT_BLACK);
  sprTitle.loadFont(uiSemiboldFont(UI_FONT_TITLE).c_str());
  sprTitle.setTextWrap(false);
  sprTitle.setTextColor(TFT_SILVER, TFT_BLACK); // DAL CÍM SZÍNE

  sprMenu.setColorDepth(16);
  if (havePsram) sprMenu.setPsram(true);
  sprMenu.createSprite(W, UI_MENU_H);
  sprMenu.fillScreen(TFT_BLACK);
  sprMenu.loadFont(uiSemiboldFont(UI_FONT_MENU).c_str());
  sprMenu.setTextWrap(false);
  sprMenu.setTextColor(TFT_WHITE, TFT_BLACK);

  g_uiReady = true;
}

// ------------------ WiFi icon ------------------ //
// (kiszervezve ui_display.*-be)


// ------------------ Bitrate formázás: 320k / 1.4M ------------------ //
static String formatRate(int kbps) {
  if (kbps <= 0) return "";
  if (kbps < 1000) return String(kbps) + "k";
  int tenths = (kbps + 50) / 100;   // 0.1M egység, kerekítve
  int whole  = tenths / 10;
  int dec    = tenths % 10;
  return String(whole) + "." + String(dec) + "M";
}

static String formatAudioInfoLine() {
  // Cél formátum: "2ch | 44KHz | 16bit"
  String chStr  = (g_ch > 0) ? (String(g_ch) + "ch") : String("--ch");
  String srStr;
  if (g_sampleRate > 0) {
    int khz = (g_sampleRate + 500) / 1000; // 44100 -> 44
    srStr = String(khz) + "KHz";
  } else {
    srStr = String("--KHz");
  }
  String bitStr = (g_bitsPerSample > 0) ? (String(g_bitsPerSample) + "bit") : String("--bit");
  return chStr + " | " + srStr + " | " + bitStr;
}


// ------------------ Buffer kijelző rajzolása ------------------ //
// (kiszervezve stream_watchdog.*-ba)


// ------------------ Bottom bar: Volume + buffer + WiFi ------------------ //
void drawBottomBar() {
  if (g_mode != MODE_PLAY || ui_stationSelectorActive()) return;
  // Buffer % frissítés (500ms-enként), a tényleges rajzolás a ui_display modulban van
  clearRect(wifiX - 80, yVol - 20, 120, 40);

  StreamWatchdogBufferCtx bctx{};
  bctx.stationUrl = &g_stationUrl;
  bctx.lastBufferCheckMs = &lastBufferCheck;
  bctx.bufferFilled = &g_bufferFilled;
  bctx.bufferFree = &g_bufferFree;
  bctx.bufferTotal = &g_bufferTotal;
  bctx.bufferPercent = &g_bufferPercent;
  bctx.readBufferFilledFn = []() -> size_t { return audio.inBufferFilled(); };
  bctx.readBufferFreeFn = []() -> size_t { return audio.inBufferFree(); };
  bctx.refreshMs = 500;
  stream_watchdog_updateBuffer(bctx);

  ui_drawBottomBar(g_Volume, g_bufferPercent, (WiFi.status() == WL_CONNECTED));
}


// ------------------ UI ------------------ //
static void drawStreamLabelLine() {
  tft.loadFont(uiRegularFont(UI_FONT_STREAM).c_str()); // display-profile stream font
  int th = tft.fontHeight();
  int lineY = yStreamLabel - ((W <= 320) ? 8 : 0);
  int lineH = th + 2;

  // teljes sor frissítés (villogás nélkül)
  clearRect(0, lineY - 4, W, lineH + 8);

  // "Stream: 2ch | 44KHz | 16bit | 320k" középre igazítva
  String line = text_fix(lang::ui_stream_prefix);
  line += formatAudioInfoLine();
  if (g_bitrateK > 0) {
    line += " | " + formatRate(g_bitrateK);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int twLine = tft.textWidth(line.c_str());
  int x = (twLine <= W) ? (W - twLine) / 2 : 0;
  tft.setCursor(x, lineY);
  tft.print(line);

  // Paused badge (jobb oldalon), ha kell
  if (!g_paused) return;

  String txt = text_fix(lang::ui_paused);
  int tw = tft.textWidth(txt.c_str());

  int padX = 10;
  int badgeH = lineH;

  int padY = (badgeH - th) / 2;
  if (padY < 1) padY = 1;

  int bw = tw + padX * 2;
  int bh = badgeH - 1;

  int bx = W - bw - 6;
  int by = lineY;

  tft.fillRoundRect(bx, by, bw, bh, 6, TFT_ORANGE); // orange háttér

  int innerY = by + 1;
  int innerH = bh - 2;
  int textY = innerY + (innerH - th) / 2;
  if (textY < innerY) textY = innerY;

  tft.setTextColor(TFT_BLACK, TFT_ORANGE);  // orange háttér, black szöveg
  tft.setCursor(bx + padX, textY);
  tft.print(txt);

  tft.drawRoundRect(bx, by, bw, bh, 6, TFT_YELLOW);
  tft.drawFastHLine(bx + 6, by + bh - 1, bw - 12, TFT_YELLOW);
}

static AudioControlCtx makeAudioControlCtx() {
  AudioControlCtx ctx{};
  ctx.paused = &g_paused;
  ctx.volume = &g_Volume;
  ctx.playUrl = &g_playUrl;
  ctx.stationUrl = &g_stationUrl;
  ctx.needStreamReconnect = &g_needStreamReconnect;
  ctx.sendStop = stream_core_sendStop;
  ctx.sendVolume = stream_core_sendVolume;
  ctx.sendConnect = stream_core_sendConnect;
  ctx.drawStreamLabelFn = drawStreamLabelLine;
  ctx.startPlaybackCurrentFn = startPlaybackCurrent;
  ctx.logf = serialLogf;
  return ctx;
}

static void setPaused(bool paused) {
  AudioControlCtx ctx = makeAudioControlCtx();
  audio_control_setPaused(ctx, paused);
}

void togglePaused() {
  AudioControlCtx ctx = makeAudioControlCtx();
  audio_control_togglePaused(ctx);
}

// ---------- M3U playlist helpers ----------
bool startPlaybackCurrent(bool allowReloadPlaylist) {
  return playlist_runtime_startPlaybackCurrent(g_playlistMetaCtx, allowReloadPlaylist);
}

static bool advancePlaylistAndPlay() {
  return playlist_runtime_advancePlaylistAndPlay(g_playlistMetaCtx);
}

void updateStationNameUI() {
  // Ensure widths/centering are up to date for the current text
  recalcTextMetrics();
  // Draw via sprite (font already loaded), and mark caches so marquee won't immediately redraw again.
  int32_t xS = (!g_scrollStation && g_stationName.length()) ? g_centerXStation : 0;
  // If it's wider than the sprite, start at 0 (marquee handles scrolling elsewhere).
  if (sprStation.textWidth(g_stationName.c_str()) > (int)sprStation.width()) xS = 0;
  renderLine(sprStation, g_stationName, xS);
  sprStation.pushSprite(0, yStationName);

  g_lastStationDrawn = g_stationName;
  g_lastStationX = xS;
}

static int spriteTextYOffset(const LGFX_Sprite& spr) {
  return (&spr == &sprStation) ? 1 : 0;
}

static void renderLine(LGFX_Sprite& spr, const String& text, int32_t x) {
  spr.fillScreen(TFT_BLACK);
  spr.setCursor(x, spriteTextYOffset(spr));
  spr.print(text);
}

// Seamless marquee: draw the same (already "text + sep") string twice back-to-back.
// This makes the loop naturally become: "...end * start..." without a dead gap.
static void renderMarqueeLine(LGFX_Sprite& spr, const String& marqueeText, int32_t x, int wMarq) {
  spr.fillScreen(TFT_BLACK);
  const int y = spriteTextYOffset(spr);

  // First copy
  spr.setCursor(x, y);
  spr.print(marqueeText);

  // Second copy immediately after the first (so there is always content coming in)
  spr.setCursor(x + wMarq, y);
  spr.print(marqueeText);
}

static int32_t calcCenterX(LGFX_Sprite& spr, const String& s) {
  // Fallback centering based on sprite width and textWidth.
  int w = spr.textWidth(s.c_str());
  int sw = (int)spr.width();
  int32_t x = (w <= sw) ? ((sw - w) / 2) : 0;
  return (x < 0) ? 0 : x;
}


static void drawStaticUI() {
  tft.fillScreen(TFT_BLACK);

  tft.loadFont(uiRegularFont(UI_FONT_HEADER).c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  String header = text_fix("myRadio");

// Codec ikon bal felső sarok
  drawCodecIconTopLeft();

  // Fejléc + jobb felső LOGO (UI modul)
  ui_drawHeaderAndLogo(header, yHeader, CODEC_ICON_W, LOGO_W);
  tft.loadFont(uiRegularFont(UI_FONT_LABEL).c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
int wS = sprStation.textWidth(g_stationName.c_str());
  int xS = (wS <= (int)sprStation.width()) ? (((int)sprStation.width() - wS) / 2) : 0;
  renderLine(sprStation, g_stationName, xS);
  sprStation.pushSprite(0, yStationName);

  drawStreamLabelLine();

  clearRect(0, yArtist, W, hArtistLine);
  clearRect(0, yTitle,  W, hTitleLine);
  sprArtist.pushSprite(0, yArtist);
  sprTitle.pushSprite(0, yTitle);

  drawBottomBar();
}

static void updateMarquee() {
  const uint32_t now = millis();
  static String mStation, mArtist, mTitle;
  if (g_forceRedrawText || mStation != g_stationName || mArtist != g_artist || mTitle != g_title) {
    recalcTextMetrics();
    mStation = g_stationName;
    mArtist  = g_artist;
    mTitle   = g_title;
  }


  // If we are in hold phase, only (re)draw when something actually changed.
  if (holdPhase) {
    if (now - trackChangedAt >= HOLD_MS) {
      holdPhase = false;
      lastMarquee = now;
      // Force one redraw right after hold ends so scrolling can start cleanly if needed
      g_forceRedrawText = true;
    }

    // Centered draw (no scrolling during hold). Only redraw when text changed.
    const int xS = g_centerXStation;
    const int xA = g_centerXArtist;
    const int xT = g_centerXTitle;

    if (g_forceRedrawText || g_lastStationDrawn != g_stationName || g_lastStationX != xS) {
      // During hold phase we do not advance the X position, but we still
      // want to render the marquee content (with separator) if this line
      // normally scrolls.
      if (g_scrollStation) renderMarqueeLine(sprStation, g_mStation, xS, g_wStationMarq);
      else                 renderLine(sprStation, g_stationName, xS);
      sprStation.pushSprite(0, yStationName);
      g_lastStationDrawn = g_stationName;
      g_lastStationX = xS;
    }
    if (g_forceRedrawText || g_lastArtistDrawn != g_artist || g_lastArtistX != xA) {
      if (g_scrollArtist) renderMarqueeLine(sprArtist, g_mArtist, xA, g_wArtistMarq);
      else                renderLine(sprArtist, g_artist, xA);
      sprArtist.pushSprite(0, yArtist);
      g_lastArtistDrawn = g_artist;
      g_lastArtistX = xA;
    }
    if (g_forceRedrawText || g_lastTitleDrawn != g_title || g_lastTitleX != xT) {
      if (g_scrollTitle) renderMarqueeLine(sprTitle, g_mTitle, xT, g_wTitleMarq);
      else               renderLine(sprTitle, g_title, xT);
      sprTitle.pushSprite(0, yTitle);
      g_lastTitleDrawn = g_title;
      g_lastTitleX = xT;
    }

    g_forceRedrawText = false;
    return;
  }

  // Throttle marquee tick
  if (now - lastMarquee < MARQUEE_MS) return;
  lastMarquee = now;

  // Cached widths/scroll flags (computed on text/layout changes)
  const int wS = g_wStation;
  const int wA = g_wArtist;
  const int wT = g_wTitle;


  // Marquee widths (base + separator). Used only when scrolling.
  const int wSM = g_wStationMarq;
  const int wAM = g_wArtistMarq;
  const int wTM = g_wTitleMarq;

  const bool scrollS = g_scrollStation;
  const bool scrollA = g_scrollArtist;
  const bool scrollT = g_scrollTitle;

  // If nothing scrolls AND nothing changed, don't redraw at all.
  if (!g_forceRedrawText &&
      !scrollS && !scrollA && !scrollT &&
      g_lastStationDrawn == g_stationName &&
      g_lastArtistDrawn  == g_artist &&
      g_lastTitleDrawn   == g_title) {
    return;
  }

  // Station
  int32_t xS = 0;
  if (!g_stationName.length()) {
    xS = 0;
  } else if (!scrollS) {
    xS = g_centerXStation;
  } else {
    xStation -= SCROLL_STEP;
    // Seamless wrap (we draw two copies back-to-back)
    if (xStation <= -wSM) xStation += wSM;
    xS = xStation;
  }
  if (g_forceRedrawText || scrollS || g_lastStationDrawn != g_stationName || g_lastStationX != xS) {
    if (scrollS) renderMarqueeLine(sprStation, g_mStation, xS, wSM);
    else         renderLine(sprStation, g_stationName, xS);
    sprStation.pushSprite(0, yStationName);
    g_lastStationDrawn = g_stationName;
    g_lastStationX = xS;
  }

  // Artist
  int32_t xA = 0;
  if (!g_artist.length()) {
    xA = 0;
  } else if (!scrollA) {
    xA = g_centerXArtist;
  } else {
    xArtist -= SCROLL_STEP;
    // Seamless wrap (we draw two copies back-to-back)
    if (xArtist <= -wAM) xArtist += wAM;
    xA = xArtist;
  }
  if (g_forceRedrawText || scrollA || g_lastArtistDrawn != g_artist || g_lastArtistX != xA) {
    if (scrollA) renderMarqueeLine(sprArtist, g_mArtist, xA, wAM);
    else         renderLine(sprArtist, g_artist, xA);
    sprArtist.pushSprite(0, yArtist);
    g_lastArtistDrawn = g_artist;
    g_lastArtistX = xA;
  }

  // Title
  int32_t xT = 0;
  if (!g_title.length()) {
    xT = 0;
  } else if (!scrollT) {
    xT = g_centerXTitle;
  } else {
    xTitle -= SCROLL_STEP;
    // Seamless wrap (we draw two copies back-to-back)
    if (xTitle <= -wTM) xTitle += wTM;
    xT = xTitle;
  }
  if (g_forceRedrawText || scrollT || g_lastTitleDrawn != g_title || g_lastTitleX != xT) {
    if (scrollT) renderMarqueeLine(sprTitle, g_mTitle, xT, wTM);
    else         renderLine(sprTitle, g_title, xT);
    sprTitle.pushSprite(0, yTitle);
    g_lastTitleDrawn = g_title;
    g_lastTitleX = xT;
  }

  g_forceRedrawText = false;
}

// ------------------ Menu ------------------ //
static void drawMenuListArea() {
  ui_stationSelectorDraw();
}

static String clipTextKeepRight(LGFX_Device* dev, const String& s, int maxW) {
  if (!dev || maxW <= 0) return "";
  if (dev->textWidth(s.c_str()) <= maxW) return s;

  const char* dots = "...";
  const int dotsW = dev->textWidth(dots);
  if (dotsW >= maxW) return "";

  String tail;
  tail.reserve(s.length());
  for (int i = (int)s.length() - 1; i >= 0; --i) {
    String trial = String(s[i]) + tail;
    if (dev->textWidth((String(dots) + trial).c_str()) > maxW) break;
    tail = trial;
  }
  return String(dots) + tail;
}

static void redrawMenuCounterAndList() {
  if (g_mode != MODE_MENU) return;

  tft.loadFont(uiRegularFont(UI_FONT_LABEL).c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  const int sidePad = 0;
  if (W <= 320 && H <= 240) {
    String counterText = (g_stationCount > 0)
                           ? String(g_menuIndex + 1) + " / " + String(g_stationCount)
                           : String(lang::ui_no_list);
    const int counterW = tft.textWidth(counterText);
    const int counterX = W - counterW;
    if (g_menuTextH > 0) {
      tft.fillRect(counterX, g_menuHeaderY, counterW, g_menuTextH, TFT_BLACK);
    }
    tft.setCursor(counterX, g_menuHeaderY);
    tft.print(counterText);
  } else {
    if (g_menuTextH > 0) {
      tft.fillRect(0, g_menuCounterY, W, g_menuTextH, TFT_BLACK);
    }

    tft.setCursor(sidePad, g_menuCounterY);
    if (g_stationCount > 0) tft.printf("%d / %d", (g_menuIndex + 1), g_stationCount);
    else tft.print(lang::ui_no_list_stations);
  }

  drawMenuListArea();
}

static void drawMenuScreen() {
  tft.loadFont(uiRegularFont(UI_FONT_LABEL).c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  const int h = tft.fontHeight();
  const int gap = (H >= 300) ? 8 : 4;
  const int sidePad = 0;
  const int menuTop = 0;
  const int menuBottom = tft.height();

  clearRect(0, menuTop, W, (menuBottom - menuTop));

  int yHeader  = gap;
  int yCounter = yHeader + h + gap;
  int yIp      = menuBottom - h - gap;
  int yOk      = yIp - h - gap;

  g_menuListTop = yCounter + h + gap;
  g_menuListHeight = yOk - gap - g_menuListTop;
  if (g_menuListHeight < 5 * 28) g_menuListHeight = 5 * 28;

  const int targetRows = 5;
  g_menuItemH = g_menuListHeight / targetRows;
  if (W <= 320 && H <= 240) {
    // 320x240-en fix 28 px-es sormagasság kell: így az 5 sor éppen elfér,
    // és az utolsó elem nem csúszik rá az "OK" sorra.
    g_menuItemH = 28;
  } else {
    if (g_menuItemH < 30) g_menuItemH = 30;
  }
  g_menuListHeight = g_menuItemH * targetRows;

  const int totalMenuBlock = (yCounter - yHeader) + h + gap + g_menuListHeight + gap + h + gap + h;
  if (totalMenuBlock < H) {
    const int extra = (H - totalMenuBlock) / 2;
    yHeader += extra;
    yCounter += extra;
    g_menuListTop += extra;
    yOk += extra;
    yIp += extra;
  }

  // 320x240-en itt már nem kell külön nudge: a fix 28 px-es sormagasság
  // pontosan elég helyet ad az 5. sornak az "OK" sor fölött.

  g_menuHeaderY = yHeader;
  g_menuCounterY = yCounter;
  g_menuOkY = yOk;
  g_menuIpY = yIp;
  g_menuTextH = h;
  g_menuGap = gap;

  tft.setCursor(sidePad, yHeader);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(lang::ui_select_station);

  redrawMenuCounterAndList();

  tft.loadFont(uiRegularFont(UI_FONT_LABEL).c_str());
  tft.setCursor(sidePad, yOk);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.print(lang::ui_ok_exit_hint);

  tft.loadFont(uiRegularFont(UI_FONT_LABEL).c_str());
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    const String ipText = String(lang::ui_ip_prefix) + WiFi.localIP().toString();

    String activeSsid = wifi_manager_active_ssid();
    if (!activeSsid.length()) activeSsid = WiFi.SSID();
    String ssidText = activeSsid.length() ? (String("SSID: ") + activeSsid) : String();

    const int rightPad = (W <= 320) ? 0 : 2;
    const int minGap = (W <= 320) ? 8 : 14;
    const int ipW = tft.textWidth(ipText);
    const int ssidRightEdge = W - rightPad;
    const int maxSsidW = ssidText.length() ? (ssidRightEdge - (ipW + minGap)) : 0;

    tft.setCursor(sidePad, yIp);
    tft.print(ipText);

    if (ssidText.length() && maxSsidW > 24) {
      ssidText = clipTextKeepRight(&tft, ssidText, maxSsidW);
      if (ssidText.length()) {
        tft.setTextDatum(top_right);
        tft.drawString(ssidText, ssidRightEdge, yIp);
        tft.setTextDatum(top_left);
      }
    }
  } else {
    tft.setCursor(sidePad, yIp);
    tft.print(lang::ui_no_wifi_ip);
  }
}

static void updateMenuNameScroll() {
  ui_stationSelectorTick();
}

static void exitMenuRedrawPlayUI() {
  ui_stationSelectorEnd();
  g_mode = MODE_PLAY;

  tft.loadFont(uiRegularFont(UI_FONT_LABEL).c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // A station selector most teljes képernyős, ezért visszalépéskor a TELJES UI-t újra kell rajzolni,
  // különben menü-maradványok látszanak lejátszás közben.
  clearRect(0, 0, W, H);
  drawStaticUI();

  updateMarquee();

  // A teljes redraw után a VU statikus keretét is újra kell építeni.
  ui_invalidateVuMeter();
  ui_drawVuMeter(vu_getL(), vu_getR(), vu_getPeakL(), vu_getPeakR());
}

bool app_isMenuMode() {
  return g_mode == MODE_MENU;
}

void app_exitMenuRedrawPlayUI() {
  exitMenuRedrawPlayUI();
}

// ------------------ Codec/bitrate parse helpers ------------------ //
void my_audio_info(Audio::msg_t m) {
  playlist_meta_handleAudioInfo(g_playlistMetaCtx, m);
  
}

// ------------------ Button ------------------ //
static InputButtonState g_buttonState;
static const uint32_t LONG_MS = 650;

static void onButtonLongPress() {
  if (g_mode == MODE_PLAY) {
    g_mode = MODE_MENU;
    ui_stationSelectorBegin(g_currentIndex);
    ui_invalidateVuMeter();
    drawMenuScreen();
  } else {
    exitMenuRedrawPlayUI();
  }
}

static void onButtonShortPress() {
  if (g_mode == MODE_PLAY) {
    togglePaused();
  } else if (g_mode == MODE_MENU) {
    if (g_stationCount > 0) {
      g_currentIndex = ui_stationSelectorSelected();
      g_stationName = g_stations[g_currentIndex].name;
      g_stationUrl  = g_stations[g_currentIndex].url;

      // állomásváltáskor töröljük a régi meta infót (ha az új nem küld, ne maradjon a régi)
      g_artist = "";
      g_title  = "";
      g_pendingTitle = "";
      g_newTitleFlag = false;
      xArtist = 0;
      xTitle  = 0;
      holdPhase = false;
      trackChangedAt = millis();

      g_forceRedrawText = true;
      tft.loadFont(uiRegularFont(UI_FONT_LABEL).c_str());
      renderLine(sprArtist, "", 0);
      renderLine(sprTitle,  "", 0);
      sprArtist.pushSprite(0, yArtist);
      sprTitle.pushSprite(0, yTitle);

      updateStationNameUI();

      saveLastStationToNVS();
      saveLastStationToSPIFFS();

      startPlaybackCurrent(true);
      if (g_playUrl.length()) stream_core_sendConnect(g_playUrl);

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

      exitMenuRedrawPlayUI();
    } else {
      exitMenuRedrawPlayUI();
    }
  }
}

// ------------------ WiFi callbacks ------------------ //

static void onWiFiRestored() {
  stream_watchdog_markWifiRestored(g_needStreamReconnect, &Serial);
}

// ------------------ Web Server Functions ------------------ //

static void startWebServer() {
  const HttpApiHandlers handlers = {
    handleRoot,
    handleSearch,
    handleGetStations,
    handleAddStation,
    handleDeleteStation,
    handleUpdateStation,
    handleMoveStation,
    handleSetStation,
    handleSetVolume,
    handleGetBrightness,
    handleSetBrightness,
    handleTogglePause,
    handleGetStatus,
    handleNextStation,
    handleTrackNext,
    handleTrackPrev,
    handlePrevStation,
    handleGetBuffer,
    handleReset,
    handleUploadPage,
    handleUploadDone,
    handleFileUpload,
    handleFsList
  };

  http_api_register_routes(server, handlers);
}

// ---------- STARTUP ANIM BOX (PRO) ----------
void drawStartupScreen(uint8_t phase){
  tft.fillScreen(TFT_BLACK);

  const int pad = 8;
  const int w   = tft.width()  - pad * 2;
  const int h   = tft.height() - pad * 2;

  tft.drawRoundRect(pad, pad, w, h, 12, TFT_WHITE);

  const bool have20 = (FS_20.length() && FS_SB_20.length() && SPIFFS.exists(FS_20) && SPIFFS.exists(FS_SB_20));
  const bool haveSB24 = (FS_SB_24.length() && SPIFFS.exists(FS_SB_24));

  const String& fontBody  = have20 ? FP_20    : FP_24;
  const String& fontTitle = haveSB24 ? FP_SB_24 : FP_24;

  const String title = lang::boot_starting_radio;
  const bool connectedNow = (WiFi.status() == WL_CONNECTED);
  String base  = connectedNow ? lang::boot_wifi_connected : lang::boot_wifi_connecting;

  String ssidLine = lang::boot_searching_network;
  String infoLine = "";

  if (connectedNow) {
    String activeSsid = wifi_manager_active_ssid();
    if (!activeSsid.length()) activeSsid = WiFi.SSID();
    if (activeSsid.length()) ssidLine = activeSsid;
    const int rssi = wifi_manager_active_rssi_dbm();
    if (rssi != 0) infoLine = String(rssi) + " dBm";
  } else {
    String attemptSsid = wifi_manager_current_attempt_ssid();
    if (attemptSsid.length()) {
      if (g_startupAttemptTotal > 0) {
        ssidLine = String(lang::boot_ssid_attempt_prefix) + String(g_startupAttemptIndex) + "/" + String(g_startupAttemptTotal) + ": " + attemptSsid;
      } else {
        ssidLine = String(lang::boot_ssid_prefix) + attemptSsid;
      }
    }
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.loadFont(fontTitle.c_str());
  const int hTitle = tft.fontHeight();
  const int wTitle = tft.textWidth(title);

  tft.loadFont(fontBody.c_str());
  const int hBody  = tft.fontHeight();
  const int wBase  = tft.textWidth(base);
  const int wDots  = connectedNow ? 0 : tft.textWidth("...");
  const int hSsid  = hBody;
  const int wSsid  = tft.textWidth(ssidLine);
  const int hInfo  = infoLine.length() ? hBody : 0;
  const int wInfo  = infoLine.length() ? tft.textWidth(infoLine) : 0;

  const int gap1 = 8;
  const int gap2 = 10;
  const int gap3 = infoLine.length() ? 6 : 0;
  const int totalH = hTitle + gap1 + hBody + gap2 + hSsid + (infoLine.length() ? (gap3 + hInfo) : 0);

  const int yTitle = (tft.height() - totalH) / 2;
  const int yBody  = yTitle + hTitle + gap1;
  const int ySsid  = yBody + hBody + gap2;
  const int yInfo  = ySsid + hSsid + gap3;

  const int xBody = (tft.width() - (wBase + wDots)) / 2;
  const int xSsid = (tft.width() - wSsid) / 2;
  const int xInfo = (tft.width() - wInfo) / 2;

  tft.loadFont(fontTitle.c_str());
  tft.setCursor((tft.width() - wTitle) / 2, yTitle);
  tft.print(title);

  tft.loadFont(fontBody.c_str());
  tft.setCursor(xBody, yBody);
  tft.print(base);

  uint16_t c1 = TFT_DARKGREY, c2 = TFT_DARKGREY, c3 = TFT_DARKGREY;
  if (phase == 1) { c1 = TFT_WHITE;      c2 = TFT_LIGHTGREY; c3 = TFT_DARKGREY; }
  if (phase == 2) { c1 = TFT_LIGHTGREY; c2 = TFT_WHITE;     c3 = TFT_LIGHTGREY; }
  if (phase == 3) { c1 = TFT_DARKGREY;  c2 = TFT_LIGHTGREY; c3 = TFT_WHITE; }

  if (!connectedNow) {
    const int xDots = xBody + wBase;
    tft.fillRect(xDots, yBody - 2, wDots + 4, hBody + 4, TFT_BLACK);

    tft.setCursor(xDots, yBody);
    tft.setTextColor(c1, TFT_BLACK); tft.print(".");
    tft.setTextColor(c2, TFT_BLACK); tft.print(".");
    tft.setTextColor(c3, TFT_BLACK); tft.print(".");
  }

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setCursor(xSsid, ySsid);
  tft.print(ssidLine);

  if (infoLine.length()) {
    tft.setCursor(xInfo, yInfo);
    tft.print(infoLine);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}
void app_setup() {
  WiFiClientSecure client;
  client.setInsecure();   // HTTPS tanúsítványok kikapcsolása
  
  hw_backlight_init_pwm();
  hw_backlight_init_pwm();

  {
    InputEncoderAppCtx ectx;
    ectx.pinA = ENC_A;
    ectx.pinB = ENC_B;
    ectx.pinBtn = ENC_BTN;
    ectx.isr = encoderISR;
    ectx.encHist = &g_encHist;

    ectx.encDelta = &g_encDelta;
    ectx.mode = (uint8_t*)&g_mode;
    ectx.volume = &g_Volume;
    ectx.menuIndex = &g_menuIndex;
    ectx.stationCount = &g_stationCount;

    ectx.modePlay = (uint8_t)MODE_PLAY;
    ectx.volMin = VOLUME_MIN;
    ectx.volMax = VOLUME_MAX;
    ectx.pulsesPerStep = ENC_PULSES_PER_STEP;

    ectx.onVolumeChanged = updateVolumeOnly;
    ectx.onMenuChanged = redrawMenuCounterAndList;
    ectx.sendVolume = stream_core_sendVolume;

    input_encoder_init(ectx);
  }


  Serial.begin(115200);
  ensureLogMutex();
  reserveHotStrings();
  logMemorySnapshot("boot");

  // Stabil dekódoláshoz / streamhez (AAC/MP3) érdemes fixen 240 MHz-en futtatni
  setCpuFrequencyMhz(240);
  delay(300);

  SPIFFS.begin(true);
  initFontPaths();
  loadLastStationFromNVS();
  loadStationsFromSPIFFS();

  initDisplayBasic();
  tft.setFont(&fonts::Font0);

// Boot logo 3 másodpercig
{
  const int logoW = LOGO_WIDTH;
  const int logoH = LOGO_HEIGHT;
  const int x = (tft.width()  - logoW) / 2;
  const int y = (tft.height() - logoH) / 2;

  tft.fillScreen(TFT_BLACK);
  tft.pushImage(x, y, logoW, logoH, myradiologo_240);
  delay(3000);
}

drawStartupScreen(0);


  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
{
  g_startupConnectScreenActive = true;
  g_startupConnectPhase = 1;
  g_startupAttemptIndex = 0;
  g_startupAttemptTotal = 0;

  wifi_manager_init(drawWiFiPortalHelp, onWiFiRestored, onWiFiAttempt);

  wifi_manager_begin_or_portal();
}

  
uint8_t phase = 0;
while (WiFi.status() != WL_CONNECTED) {
  g_startupConnectPhase = phase;
  drawStartupScreen(phase);

  phase++;
  if (phase > 3) phase = 1;
  delay(350);
}
  g_startupConnectScreenActive = false;
  drawStartupScreen(0);
  Serial.println(String("\n") + String(lang::boot_wifi_connected));
  Serial.println(lang::boot_wifi_stabilizing_log);
  delay(1200);

  startWebServer();

  recomputeLayout();

  // UI modul (első lépés: WiFi ikon) – ide kötjük be a közös állapot pointereket.
  {
    UIDisplayCtx u;
    u.tft = &tft;
    u.W = &W; u.H = &H;

    // Bottom bar + icons layout
    u.wifiX = &wifiX; u.wifiY = &wifiY; u.wifiW = &wifiW; u.wifiH = &wifiH;
    u.yVol  = &yVol;

    // Font paths (LGFX loadFont)
    u.FP_20    = uiRegularFontPtr(UI_FONT_LABEL);
    u.FP_SB_20 = uiSemiboldFontPtr(20);

    u.wifiConnectedAtMs = wifi_manager_connected_at_ptr();
    ui_display_bind(u);
  }

  initSprites();

  {
    UIStationSelectorCtx sctx;
    sctx.tft = &tft;
    sctx.sprMenu = &sprMenu;
    sctx.stations = g_stations;
    sctx.stationCount = &g_stationCount;
    sctx.menuIndex = &g_menuIndex;
    sctx.screenW = &W;
    sctx.menuListTop = &g_menuListTop;
    sctx.menuListHeight = &g_menuListHeight;
    sctx.menuItemH = &g_menuItemH;
    sctx.menuNameY = &g_menuNameY;
    sctx.labelFontPath = uiRegularFontPtr(UI_FONT_LABEL);
    sctx.activeFontPath = uiSemiboldFontPtr(24);
    sctx.menuScrollIntervalMs = MENU_MS;
    sctx.colorBg = TFT_BLACK;
    sctx.colorActiveBg = TFT_DARKGREY;
    sctx.colorActiveBorder = TFT_GOLD;
    sctx.colorActiveText = TFT_GOLD;
    sctx.colorSideText = 0xD69A;
    sctx.colorSideFarText = 0x7BEF;
    ui_stationSelectorInit(sctx);
  }

  logMemorySnapshot("after sprites");
  drawStaticUI();

  // VU meter init (audio hook fogja etetni)
  vu_init();
  ui_drawVuMeter(0, 0, 0, 0);

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);

  {
    StreamLifecyclePlaylistBind bind{};
    bind.playlistUrls = &g_playlistUrls;
    bind.playlistIndex = &g_playlistIndex;
    bind.playlistSourceUrl = &g_playlistSourceUrl;
    bind.stationUrl = &g_stationUrl;
    bind.playUrl = &g_playUrl;
    bind.paused = &g_paused;
    bind.autoNextRequested = &g_autoNextRequested;
    bind.autoNextRequestedAt = &g_autoNextRequestedAt;
    bind.pendingTitle = &g_pendingTitle;
    bind.newTitleFlag = &g_newTitleFlag;
    bind.id3Artist = &g_id3Artist;
    bind.id3Title = &g_id3Title;
    bind.id3SeenAt = &g_id3SeenAt;
    bind.pendingCodec = &g_pendingCodec;
    bind.pendingBitrateK = &g_pendingBitrateK;
    bind.pendingCh = &g_pendingCh;
    bind.pendingSampleRate = &g_pendingSampleRate;
    bind.pendingBitsPerSample = &g_pendingBitsPerSample;
    bind.newStatusFlag = &g_newStatusFlag;
    bind.connectFn = stream_core_sendConnect;
    stream_lifecycle_bindPlaylistRuntime(g_playlistMetaCtx, bind);
  }

  Audio::audio_info_callback = my_audio_info;

  {
    StreamLifecycleCoreBind scfg{};
    scfg.audio = &audio;
    scfg.logf = serialLogf;
    scfg.logln = (StreamCoreLoglnFn)serialLogln;
    scfg.connectRequestedAtMs = &g_connectRequestedAt;
    scfg.lastConnectUrl = &g_lastConnectUrl;
    scfg.taskCore = AUDIO_TASK_CORE;
    scfg.taskStack = AUDIO_TASK_STACK;
    scfg.taskPriority = AUDIO_TASK_PRIORITY;
    stream_lifecycle_beginCore(scfg);
  }

  serialLogf("[TASK] loopTask core=%d, audioTask core=%d\n", xPortGetCoreID(), AUDIO_TASK_CORE);
  logMemorySnapshot("after audio task");

  stream_lifecycle_startCurrent(g_playlistMetaCtx, g_Volume, stream_core_sendVolume, stream_core_sendConnect);

  // state_meta modul (title/status feldolgozás) init
  state_meta_setup();
}
void app_loop() {
  NetServerCtx nctx;
  nctx.server = &server;

  nctx.restartRequested = &g_restartRequested;
  nctx.restartAtMs      = &g_restartAtMs;

  nctx.lastWifiDraw = &lastWifiDraw;
  nctx.wifiDrawMs   = WIFI_DRAW_MS;
  nctx.updateWifiIconOnly        = ui_updateWifiIconOnly;
  nctx.updateBufferIndicatorOnly = updateBufferIndicatorOnly;

  nctx.handleWiFiReconnect = wifi_manager_handle_reconnect;

  net_server_poll(nctx);

  InputEncoderAppCtx ectx;
  ectx.pinA = ENC_A;
  ectx.pinB = ENC_B;
  ectx.pinBtn = ENC_BTN;
  ectx.isr = encoderISR;
  ectx.encHist = &g_encHist;

  ectx.encDelta = &g_encDelta;
  ectx.mode = (uint8_t*)&g_mode;
  ectx.volume = &g_Volume;
  ectx.menuIndex = &g_menuIndex;
  ectx.stationCount = &g_stationCount;

  ectx.modePlay = (uint8_t)MODE_PLAY;
  ectx.volMin = VOLUME_MIN;
  ectx.volMax = VOLUME_MAX;
  ectx.pulsesPerStep = ENC_PULSES_PER_STEP;

  ectx.onVolumeChanged = updateVolumeOnly;
  ectx.onMenuChanged = redrawMenuCounterAndList;
  ectx.sendVolume = stream_core_sendVolume;

  input_encoder_apply(ectx);

  InputButtonCtx bctx;
  bctx.pin = ENC_BTN;
  bctx.activeLow = true;
  bctx.longPressMs = LONG_MS;
  bctx.state = &g_buttonState;
  bctx.onShortPress = onButtonShortPress;
  bctx.onLongPress = onButtonLongPress;
  input_button_apply(bctx);
// Auto-advance playlist when the current track finishes
  if (g_autoNextRequested) {
    g_autoNextRequested = false;
    advancePlaylistAndPlay();
  }

// --- Title/Status feldolgozás kiszervezve a state_meta modulba ---
StateMetaCtx mctx;
mctx.newTitleFlag        = &g_newTitleFlag;
mctx.pendingTitle        = &g_pendingTitle;
mctx.splitArtistTitleFn  = splitArtistTitle;
mctx.artistOut           = &g_artist;
mctx.titleOut            = &g_title;
mctx.trackChangedAtMs    = &trackChangedAt;
mctx.forceRedrawText     = &g_forceRedrawText;
mctx.holdPhase           = &holdPhase;
mctx.xStation            = &xStation;
mctx.xArtist             = &xArtist;
mctx.xTitle              = &xTitle;
mctx.mode                = (uint8_t*)&g_mode;
mctx.modePlay            = (uint8_t)MODE_PLAY;
mctx.updateMarqueeFn     = updateMarquee;

mctx.newStatusFlag       = &g_newStatusFlag;
mctx.codecCur            = &g_codec;
mctx.bitrateCurKbps      = &g_bitrateK;
mctx.pendingCodec        = &g_pendingCodec;
mctx.pendingBitrateKbps  = &g_pendingBitrateK;
mctx.chCur               = &g_ch;
mctx.sampleRateCur       = &g_sampleRate;
mctx.bitsPerSampleCur    = &g_bitsPerSample;
mctx.pendingCh           = &g_pendingCh;
mctx.pendingSampleRate   = &g_pendingSampleRate;
mctx.pendingBitsPerSample= &g_pendingBitsPerSample;

mctx.drawCodecIconFn     = drawCodecIconTopLeft;
mctx.drawBottomBarFn     = drawBottomBar;
mctx.drawStreamLabelFn   = drawStreamLabelLine;

state_meta_poll(mctx);
  if (g_mode == MODE_PLAY) {
    // Only tick the marquee when it actually matters.
    if (holdPhase || g_forceRedrawText || g_anyScrollActive) updateMarquee();
  } else {
    updateMenuNameScroll();
  }

  {
    AudioControlCtx ctx = makeAudioControlCtx();
    stream_watchdog_pollReconnect(ctx);
  }

  // ---- VU meter frissítés (UI oldalon) ----
  static uint32_t lastVuMs = 0;
  uint32_t nowVu = millis();
  if (g_mode == MODE_PLAY && (nowVu - lastVuMs >= 60)) {
    lastVuMs = nowVu;
    ui_updateVuMeterOnly(vu_getL(), vu_getR(), vu_getPeakL(), vu_getPeakR());
  }

  delay(1);
}
