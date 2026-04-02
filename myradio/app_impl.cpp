// app_impl.cpp
#include <Arduino.h>
#include <WiFi.h>
#include "src/net/net_server.h"
#include <esp_wifi.h>
#include <WebServer.h>
#include "src/maint/serial_spiffs.h"
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
#include "conf/display_profile.h"
#include <SPIFFS.h>
#include "Rotary.h"
#if defined(SSD1322)
#include "logo_rgb565_20x20.h"
#if defined(SSD1322)
static constexpr int OLED_CORNER_LOGO_W = 20;
static constexpr int OLED_CORNER_LOGO_H = 20;
#ifndef MYRADIO_LOGO_20X20_W
#define MYRADIO_LOGO_20X20_W OLED_CORNER_LOGO_W
#endif
#ifndef MYRADIO_LOGO_20X20_H
#define MYRADIO_LOGO_20X20_H OLED_CORNER_LOGO_H
#endif
#endif
#include "audio_icons/aac_20.h"
#include "audio_icons/flac_20.h"
#include "audio_icons/mp3_20.h"
#include "audio_icons/ogg_20.h"
#include "audio_icons/opus_20.h"
#include "audio_icons/vor_20.h"
#include "audio_icons/icon_speaker_12.h"
#else
#include "audio_icons/aac_60.h"
#include "audio_icons/flac_60.h"
#include "audio_icons/mp3_60.h"
#include "audio_icons/ogg_60.h"
#include "audio_icons/opus_60.h"
#include "audio_icons/vor_60.h"
#endif
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
#include "src/input/input_touch.h"
#if defined(SSD1322)
#include "src/myradiologo_oled_200.h"
#else
#include "src/myradiologo_240.h"
#endif

// VU meter hooks (audio_process_i2s -> vu_feedStereoISR)
#include "src/ui/vu_meter.h"
#if defined(SSD1322)
#include "src/hw/ssd1322/ssd1322_debug_ui.h"
#endif

#include "src/lang/lang.h"


// Fallback színek, ha a platform nem definiálná ezeket a neveket
#ifndef TFT_WHITE
  #define TFT_WHITE 0xC618
#endif
#ifndef TFT_WHITE
  #define TFT_WHITE  0x7BEF
#endif
static constexpr uint16_t OLED_MUTED_GREY = 0x2104;
static constexpr uint16_t OLED_DARKER_GREY = 0x1082;
#if defined(SSD1322)
static const uint8_t OLED_MUTED_GREY_4BPP = oledgfx::ssd1322_gray4_from_rgb565(OLED_MUTED_GREY);
#endif

#if defined(SSD1322)
using LGFX_Sprite = oledgfx::LGFX_Sprite;
using LGFX_Device = oledgfx::LGFX_Device;
using oledgfx::top_left;
using oledgfx::top_right;
using oledgfx::middle_center;
#else
using LGFX_Sprite = lgfx::LGFX_Sprite;
using LGFX_Device = lgfx::LGFX_Device;
#endif

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
template <typename T>
static void applyFontPath(T& dev, const String* fp);
static String* uiRegularFontPtr(int preferredSize);
static String* uiSemiboldFontPtr(int preferredSize);
static const String& uiRegularFont(int preferredSize);
static const String& uiSemiboldFont(int preferredSize);
bool app_isMenuMode();
void app_exitMenuRedrawPlayUI();
bool startPlaybackCurrent(bool allowReloadPlaylist);
void drawStartupScreen(uint8_t phase);
static void onButtonShortPress();
static void onButtonLongPress();
static void onTouchTap(int x, int y);
static void onTouchLongPress(int x, int y);
void drawBottomBar();
static void drawOledIpLine();
#if defined(SSD1322)
static void oledUpdateVolumeOnly();

static void oledInvalidateVuMeter();
static void oledDrawVuMeter(uint8_t lvlL, uint8_t lvlR, uint8_t peakL, uint8_t peakR);
static void oledUpdateVuMeterOnly(uint8_t lvlL, uint8_t lvlR, uint8_t peakL, uint8_t peakR);
#endif

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
#if defined(SSD1322)
static void drawGray4Bitmap(int x, int y, int w, int h, const uint8_t* data) {
  if (!data) return;
  int i = 0;
  for (int yy = 0; yy < h; ++yy) {
    for (int xx = 0; xx < w; xx += 2) {
      const uint8_t b = data[i++];
      const uint8_t p1 = (b >> 4) & 0x0F;
      const uint8_t p2 = b & 0x0F;
      tft.Jamis_SSD1322::drawPixel(x + xx, y + yy, p1);
      if (xx + 1 < w) tft.Jamis_SSD1322::drawPixel(x + xx + 1, y + yy, p2);
    }
  }
  tft.display();
}
#endif

#if defined(SSD1322)
static inline uint8_t rgb565ToGray4(uint16_t c) {
  uint8_t r = (c >> 11) & 0x1F;
  uint8_t g = (c >> 5) & 0x3F;
  uint8_t b = c & 0x1F;
  uint16_t gray8 = (r * 255 / 31 * 30 + g * 255 / 63 * 59 + b * 255 / 31 * 11) / 100;
  return (uint8_t)(gray8 >> 4);
}

static void drawGrayFromRgb565Bitmap(int x, int y, int w, int h, const uint16_t* data) {
  if (!data) return;
  for (int yy = 0; yy < h; ++yy) {
    for (int xx = 0; xx < w; ++xx) {
      uint8_t g4 = rgb565ToGray4(data[yy * w + xx]);
      if (g4) tft.Jamis_SSD1322::drawPixel(x + xx, y + yy, g4);
    }
  }
  tft.display();
}
#endif

static bool g_tftReady = false;
static bool g_touchEnabled = (TOUCH_MODEL != TOUCH_NONE);
static InputTouchRuntimeState g_touchState;

static void initDisplayBasic() {
  if (g_tftReady) return;
  // Kijelző init (ne maradjon fehér / üres a háttérvilágítás alatt)
  tft.init();
  tft.setRotation(TFT_ROTATION);      // a Lovyan_config.h-ban állítod be
  tft.setBrightness(255);
  tft.fillScreen(TFT_BLACK);
#if defined(SSD1322)
  ssd1322_draw_debug_boot(tft);
#endif
  g_tftReady = true;
}

static void drawWiFiPortalHelp(const char* apSsid, const IPAddress& ip) {
  initDisplayBasic();
  tft.fillScreen(TFT_BLACK);

#if defined(SSD1322)
  const bool compactOledWifiSetup = (tft.width() == 256 && tft.height() == 64);
  if (compactOledWifiSetup) {
    const int pad = 3;
    const int radius = 6;
    const int innerL = pad + 5;
    const int innerR = tft.width() - pad - 5;
    const int innerW = innerR - innerL + 1;
    tft.drawRoundRect(pad, pad, tft.width() - pad * 2, tft.height() - pad * 2, radius, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    { String fp = uiSemiboldFont(8); applyFontPath(tft, &fp); }
    int y = 0;
    String title = lang::wifi_setup_about_title;
    int titleW = tft.textWidth(title.c_str());
    int titleX = innerL;
    if (titleW < innerW) titleX = innerL + ((innerW - titleW) / 2);
    tft.drawString(title, titleX, y);
    y += tft.fontHeight();

    { String fp = uiRegularFont(9); applyFontPath(tft, &fp); }
    const int lineH = tft.fontHeight();

    auto clipToWidth = [&](const String& text, int maxW) -> String {
      if (maxW <= 0) return String("");
      if (tft.textWidth(text.c_str()) <= maxW) return text;
      const char* dots = "...";
      const int dotsW = tft.textWidth(dots);
      if (dotsW >= maxW) return String("");
      String out;
      out.reserve(text.length());
      for (size_t i = 0; i < text.length(); ++i) {
        String trial = out + text[i];
        if (tft.textWidth((trial + dots).c_str()) > maxW) break;
        out += text[i];
      }
      return out + dots;
    };

    auto drawClippedLine = [&](const String& text, int yy) {
      tft.drawString(clipToWidth(text, innerW), innerL, yy);
    };

    drawClippedLine(String(lang::wifi_setup_step1) + " " + String(apSsid ? apSsid : ""), y); y += lineH;
    drawClippedLine(String(lang::wifi_setup_step2) + String(" http://") + ip.toString(), y); y += lineH;
    drawClippedLine(String(lang::wifi_setup_step3), y); y += lineH;
    drawClippedLine(String(lang::wifi_setup_save_hint) + " (" + String(lang::wifi_setup_restart_hint) + ")", y);
    return;
  }
#endif

  const int pad = 8;
  const int w = tft.width()  - pad * 2;
  const int h = tft.height() - pad * 2;
  tft.drawRoundRect(pad, pad, w, h, 12, TFT_WHITE);

  const String& fontTitle = uiSemiboldFont(UI_FONT_HEADER);
  const String& fontBody  = uiRegularFont(UI_FONT_STREAM);

  const int x = pad + 10;
  int y = pad + 10;
  const bool compactWifiSetup = (tft.width() <= 320 || tft.height() <= 240);
  const int titleGap = compactWifiSetup ? 4 : 6;
  const int lineGap  = 1;
  const int blockGap = compactWifiSetup ? 1 : 3;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  { String fp = fontTitle; applyFontPath(tft, &fp); }
  tft.setCursor(x + 60, y);
  tft.println(lang::wifi_setup_about_title);
  y += tft.fontHeight() + titleGap;

  { String fp = fontBody; applyFontPath(tft, &fp); }

  tft.setCursor(x, y);
  tft.println(lang::wifi_setup_step1);
  y += tft.fontHeight() + lineGap;
  tft.setCursor(x + 18, y);
  tft.print(" ");
  tft.println(apSsid);
  y += tft.fontHeight() + blockGap;

  tft.setCursor(x, y);
  tft.println(lang::wifi_setup_step2);
  y += tft.fontHeight() + lineGap;
  tft.setCursor(x + 22, y);
  tft.print("http://");
  tft.println(ip.toString());
  y += tft.fontHeight() + blockGap;

  tft.setCursor(x, y);
  tft.println(lang::wifi_setup_step3);
  y += tft.fontHeight() + lineGap;
  tft.setCursor(x + 22, y);
  tft.println(lang::wifi_setup_save_hint);
  y += tft.fontHeight() + blockGap;

  tft.setCursor(x + 68, y);
  tft.println(lang::wifi_setup_restart_hint);
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
static String FS_9, FS_10, FS_12, FS_20, FS_24, FS_28;
static String FS_SB_9, FS_SB_10, FS_SB_12, FS_SB_20, FS_SB_24, FS_SB_28;

// LGFX útvonal (LGFX loadFont-hoz: /spiffs/...)
static String FP_9, FP_10, FP_12, FP_20, FP_24, FP_28;
static String FP_SB_9, FP_SB_10, FP_SB_12, FP_SB_20, FP_SB_24, FP_SB_28;
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
  FS_9  = resolveFSPath("test_9.vlw");
  FS_10 = resolveFSPath("test_10.vlw");
  FS_12 = resolveFSPath("test_12.vlw");
  FS_20 = resolveFSPath("test_20.vlw");
  FS_24 = resolveFSPath("test_24.vlw");
  FS_28 = resolveFSPath("test_28.vlw");

  // SemiBold
  FS_SB_9  = resolveFSPath("test_sb_9.vlw");
  FS_SB_10 = resolveFSPath("test_sb_10.vlw");
  FS_SB_12 = resolveFSPath("test_sb_12.vlw");
  FS_SB_20 = resolveFSPath("test_sb_20.vlw");
  FS_SB_24 = resolveFSPath("test_sb_24.vlw");
  FS_SB_28 = resolveFSPath("test_sb_28.vlw");

  // LGFX pathok
  FP_9  = toLGFXPath(FS_9);
  FP_10 = toLGFXPath(FS_10);
  FP_12 = toLGFXPath(FS_12);
  FP_20 = toLGFXPath(FS_20);
  FP_24 = toLGFXPath(FS_24);
  FP_28 = toLGFXPath(FS_28);

  FP_SB_9  = toLGFXPath(FS_SB_9);
  FP_SB_10 = toLGFXPath(FS_SB_10);
  FP_SB_12 = toLGFXPath(FS_SB_12);
  FP_SB_20 = toLGFXPath(FS_SB_20);
  FP_SB_24 = toLGFXPath(FS_SB_24);
  FP_SB_28 = toLGFXPath(FS_SB_28);

  Serial.println("[FONT] SPIFFS files:");
  Serial.printf("  10: %s (%s)\n", FS_10.c_str(), SPIFFS.exists(FS_10) ? "OK" : "MISSING");
  Serial.printf("  12: %s (%s)\n", FS_12.c_str(), SPIFFS.exists(FS_12) ? "OK" : "MISSING");
  Serial.printf("  SB12: %s (%s)\n", FS_SB_12.c_str(), SPIFFS.exists(FS_SB_12) ? "OK" : "MISSING");
  Serial.println("[FONT] LGFX loadFont paths:");
  Serial.printf("  10: %s\n", FP_10.c_str());
  Serial.printf("  12: %s\n", FP_12.c_str());
  Serial.printf("  SB12: %s\n", FP_SB_12.c_str());
}

static String* pickAvailableFontPtr(
    int preferredSize,
    String& fp9, String& fp10, String& fp12, String& fp20, String& fp24, String& fp28,
    String& fs9, String& fs10, String& fs12, String& fs20, String& fs24, String& fs28) {
  struct FontChoice { int size; String* fp; String* fs; };
  FontChoice choices[] = {
    {9,  &fp9,  &fs9},
    {10, &fp10, &fs10},
    {12, &fp12, &fs12},
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

  return &fp10;
}

static String* uiRegularFontPtr(int preferredSize) {
  return pickAvailableFontPtr(preferredSize, FP_9, FP_10, FP_12, FP_20, FP_24, FP_28,
                              FS_9, FS_10, FS_12, FS_20, FS_24, FS_28);
}

static String* uiSemiboldFontPtr(int preferredSize) {
  return pickAvailableFontPtr(preferredSize, FP_SB_9, FP_SB_10, FP_SB_12, FP_SB_20, FP_SB_24, FP_SB_28,
                              FS_SB_9, FS_SB_10, FS_SB_12, FS_SB_20, FS_SB_24, FS_SB_28);
}

static const String& uiRegularFont(int preferredSize)  { return *uiRegularFontPtr(preferredSize); }
static const String& uiSemiboldFont(int preferredSize) { return *uiSemiboldFontPtr(preferredSize); }


template <typename T>
static void applyFontPath(T& dev, const String* fp) {
#if defined(SSD1322)
  if (fp && fp->length()) dev.loadFont(fp->c_str());
  else dev.setFont((const GFXfont*)nullptr);
#else
  if (fp && fp->length()) dev.loadFont(fp->c_str());
  else dev.unloadFont();
#endif
  dev.setTextSize(1);
  dev.setTextWrap(false);
}

template <typename T>
static void applyRegularUiFont(T& dev, int preferredSize) {
  applyFontPath(dev, uiRegularFontPtr(preferredSize));
}

template <typename T>
static void applySemiboldUiFont(T& dev, int preferredSize) {
  applyFontPath(dev, uiSemiboldFontPtr(preferredSize));
}

template <typename T>
static void applyStationUiFont(T& dev) {
#if defined(SSD1322)
  if (FS_SB_12.length() && SPIFFS.exists(FS_SB_12)) applyFontPath(dev, &FP_SB_12);
  else applyFontPath(dev, uiSemiboldFontPtr(10));
#else
  applySemiboldUiFont(dev, UI_FONT_STATION);
#endif
}

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
static void clearRect(int x, int y, int w, int h) {
#if defined(SSD1322)
  const int BOTTOM_BAR_Y = 52;
  if (w <= 0 || h <= 0) return;
  if (ui_stationSelectorActive()) {
    tft.fillRect(x, y, w, h, TFT_BLACK);
    return;
  }
  if (y >= BOTTOM_BAR_Y) return;
  if (y + h > BOTTOM_BAR_Y) h = BOTTOM_BAR_Y - y;
  if (h <= 0) return;
#endif
  tft.fillRect(x, y, w, h, TFT_BLACK);
}



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
// ------------------ Codec ikon ------------------ //
#if defined(SSD1322)
#ifndef CODEC_ICON_W
  #define CODEC_ICON_W 20
#endif
#ifndef CODEC_ICON_H
  #define CODEC_ICON_H 20
#endif
static const uint8_t* codecIconPtrFromCodec(const String& codec) {
  String u = codec; u.toUpperCase();
  if (u.indexOf("FLAC") >= 0) return flac_20;
  if (u.indexOf("OPUS") >= 0) return opus_20;
  if (u.indexOf("VORBIS") >= 0) return vor_20;
  if (u.indexOf("OGG") >= 0) return ogg_20;
  if (u.indexOf("AAC") >= 0) return aac_20;
  if (u.indexOf("MP3") >= 0) return mp3_20;
  return nullptr;
}
#else
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
#endif

static void drawCodecIconTopLeft() {
  if (g_mode != MODE_PLAY || ui_stationSelectorActive()) return;
  const int x = 0;
  const int y = 0;
  clearRect(x, y, CODEC_ICON_W, CODEC_ICON_H);

#if defined(SSD1322)
  const uint8_t* img = codecIconPtrFromCodec(g_codec);
  if (!img) return;
  drawGray4Bitmap(x, y, CODEC_ICON_W, CODEC_ICON_H, img);
#else
  const uint16_t* img = codecIconPtrFromCodec(g_codec);
  if (!img) return;
  tft.pushImage(x, y, CODEC_ICON_W, CODEC_ICON_H, img);
#endif
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
#if defined(SSD1322)
  oledUpdateVolumeOnly();
#else
  ui_drawBottomBar(g_Volume, g_bufferPercent, (WiFi.status() == WL_CONNECTED));
#endif
}

static void updateBufferIndicatorOnly() {
  if (g_mode != MODE_PLAY || ui_stationSelectorActive()) return;
#if defined(SSD1322)
  drawBottomBar();
#else
  ui_updateBufferIndicatorOnly(g_bufferPercent);
#endif
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

#if defined(SSD1322)
static const uint32_t MARQUEE_MS = 120;  // OLED-en ritkább marquee tick, hogy kisebb legyen a kijelzőterhelés
static const int32_t  SCROLL_STEP = 2;   // közel azonos vizuális tempó, de kevesebb redraw
#else
static const uint32_t MARQUEE_MS = 80;   // marquee tick (smaller=faster)
static const int32_t  SCROLL_STEP = 3;   // pixels per tick
#endif
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

static int textWidthMain(const String& s) { applyStationUiFont(tft); return tft.textWidth(s.c_str()); }

static void recomputeLayout() {
  W = tft.width();
  H = tft.height();

  // Header font
#if defined(SSD1322)
  applySemiboldUiFont(tft, 12);
#else
  applySemiboldUiFont(tft, UI_FONT_HEADER);
#endif
  int hHeader = tft.fontHeight();
  yHeader = UI_HEADER_Y;

  // Small label font
#if defined(SSD1322)
  applyRegularUiFont(tft, 9);
#else
  applyRegularUiFont(tft, UI_FONT_LABEL);
#endif
  int hLabel = tft.fontHeight();

  // Stream line font
#if defined(SSD1322)
  applyRegularUiFont(tft, 9);
#else
  applyRegularUiFont(tft, UI_FONT_STREAM);
#endif
  int hStream = tft.fontHeight();

  // Main text fonts / sprite heights
#if defined(SSD1322)
  applyRegularUiFont(tft, 9);
#else
  applySemiboldUiFont(tft, UI_FONT_ARTIST);
#endif
  int hArtistText = tft.fontHeight();
  hArtistLine = hArtistText + UI_TEXT_LINE_EXTRA;

#if defined(SSD1322)
  applyRegularUiFont(tft, 9);
#else
  applyRegularUiFont(tft, UI_FONT_TITLE);
#endif
  int hTitleText = tft.fontHeight();
  hTitleLine  = hTitleText + UI_TEXT_LINE_EXTRA;

applyStationUiFont(tft);
  int hStationText = tft.fontHeight();
  hStationLine = hStationText + UI_TEXT_LINE_EXTRA;

#if defined(SSD1322)
  // OLED: a VLW sorok tényleges magasságát vegyük figyelembe,
  // különben a sorok egymásba törölnek vagy a karakterek alja levágódik.
  hStationLine = max(hStationText + 2, 14);
  hArtistLine  = max(hArtistText + 1, 10);
  hTitleLine   = max(hTitleText + 1, 10);
#endif

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

#if defined(SSD1322)
  // OLED: fix sávos layout, de hagyjunk 1-2 px biztonsági rést a sorok között,
  // hogy a részleges törlések ne nyúljanak bele a szomszédos sorokba.
  yHeader = 0;
  yStationLabel = 0;
  yStationName = 2;
  yStreamLabel = 19;
  yArtist = 30;
  yTitle = 41;
  yVol = 53;
  wifiW = 17; wifiH = 8;
  wifiX = W - wifiW - 1;
  wifiY = 53;
#endif
}

//
// ------------------ Sprites ------------------ //

static int stationSpriteX();
static int stationSpriteWidth();
static int oledStationRowY();
void initSprites() {
  const bool havePsram = psramFound();

  sprStation.setColorDepth(16);
  if (havePsram) sprStation.setPsram(true);
  sprStation.createSprite(stationSpriteWidth(), hStationLine);
  sprStation.fillScreen(TFT_BLACK);
  applyStationUiFont(sprStation);
  sprStation.setTextWrap(false);
  sprStation.setTextColor(TFT_ORANGE, TFT_BLACK); // ÁLLOMÁS SZÍNE

  sprArtist.setColorDepth(16);
  if (havePsram) sprArtist.setPsram(true);
  sprArtist.createSprite(W, hArtistLine);
  sprArtist.fillScreen(TFT_BLACK);
#if defined(SSD1322)
  applySemiboldUiFont(sprArtist, 10);
#else
  applySemiboldUiFont(sprArtist, UI_FONT_ARTIST);
#endif
  sprArtist.setTextWrap(false);
  sprArtist.setTextColor(TFT_CYAN, TFT_BLACK); // ELŐADÓ SZÍNE

  sprTitle.setColorDepth(16);
  if (havePsram) sprTitle.setPsram(true);
  sprTitle.createSprite(W, hTitleLine);
  sprTitle.fillScreen(TFT_BLACK);
#if defined(SSD1322)
  applyRegularUiFont(sprTitle, 10);
#else
  applyRegularUiFont(sprTitle, UI_FONT_TITLE);
#endif
  sprTitle.setTextWrap(false);
#if defined(SSD1322)
  sprTitle.setTextColor(OLED_MUTED_GREY, TFT_BLACK); // DAL CÍM SZÍNE (OLED: egységes szürke)
#else
  sprTitle.setTextColor(TFT_SILVER, TFT_BLACK); // DAL CÍM SZÍNE
#endif

  sprMenu.setColorDepth(16);
  if (havePsram) sprMenu.setPsram(true);
  sprMenu.createSprite(W, UI_MENU_H);
  sprMenu.fillScreen(TFT_BLACK);
#if defined(SSD1322)
  applyRegularUiFont(sprMenu, 10);
#else
  applyRegularUiFont(sprMenu, UI_FONT_MENU);
#endif
  sprMenu.setTextWrap(false);
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
#if defined(SSD1322)
  // Az eredeti alsó 1 px-es puffercsík OLED-en megszűnt.
  // A buffer kijelzés most a PUF blokkban történik az IP és a VU között.
  return;
#else
  if (g_mode != MODE_PLAY || ui_stationSelectorActive()) return;
  StreamWatchdogBufferCtx bctx{};
  bctx.stationUrl = &g_stationUrl;
  bctx.lastBufferCheckMs = &lastBufferCheck;
  bctx.bufferFilled = &g_bufferFilled;
  bctx.bufferFree = &g_bufferFree;
  bctx.bufferTotal = &g_bufferTotal;
  bctx.bufferPercent = &g_bufferPercent;
  bctx.readBufferFilledFn = []() -> size_t { return audio.inBufferFilled(); };
  bctx.readBufferFreeFn = []() -> size_t { return audio.inBufferFree(); };
  bctx.needStreamReconnect = &g_needStreamReconnect;
  bctx.paused = &g_paused;
  bctx.connectRequestedAtMs = &g_connectRequestedAt;
  bctx.refreshMs = 500;
  bctx.startupGraceMs = 6000;
  bctx.lowBufferHoldMs = 7000;
  bctx.lowBufferPercent = 8;
  stream_watchdog_updateBuffer(bctx);

  ui_drawBottomBar(g_Volume, g_bufferPercent, (WiFi.status() == WL_CONNECTED));
#endif
}


// ------------------ UI ------------------ //
#if defined(SSD1322)
static int oledArtistRowY();
static int oledTitleRowY();
static bool g_oledPauseBadgeDirty = true;
static bool g_oledPauseBadgeLastState = false;
#endif
#if defined(SSD1322)
static void drawOledPausedBadgeOverlay(bool force = false) {
  if (!force && !g_oledPauseBadgeDirty && g_oledPauseBadgeLastState == g_paused) return;

  const int leftBound = CODEC_ICON_W + 1;
  const int rightBound = W - OLED_CORNER_LOGO_W - 1;
  if (rightBound <= leftBound) return;

  applyRegularUiFont(tft, 9);
  const String txt = text_fix(lang::ui_paused);
  const int th = tft.fontHeight();
  const int tw = tft.textWidth(txt.c_str());

  const int padX = 4;
  const int padY = 2;
  const int bw = tw + padX * 2;
  const int bh = th + padY * 2;
  const int bx = W - bw - 4;
  int by = yArtist + ((yTitle + hTitleLine - yArtist) - bh) / 2;
  if (by < 0) by = 0;

  const int clearX = bx - 1;
  const int clearY = (by > 1) ? (by - 1) : 0;
  const int clearW = min(W - clearX, bw + 3);
  const int clearH = min(H - clearY, bh + 3);

  // Ha nincs pause aktív, normál full redraw közben ne töröljük ki a title végét.
  // Törlés csak akkor kell, ha korábban tényleg volt badge a képernyőn.
  if (!g_paused) {
    if (g_oledPauseBadgeLastState) {
      // Unpause után a badge helyét ne csak lokálisan töröljük,
      // hanem az egész artist+title zónát rajzoljuk vissza.
      // Erre azért van szükség, mert meta nélküli adóknál (pl. csak "LIVE")
      // nem feltétlen jön azonnal új szövegfrissítés, így a badge maradéka
      // külön redraw nélkül ott maradhatna a képernyőn.
      const int fullClearY = (oledArtistRowY() > 0) ? oledArtistRowY() : 0;
      const int fullClearH = (oledTitleRowY() + hTitleLine) - fullClearY;
      if (fullClearH > 0) clearRect(0, fullClearY, W, fullClearH);
      sprArtist.pushSprite(0, oledArtistRowY());
      sprTitle.pushSprite(0, oledTitleRowY());
    }
    g_oledPauseBadgeLastState = false;
    g_oledPauseBadgeDirty = false;
    return;
  }

  if (clearW > 0 && clearH > 0) clearRect(clearX, clearY, clearW, clearH);
  tft.fillRect(bx, by, bw, bh, OLED_DARKER_GREY);
  tft.drawRect(bx, by, bw, bh, OLED_MUTED_GREY);
  tft.setTextColor(TFT_WHITE, OLED_DARKER_GREY);
  tft.setTextDatum(top_left);
  tft.drawString(txt, bx + padX, by + padY);

  g_oledPauseBadgeLastState = true;
  g_oledPauseBadgeDirty = false;
}
#endif

static void drawStreamLabelLine() {
#if defined(SSD1322)
  applyRegularUiFont(tft, 9);
#else
  applyRegularUiFont(tft, UI_FONT_STREAM);
#endif
  int th = tft.fontHeight();
  int lineY = yStreamLabel;
#if defined(SSD1322)
  lineY -= 1;
#endif
  int lineH = th + 1;

#if defined(SSD1322)
  // OLED-en a stream sor területét és a lejjebb helyezett pause badge helyét is
  // teljesen töröljük, hogy ki/be kapcsoláskor ne maradjon ott semmi.
  const int leftBound = CODEC_ICON_W + 1;
  const int rightBound = W - OLED_CORNER_LOGO_W - 1;
  const int clearY = (lineY > 1) ? (lineY - 1) : 0;
  const int clearH = lineH + 4;
  if (rightBound > leftBound) clearRect(leftBound, clearY, rightBound - leftBound, clearH);

  const int badgeClearY = (yArtist > 1) ? (yArtist - 1) : 0;
  const int badgeClearH = (yTitle - yArtist) + hTitleLine + 3;
  if (rightBound > leftBound && badgeClearH > 0) clearRect(leftBound, badgeClearY, rightBound - leftBound, badgeClearH);
#else
  clearRect(0, lineY - 1, W, lineH + 2);
#endif

  String line;
#if defined(SSD1322)
  line = String(g_ch > 0 ? String(g_ch) : String("--"));
  line += "ch | ";
  if (g_sampleRate > 0) line += String((g_sampleRate + 500) / 1000) + "kHz";
  else line += "--kHz";
  line += " | ";
  if (g_bitsPerSample > 0) line += String(g_bitsPerSample) + "bit";
  else line += "--bit";
  if (g_bitrateK > 0) line += " | " + String(g_bitrateK) + "kb/s";
#else
  line = text_fix(lang::ui_stream_prefix);
  line += formatAudioInfoLine();
  if (g_bitrateK > 0) line += " | " + formatRate(g_bitrateK);
#endif

  #if defined(SSD1322)
  tft.setTextColor(OLED_MUTED_GREY, TFT_BLACK);
  int twLine = tft.textWidth(line.c_str());
  int x = (twLine <= (rightBound - leftBound)) ? (leftBound + ((rightBound - leftBound) - twLine) / 2) : leftBound;
  #else
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int twLine = tft.textWidth(line.c_str());
  int x = (twLine <= W) ? (W - twLine) / 2 : 0;
  #endif
#if defined(SSD1322)
  tft.setTextDatum(top_left);
  tft.drawString(line, x, lineY);
#else
  tft.setCursor(x, lineY);
  tft.print(line);
#endif

#if defined(SSD1322)
  g_oledPauseBadgeDirty = true;
#else
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
#endif
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
  g_forceRedrawText = true;
#if defined(SSD1322)
  g_oledPauseBadgeDirty = true;
#endif
}

void togglePaused() {
  AudioControlCtx ctx = makeAudioControlCtx();
  audio_control_togglePaused(ctx);
  g_forceRedrawText = true;
#if defined(SSD1322)
  g_oledPauseBadgeDirty = true;
#endif
}

// ---------- M3U playlist helpers ----------
bool startPlaybackCurrent(bool allowReloadPlaylist) {
  return playlist_runtime_startPlaybackCurrent(g_playlistMetaCtx, allowReloadPlaylist);
}

static bool advancePlaylistAndPlay() {
  return playlist_runtime_advancePlaylistAndPlay(g_playlistMetaCtx);
}

static int oledStationRowY() {
#if defined(SSD1322)
  return yStationName - 1;
#else
  return yStationName;
#endif
}

void updateStationNameUI() {
  // Ensure widths/centering are up to date for the current text
  recalcTextMetrics();
  // Draw via sprite (font already loaded), and mark caches so marquee won't immediately redraw again.
  int32_t xS = (!g_scrollStation && g_stationName.length()) ? g_centerXStation : 0;
  // If it's wider than the sprite, start at 0 (marquee handles scrolling elsewhere).
  if (sprStation.textWidth(g_stationName.c_str()) > (int)sprStation.width()) xS = 0;
  renderLine(sprStation, g_stationName, xS);
  #if defined(SSD1322)
  sprStation.pushSprite(stationSpriteX(), oledStationRowY());
#else
  sprStation.pushSprite(0, oledStationRowY());
#endif

  g_lastStationDrawn = g_stationName;
  g_lastStationX = xS;
}

static int spriteTextYOffset(const LGFX_Sprite& spr) {
#if defined(SSD1322)
  if (&spr == &sprStation) return 0;
#endif
  return (&spr == &sprStation) ? 1 : 0;
}

static int oledArtistRowY() {
#if defined(SSD1322)
  return yArtist;
#else
  return yArtist;
#endif
}

static int oledTitleRowY() {
#if defined(SSD1322)
  return yTitle + 2;
#else
  return yTitle;
#endif
}

static int oledTitleClearY() {
#if defined(SSD1322)
  return yTitle - 4;
#else
  return (yTitle - 1);
#endif
}

static int stationSpriteX() {
#if defined(SSD1322)
  return 18;
#else
  return 0;
#endif
}

static int stationSpriteWidth() {
#if defined(SSD1322)
  return (W > 40 ? W - 40 : W);
#else
  return W;
#endif
}


static void renderLine(LGFX_Sprite& spr, const String& text, int32_t x) {
  spr.fillScreen(TFT_BLACK);
#if defined(SSD1322)
  // OLED sprite + VLW: a custom kompatibilitási réteg drawString()-ban kezeli a VLW fontot.
  // Sima print() esetén bitmap fallback jönne.
  spr.setTextDatum(top_left);
  spr.drawString(text, x, spriteTextYOffset(spr));
#else
  spr.setCursor(x, spriteTextYOffset(spr));
  spr.print(text);
#endif
}

// Seamless marquee: draw the same (already "text + sep") string twice back-to-back.
// This makes the loop naturally become: "...end * start..." without a dead gap.
static void renderMarqueeLine(LGFX_Sprite& spr, const String& marqueeText, int32_t x, int wMarq) {
  spr.fillScreen(TFT_BLACK);
  const int y = spriteTextYOffset(spr);

#if defined(SSD1322)
  spr.setTextDatum(top_left);
  // First copy
  spr.drawString(marqueeText, x, y);

  // Second copy immediately after the first (so there is always content coming in)
  spr.drawString(marqueeText, x + wMarq, y);
#else
  // First copy
  spr.setCursor(x, y);
  spr.print(marqueeText);

  // Second copy immediately after the first (so there is always content coming in)
  spr.setCursor(x + wMarq, y);
  spr.print(marqueeText);
#endif
}

static int32_t calcCenterX(LGFX_Sprite& spr, const String& s) {
  // Fallback centering based on sprite width and textWidth.
  int w = spr.textWidth(s.c_str());
  int sw = (int)spr.width();
  int32_t x = (w <= sw) ? ((sw - w) / 2) : 0;
  return (x < 0) ? 0 : x;
}


#if defined(SSD1322)
static void drawOledWifiBarsDirect(int x, int y, int level) {
  // OLED 4-bit grayscale direkt pixeles Wi-Fi ikon.
  // x,y = bal felső sarok, 11x8 px terület.
  if (level < 0) level = 0;
  if (level > 4) level = 4;

  const uint8_t offG = 0x4;
  const uint8_t onG  = 0xD;
  const int barW = 2;
  const int gap  = 1;
  const int heights[4] = {2, 4, 6, 8};
  const int baseY = y + 7;

  // Teljes ikon terület törlése.
  for (int yy = 0; yy < 8; ++yy) {
    for (int xx = 0; xx < 11; ++xx) {
      tft.Jamis_SSD1322::drawPixel(x + xx, y + yy, 0x0);
    }
  }

  for (int i = 0; i < 4; ++i) {
    const uint8_t g = (i < level) ? onG : offG;
    const int h = heights[i];
    const int bx = x + i * (barW + gap);
    const int by = baseY - h + 1;
    for (int xx = 0; xx < barW; ++xx) {
      for (int yy = 0; yy < h; ++yy) {
        tft.Jamis_SSD1322::drawPixel(bx + xx, by + yy, g);
      }
    }
  }
}
#endif


#if defined(SSD1322)
static int g_oledVuX = 0;
static int g_oledVuY = 0;
static int g_oledVuW = 0;
static int g_oledVuH = 0;
static int g_oledVuLabelX = 0;
static int g_oledVuBarX = 0;
static int g_oledPufX = 0;
static int g_oledPufY = 0;
static int g_oledPufLabelX = 0;
static int g_oledPufBarX = 0;
static int g_oledPufBarW = 0;
static int g_oledPufBarH = 3;
static int g_oledVuLastFillL = -1;
static int g_oledVuLastFillR = -1;
static int g_oledVuLastPeakL = -1;
static int g_oledVuLastPeakR = -1;

static void oledTinyDrawGlyph(char c, int x, int y, uint8_t onG = 15) {
  static const uint8_t GL_L[5]   = {0b100,0b100,0b100,0b100,0b111};
  static const uint8_t GL_R[5]   = {0b110,0b101,0b110,0b101,0b101};
  static const uint8_t GL_P[5]   = {0b110,0b101,0b110,0b100,0b100};
  static const uint8_t GL_U[5]   = {0b101,0b101,0b101,0b101,0b111};
  static const uint8_t GL_F[5]   = {0b111,0b100,0b110,0b100,0b100};
  static const uint8_t GL_V[5]   = {0b101,0b101,0b101,0b101,0b010};
  const uint8_t* rows = nullptr;
  switch (c) {
    case 'L': rows = GL_L; break;
    case 'R': rows = GL_R; break;
    case 'P': rows = GL_P; break;
    case 'U': rows = GL_U; break;
    case 'F': rows = GL_F; break;
    case 'V': rows = GL_V; break;
    default: return;
  }
  for (int yy = 0; yy < 5; ++yy) {
    const uint8_t row = rows[yy];
    for (int xx = 0; xx < 3; ++xx) {
      if (row & (1 << (2 - xx))) {
        tft.Jamis_SSD1322::drawPixel(x + xx, y + yy, onG);
      }
    }
  }
}

static void oledTinyDrawText(const char* text, int x, int y, uint8_t onG = 15, int gap = 1) {
  if (!text) return;
  int cx = x;
  for (const char* p = text; *p; ++p) {
    oledTinyDrawGlyph(*p, cx, y, onG);
    cx += 3 + gap;
  }
}

static inline int oledTinyTextWidth(const char* text, int gap = 1) {
  if (!text || !*text) return 0;
  int n = 0;
  for (const char* p = text; *p; ++p) ++n;
  return n * 3 + (n - 1) * gap;
}

static inline int oledVuClamp(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void oledDrawPufIndicator(int percent) {
  if (g_oledPufBarW <= 0 || g_oledPufBarH <= 0) return;

  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  oledTinyDrawText("PUF", g_oledPufLabelX, g_oledPufY - 1, OLED_MUTED_GREY_4BPP, 1);

  const uint8_t trackG = 6;
  const uint8_t fillG  = 13;
  const int fillW = map(percent, 0, 100, 0, g_oledPufBarW);

  for (int x = 0; x < g_oledPufBarW; ++x) {
    const uint8_t g = (x < fillW) ? fillG : trackG;
    for (int y = 0; y < g_oledPufBarH; ++y) {
      tft.Jamis_SSD1322::drawPixel(g_oledPufBarX + x, g_oledPufY + y, g);
    }
  }
}

static void oledVuComputeLayout(int rowTop, int wifiXLocal, int vuRightGap, int volRightX) {
  (void)vuRightGap;
  const int pufLabelW = oledTinyTextWidth("PUF", 1) + 2;
  const int pufLabelGap = 2;
  const int pufBarW = 20;
  const int pufBlockW = pufLabelW + pufLabelGap + pufBarW;

  const int labelW = oledTinyTextWidth("VU", 1);
  const int labelGap = 2;
  const int targetBarW = 34;
  const int totalW = pufBlockW + 4 + labelW + labelGap + targetBarW;
  const int vuRight = wifiXLocal - 5;
  const int preferredX = vuRight - totalW;
  const int minX = volRightX + 6;
  const int baseX = (preferredX > minX) ? preferredX : minX;

  g_oledPufLabelX = baseX;
  g_oledPufBarX = g_oledPufLabelX + pufLabelW + pufLabelGap;
  g_oledPufBarW = pufBarW;
  g_oledPufY = rowTop + 4;
  g_oledPufX = g_oledPufLabelX;

  const int vuX = g_oledPufBarX + g_oledPufBarW + 4;
  const int barX = vuX + labelW + labelGap;
  const int barW = vuRight - barX;

  g_oledVuX = vuX;
  g_oledVuY = rowTop + 3;
  g_oledVuLabelX = vuX;
  g_oledVuBarX = barX;
  g_oledVuW = (barW > 12) ? barW : 0;
  g_oledVuH = 5;
}

static void oledClearVuArea() {
  if (g_oledPufBarW > 0) {
    const int pufW = (g_oledPufBarX + g_oledPufBarW) - g_oledPufX;
    tft.fillRect(g_oledPufX, g_oledPufY - 2, pufW, g_oledPufBarH + 4, TFT_BLACK);
  }
  if (g_oledVuW <= 0 || g_oledVuH <= 0) return;
  const int totalW = (g_oledVuBarX + g_oledVuW) - g_oledVuX;
  tft.fillRect(g_oledVuX, g_oledVuY - 1, totalW, g_oledVuH + 2, TFT_BLACK);
}

static void oledDrawVuLabels() {
  if (g_oledVuW <= 0) return;
  const int x = g_oledVuLabelX;
  oledTinyDrawText("VU", x, g_oledVuY, OLED_MUTED_GREY_4BPP, 1);
}

static void oledDrawVuTrackRow(int y) {
  if (g_oledVuW <= 0) return;
  const uint8_t trackG = 0x3;
  for (int x = 0; x < g_oledVuW; ++x) {
    tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + x, y, trackG);
    tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + x, y + 1, trackG);
  }
}

static void oledDrawVuBarRow(int y, int oldFillPx, int newFillPx, int oldPeakPx, int newPeakPx) {
  if (g_oledVuW <= 0) return;
  const uint8_t trackG = 0x3;
  const uint8_t fillG  = 0xB;
  const uint8_t peakG  = 0xF;

  oldFillPx = oledVuClamp(oldFillPx, 0, g_oledVuW);
  newFillPx = oledVuClamp(newFillPx, 0, g_oledVuW);
  oldPeakPx = oledVuClamp(oldPeakPx, -1, g_oledVuW - 1);
  newPeakPx = oledVuClamp(newPeakPx, -1, g_oledVuW - 1);

  if (oldFillPx < newFillPx) {
    for (int x = oldFillPx; x < newFillPx; ++x) {
      tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + x, y, fillG);
      tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + x, y + 1, fillG);
    }
  } else if (oldFillPx > newFillPx) {
    for (int x = newFillPx; x < oldFillPx; ++x) {
      tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + x, y, trackG);
      tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + x, y + 1, trackG);
    }
  }

  if (oldPeakPx >= 0 && oldPeakPx < g_oledVuW && oldPeakPx != newPeakPx) {
    const uint8_t restoreG = (oldPeakPx < newFillPx) ? fillG : trackG;
    tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + oldPeakPx, y, restoreG);
    tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + oldPeakPx, y + 1, restoreG);
  }

  if (newPeakPx >= 0 && newPeakPx < g_oledVuW) {
    tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + newPeakPx, y, peakG);
    tft.Jamis_SSD1322::drawPixel(g_oledVuBarX + newPeakPx, y + 1, peakG);
  }
}

static void oledInvalidateVuMeter() {
  g_oledVuLastFillL = -1;
  g_oledVuLastFillR = -1;
  g_oledVuLastPeakL = -1;
  g_oledVuLastPeakR = -1;
}

static void oledDrawVuMeter(uint8_t lvlL, uint8_t lvlR, uint8_t peakL, uint8_t peakR) {
  if (g_oledVuW <= 0 || g_oledVuH <= 0) return;

  const int fillL = map((int)lvlL, 0, 100, 0, g_oledVuW);
  const int fillR = map((int)lvlR, 0, 100, 0, g_oledVuW);
  const int holdL = oledVuClamp(map((int)peakL, 0, 100, 0, g_oledVuW - 1), -1, g_oledVuW - 1);
  const int holdR = oledVuClamp(map((int)peakR, 0, 100, 0, g_oledVuW - 1), -1, g_oledVuW - 1);

  oledClearVuArea();
  oledDrawPufIndicator(g_bufferPercent);
  oledDrawVuLabels();
  oledDrawVuTrackRow(g_oledVuY);
  oledDrawVuTrackRow(g_oledVuY + 3);
  oledDrawVuBarRow(g_oledVuY, 0, fillL, -1, holdL);
  oledDrawVuBarRow(g_oledVuY + 3, 0, fillR, -1, holdR);

  g_oledVuLastFillL = fillL;
  g_oledVuLastFillR = fillR;
  g_oledVuLastPeakL = holdL;
  g_oledVuLastPeakR = holdR;
}

static void oledUpdateVuMeterOnly(uint8_t lvlL, uint8_t lvlR, uint8_t peakL, uint8_t peakR) {
  if (g_oledVuW <= 0 || g_oledVuH <= 0) return;

  static int lastPufPercent = -1;
  static uint32_t lastFlushMs = 0;
  static bool pendingFlush = false;

  const int fillL = map((int)lvlL, 0, 100, 0, g_oledVuW);
  const int fillR = map((int)lvlR, 0, 100, 0, g_oledVuW);
  const int holdL = oledVuClamp(map((int)peakL, 0, 100, 0, g_oledVuW - 1), -1, g_oledVuW - 1);
  const int holdR = oledVuClamp(map((int)peakR, 0, 100, 0, g_oledVuW - 1), -1, g_oledVuW - 1);

  if (lastPufPercent < 0 || abs(g_bufferPercent - lastPufPercent) >= 5) {
    oledDrawPufIndicator(g_bufferPercent);
    lastPufPercent = g_bufferPercent;
    pendingFlush = true;
  }

  if (fillL != g_oledVuLastFillL || fillR != g_oledVuLastFillR || holdL != g_oledVuLastPeakL || holdR != g_oledVuLastPeakR) {
    oledDrawVuBarRow(g_oledVuY,     g_oledVuLastFillL, fillL, g_oledVuLastPeakL, holdL);
    oledDrawVuBarRow(g_oledVuY + 3, g_oledVuLastFillR, fillR, g_oledVuLastPeakR, holdR);

    g_oledVuLastFillL = fillL;
    g_oledVuLastFillR = fillR;
    g_oledVuLastPeakL = holdL;
    g_oledVuLastPeakR = holdR;
    pendingFlush = true;
  }

  if (!pendingFlush) return;

  const uint32_t now = millis();
  if (now - lastFlushMs < 85) return;

  tft.display();
  lastFlushMs = now;
  pendingFlush = false;
}
#endif


#if defined(SSD1322)
static inline int oledBottomBarRowTop() {
  // A downbar alsó pozícióját a hangerő ikon (12 px magas, rowTop-1-re rajzolva)
  // alsó pixeléhez igazítjuk. Így a hangszóró ikon legalja pont a kijelző legaljára kerül.
  // speakerY = rowTop - 1, speakerBottom = speakerY + (12 - 1) = rowTop + 10
  // Finomhangolás alapján a komplett downbar 2 px-szel lejjebb kerül.
  // A gyakorlatban ez adta a helyes vizuális legalja-pozíciót.
  return H - 9;
}
#endif

static void oledUpdateVolumeOnly() {
#if defined(SSD1322)
  const int rowTop = oledBottomBarRowTop();
  const int rowH   = 9;

  const String volText = String(g_Volume);

  const int speakerW = 12;
  const int speakerH = 12;
  const int speakerX = 0;
  const int speakerY = rowTop - 1;

  const int volX = speakerX + speakerW + 4;

  // Csak a bal oldali hangerő-blokkot töröljük és rajzoljuk újra.
  // Hagyunk tartalékot 2 jegy + esetleges későbbi spacing miatt,
  // így nem villog újra az IP / VU / Wi-Fi rész.
  const int volumeRegionW = speakerW + 2 + 12;
  tft.fillRect(0, rowTop, volumeRegionW, rowH, TFT_BLACK);

  drawGrayFromRgb565Bitmap(speakerX, speakerY, speakerW, speakerH, icon_speaker_12);
  applyRegularUiFont(tft, 10);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(top_left);
  tft.drawString(volText, volX, rowTop + 1);
  tft.display();
#endif
}

static void drawOledIpLine() {
#if defined(SSD1322)
  const int rowTop = oledBottomBarRowTop();
  const int rowH   = 9;           // 54..62

  // Az OLED clearRect szándékosan nem törli az alsó sávot, ezért itt közvetlenül törlünk.
  // A legalsó 1 px puffercsíkhoz nem nyúlunk.
  tft.fillRect(0, rowTop, W, rowH, TFT_BLACK);

  const String volText = String(g_Volume);
  const String ipText  = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("-");

  const int speakerW = 12;
  const int speakerH = 12;
  const int speakerX = 0;
  const int speakerY = rowTop - 1;

  // Számoljunk azzal is, hogy a hangerő kétjegyű lehet.
  const int volX = speakerX + speakerW + 4;
  applyRegularUiFont(tft, 10);
  const int volW = tft.textWidth(volText.c_str());
  const int volRightX = volX + volW;

  const int ipGap = 10;
  const int ipX = volRightX + ipGap;
  applyRegularUiFont(tft, 9);
  const int ipLabelW = tft.textWidth("IP:");
  applyRegularUiFont(tft, 10);
  const int ipW = tft.textWidth(ipText.c_str());

  // Wi-Fi ikon fixen jobb szélen, a downbar-on belül.
  const int wifiWLocal = 11;
  const int wifiHLocal = 8;
  const int wifiXLocal = W - wifiWLocal - 2;
  const int wifiYLocal = rowTop + 1;

  drawGrayFromRgb565Bitmap(speakerX, speakerY, speakerW, speakerH, icon_speaker_12);

  applyRegularUiFont(tft, 10);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(top_left);
  tft.drawString(volText, volX, rowTop + 1);

  applyRegularUiFont(tft, 9);
  tft.setTextColor(OLED_MUTED_GREY, TFT_BLACK);
  tft.drawString("IP:", ipX, rowTop + 1);

  applyRegularUiFont(tft, 10);
  tft.setTextColor(OLED_MUTED_GREY, TFT_BLACK);
  tft.drawString(ipText, ipX + ipLabelW + 2, rowTop + 1);

  oledVuComputeLayout(rowTop, wifiXLocal, 3, ipX + ipLabelW + 2 + ipW);
  oledDrawVuMeter(vu_getL(), vu_getR(), vu_getPeakL(), vu_getPeakR());

  int level = 0;
  if (WiFi.status() == WL_CONNECTED) {
    const int rssi = WiFi.RSSI();
    if      (rssi >= -55) level = 4;
    else if (rssi >= -67) level = 3;
    else if (rssi >= -75) level = 2;
    else if (rssi >= -85) level = 1;
    else                  level = 0;
  }

  drawOledWifiBarsDirect(wifiXLocal, wifiYLocal, level);
  tft.display();
#endif
}

static void drawStaticUI() {
  tft.fillScreen(TFT_BLACK);

#if defined(SSD1322)
  // Top row: codec | station | logo
  drawCodecIconTopLeft();
  tft.pushImage(W - OLED_CORNER_LOGO_W, 0, OLED_CORNER_LOGO_W, OLED_CORNER_LOGO_H, logo_rgb565_20x20);

  int wS = sprStation.textWidth(g_stationName.c_str());
  int xS = (wS <= (int)sprStation.width()) ? (((int)sprStation.width() - wS) / 2) : 0;
  renderLine(sprStation, g_stationName, xS);
  sprStation.pushSprite(stationSpriteX(), oledStationRowY());
#else
  tft.loadFont(uiRegularFont(UI_FONT_HEADER).c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String header = text_fix("myRadio");
  drawCodecIconTopLeft();
  ui_drawHeaderAndLogo(header, yHeader, CODEC_ICON_W);

  tft.loadFont(uiRegularFont(UI_FONT_LABEL).c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int wS = sprStation.textWidth(g_stationName.c_str());
  int xS = (wS <= (int)sprStation.width()) ? (((int)sprStation.width() - wS) / 2) : 0;
  renderLine(sprStation, g_stationName, xS);
  sprStation.pushSprite(0, oledStationRowY());
#endif

  drawStreamLabelLine();

  clearRect(0, oledArtistRowY(), W, hArtistLine);
#if defined(SSD1322)
  // Finomhangolás: a title sor törlése / sprite-ja 1 px-szel feljebb került,
  // hogy ne vegyen el a downbar tetejéből.
  clearRect(0, oledTitleClearY(), W, hTitleLine);
#else
  clearRect(0, yTitle,  W, hTitleLine);
#endif
  sprArtist.pushSprite(0, oledArtistRowY());
#if defined(SSD1322)
  sprTitle.pushSprite(0, oledTitleRowY());
  g_oledPauseBadgeDirty = true;
  drawOledPausedBadgeOverlay(true);
#else
  sprTitle.pushSprite(0, yTitle);
#endif

#if defined(SSD1322)
  drawOledIpLine();
#endif
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

#if defined(SSD1322)
  // OLED-en pause alatt fagyasszuk be a marquee-t és a szövegsorok újrarajzolását,
  // különben a hosszú title tovább léptet, felülírja a badge területét,
  // a badge újra-újrarajzolódik és villog.
  if (g_paused) {
    if (g_oledPauseBadgeDirty || g_oledPauseBadgeLastState != g_paused) {
      drawOledPausedBadgeOverlay();
    }
    g_forceRedrawText = false;
    return;
  }
#endif

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
      #if defined(SSD1322)
  sprStation.pushSprite(stationSpriteX(), oledStationRowY());
#else
  sprStation.pushSprite(0, oledStationRowY());
#endif
      g_lastStationDrawn = g_stationName;
      g_lastStationX = xS;
    }
    if (g_forceRedrawText || g_lastArtistDrawn != g_artist || g_lastArtistX != xA) {
      if (g_scrollArtist) renderMarqueeLine(sprArtist, g_mArtist, xA, g_wArtistMarq);
      else                renderLine(sprArtist, g_artist, xA);
      sprArtist.pushSprite(0, oledArtistRowY());
#if defined(SSD1322)
      g_oledPauseBadgeDirty = true;
#endif
      g_lastArtistDrawn = g_artist;
      g_lastArtistX = xA;
    }
    if (g_forceRedrawText || g_lastTitleDrawn != g_title || g_lastTitleX != xT) {
      if (g_scrollTitle) renderMarqueeLine(sprTitle, g_mTitle, xT, g_wTitleMarq);
      else               renderLine(sprTitle, g_title, xT);
#if defined(SSD1322)
      sprTitle.pushSprite(0, oledTitleRowY());
      g_oledPauseBadgeDirty = true;
#else
      sprTitle.pushSprite(0, yTitle);
#endif
      g_lastTitleDrawn = g_title;
      g_lastTitleX = xT;
    }

#if defined(SSD1322)
    if (g_oledPauseBadgeDirty || g_oledPauseBadgeLastState != g_paused) drawOledPausedBadgeOverlay();
#endif
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
    #if defined(SSD1322)
  sprStation.pushSprite(stationSpriteX(), oledStationRowY());
#else
  sprStation.pushSprite(0, oledStationRowY());
#endif
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
    sprArtist.pushSprite(0, oledArtistRowY());
#if defined(SSD1322)
    g_oledPauseBadgeDirty = true;
#endif
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
#if defined(SSD1322)
    sprTitle.pushSprite(0, oledTitleRowY());
    g_oledPauseBadgeDirty = true;
#else
    sprTitle.pushSprite(0, yTitle);
#endif
    g_lastTitleDrawn = g_title;
    g_lastTitleX = xT;
  }

#if defined(SSD1322)
  if (g_oledPauseBadgeDirty || g_oledPauseBadgeLastState != g_paused) drawOledPausedBadgeOverlay();
#endif
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

#if defined(SSD1322)
static void drawOledMenuCounterStatic() {
  applyRegularUiFont(tft, 10);
  const uint16_t chromeColor = TFT_LIGHTGREY;
  tft.setTextColor(chromeColor, TFT_BLACK);

  const int yHeader = 0;
  const int textH = tft.fontHeight();

  if (g_stationCount > 0) {
    const String suffix = String(" / ") + String(g_stationCount);
    const int suffixW = tft.textWidth(suffix.c_str());
    tft.fillRect(W - suffixW - 2, yHeader, suffixW + 2, textH, TFT_BLACK);
    tft.setTextDatum(top_right);
    tft.drawString(suffix, W - 1, yHeader);
    tft.setTextDatum(top_left);
  } else {
    const String noList = String(text_fix(lang::ui_no_list));
    const int noListW = tft.textWidth(noList.c_str());
    const int clearW = min(W, max(noListW + 4, W / 3));
    tft.fillRect(W - clearW, yHeader, clearW, textH, TFT_BLACK);
    tft.setTextDatum(top_right);
    tft.drawString(noList, W - 1, yHeader);
    tft.setTextDatum(top_left);
  }
}

static void drawOledMenuCounterValue() {
  applyRegularUiFont(tft, 10);
  const uint16_t chromeColor = TFT_LIGHTGREY;
  tft.setTextColor(chromeColor, TFT_BLACK);

  const int yHeader = 0;
  const int textH = tft.fontHeight();

  if (g_stationCount > 0) {
    const String suffix = String(" / ") + String(g_stationCount);
    const int suffixW = tft.textWidth(suffix.c_str());
    const String maxValueText = String(g_stationCount);
    const int valueAreaW = tft.textWidth(maxValueText.c_str()) + 4;
    const int valueRightX = W - 1 - suffixW;
    const int valueLeftX = max(0, valueRightX - valueAreaW);
    tft.fillRect(valueLeftX, yHeader, valueRightX - valueLeftX + 1, textH, TFT_BLACK);
    tft.setTextDatum(top_right);
    tft.drawString(String(g_menuIndex + 1), valueRightX, yHeader);
    tft.setTextDatum(top_left);
  } else {
    drawOledMenuCounterStatic();
  }
}

static void drawOledMenuCounter() {
  drawOledMenuCounterStatic();
  drawOledMenuCounterValue();
}

static void drawOledMenuOverlay() {
  applyRegularUiFont(tft, 10);
  const uint16_t chromeColor = TFT_LIGHTGREY;
  tft.setTextColor(chromeColor, TFT_BLACK);

  const int screenH = tft.height();
  const int textH = tft.fontHeight();
  const int yHeader = 0;
  const int yFooter = max(0, screenH - textH);

  tft.fillRect(0, yHeader, W, textH, TFT_BLACK);
  tft.fillRect(0, yFooter, W, textH, TFT_BLACK);

  tft.setTextDatum(top_left);
  tft.drawString(text_fix("Állomások:"), 0, yHeader);
  drawOledMenuCounter();

  tft.setTextDatum(top_left);
  tft.drawString(text_fix("OK: nyom"), 0, yFooter);

  tft.setTextDatum(top_right);
  tft.drawString(text_fix("Kilép: hosszan"), W - 1, yFooter);

  tft.setTextDatum(top_left);
  tft.display();
}
#endif

static void redrawMenuCounterAndList() {
  if (g_mode != MODE_MENU) return;

#if defined(SSD1322)
  drawOledMenuCounterValue();
  drawMenuListArea();
#else
  applyRegularUiFont(tft, UI_FONT_MENU);
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
#endif
}

static void drawMenuScreen() {
#if defined(SSD1322)
  applyRegularUiFont(tft, 10);

  const int screenH = tft.height();
  const int textH = tft.fontHeight();
  const int targetRows = 5;
  const int yHeader = 0;
  const int yFooter = max(0, screenH - textH);

  tft.fillScreen(TFT_BLACK);

  g_menuHeaderY = yHeader;
  g_menuCounterY = yHeader;
  g_menuOkY = yFooter;
  g_menuIpY = -1000;
  g_menuTextH = textH;
  g_menuGap = 0;

  g_menuListTop = 0;
  g_menuListHeight = screenH;
  g_menuItemH = max(1, screenH / targetRows);

  drawOledMenuOverlay();
  drawMenuListArea();
#else
  applyRegularUiFont(tft, UI_FONT_MENU);
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

  applyRegularUiFont(tft, UI_FONT_MENU);
  tft.setCursor(sidePad, yOk);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(lang::ui_ok_exit_hint);

  applyRegularUiFont(tft, UI_FONT_MENU);
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
#endif
}

static void updateMenuNameScroll() {
  ui_stationSelectorTick();
}

static void exitMenuRedrawPlayUI() {
  ui_stationSelectorEnd();
  g_mode = MODE_PLAY;

  tft.setFont((const GFXfont*)nullptr);
  tft.setTextSize(1);
  tft.setTextWrap(false);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // A station selector most teljes képernyős, ezért visszalépéskor a TELJES UI-t újra kell rajzolni,
  // különben menü-maradványok látszanak lejátszás közben.
  clearRect(0, 0, W, H);
  drawStaticUI();

  updateMarquee();

  // A teljes redraw után a VU statikus keretét is újra kell építeni.
#if defined(SSD1322)
  oledInvalidateVuMeter();
  oledDrawVuMeter(vu_getL(), vu_getR(), vu_getPeakL(), vu_getPeakR());
#else
  ui_invalidateVuMeter();
  ui_drawVuMeter(vu_getL(), vu_getR(), vu_getPeakL(), vu_getPeakR());
#endif
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
static int g_touchDragStartX = -1;
static int g_touchDragStartY = -1;
static int g_touchDragAnchorIndex = 0;


static void touchAdjustVolume(int delta) {
  if (delta == 0) return;
  int newVol = g_Volume + delta;
  if (newVol < VOLUME_MIN) newVol = VOLUME_MIN;
  if (newVol > VOLUME_MAX) newVol = VOLUME_MAX;
  if (newVol == g_Volume) return;
  g_Volume = newVol;
  stream_core_sendVolume(g_Volume);
  updateVolumeOnly();
}

static bool touchIsInMenuOkZone(int y) {
  return y >= (g_menuOkY - g_menuGap) && y <= (g_menuOkY + g_menuTextH + g_menuGap);
}

static int touchMenuVisualTop() {
#if defined(SSD1322)
  return g_menuListTop;
#else
  int top = g_menuListTop - 5;
  if (W <= 320 && H <= 240 && g_menuItemH > 0) {
    top -= g_menuItemH;
  }
  return top;
#endif
}

static bool touchIsInMenuListZone(int y) {
  const int top = touchMenuVisualTop();
  return y >= top && y < (top + g_menuListHeight);
}

static int touchMenuRowFromY(int y) {
  if (!touchIsInMenuListZone(y) || g_menuItemH <= 0) return -1;
  const int top = touchMenuVisualTop();
  int row = (y - top) / g_menuItemH;
  if (row < 0) row = 0;
  if (row > 4) row = 4;
  return row;
}

static void onTouchTap(int x, int y) {
  g_touchDragStartX = -1;
  g_touchDragStartY = -1;

  if (g_mode == MODE_PLAY) {
    const int volumeZoneW = max(56, W / 4);
    const int volumeZoneX = W - volumeZoneW;
    if (W > 0 && x >= volumeZoneX) {
      if (y < (H / 2)) touchAdjustVolume(+1);
      else             touchAdjustVolume(-1);
      return;
    }

    togglePaused();
    return;
  }

  if (g_mode != MODE_MENU) return;

  if (touchIsInMenuOkZone(y)) {
    exitMenuRedrawPlayUI();
    return;
  }

  if (g_stationCount <= 0) return;

  const int row = touchMenuRowFromY(y);
  if (row < 0) return;

  const int delta = row - 2;
  if (delta == 0) {
    onButtonShortPress();
  } else {
    ui_stationSelectorRotate(delta);
    redrawMenuCounterAndList();
  }
}

static void onTouchDrag(int startX, int startY, int x, int y) {
  if (g_mode != MODE_MENU || g_stationCount <= 0) return;
  if (!touchIsInMenuListZone(startY)) return;
  if (x < 0 || y < 0) return;

  const int dx = x - startX;
  const int dy = y - startY;
  if (abs(dy) < max(10, g_menuItemH / 4)) return;
  if (abs(dx) > abs(dy)) return;

  if (g_touchDragStartX != startX || g_touchDragStartY != startY) {
    g_touchDragStartX = startX;
    g_touchDragStartY = startY;
    g_touchDragAnchorIndex = g_menuIndex;
  }

  const int stepPx = max(16, (g_menuItemH * 2) / 3);
  const int steps = -dy / stepPx;
  int target = g_touchDragAnchorIndex + steps;

  while (target < 0) target += g_stationCount;
  while (target >= g_stationCount) target -= g_stationCount;

  if (target != g_menuIndex) {
    g_menuIndex = target;
    redrawMenuCounterAndList();
  }
}

static void onTouchLongPress(int x, int y) {
  (void)x; (void)y;
  g_touchDragStartX = -1;
  g_touchDragStartY = -1;
  onButtonLongPress();
}

static void onButtonLongPress() {
  g_touchDragStartX = -1;
  g_touchDragStartY = -1;

  if (g_mode == MODE_PLAY) {
    g_mode = MODE_MENU;
    ui_stationSelectorBegin(g_currentIndex);
#if defined(SSD1322)
    oledInvalidateVuMeter();
#else
    ui_invalidateVuMeter();
#endif
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
      tft.setFont((const GFXfont*)nullptr);
  tft.setTextSize(1);
  tft.setTextWrap(false);
      renderLine(sprArtist, "", 0);
      renderLine(sprTitle,  "", 0);
      sprArtist.pushSprite(0, oledArtistRowY());
#if defined(SSD1322)
      sprTitle.pushSprite(0, oledTitleRowY());
#else
      sprTitle.pushSprite(0, yTitle);
#endif

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

  const String& fontBody  = uiRegularFont(UI_FONT_STREAM);
  const String& fontTitle = uiSemiboldFont(UI_FONT_HEADER);

  const String title = lang::boot_starting_radio;
  const bool connectedNow = (WiFi.status() == WL_CONNECTED);
  String base  = connectedNow ? lang::boot_wifi_connected : lang::boot_wifi_connecting;
  if (!connectedNow && !base.endsWith("...")) base += "...";

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

  { String fp = fontTitle; applyFontPath(tft, &fp); }
  const int hTitle = tft.fontHeight();
  const int wTitle = tft.textWidth(title);

  { String fp = fontBody; applyFontPath(tft, &fp); }
  const int hBody  = tft.fontHeight();
  const int wBase  = tft.textWidth(base);
  const int wDots  = 0;
  const int hSsid  = hBody;
  const int wSsid  = tft.textWidth(ssidLine);
  const int hInfo  = infoLine.length() ? hBody : 0;
  const int wInfo  = infoLine.length() ? tft.textWidth(infoLine) : 0;

  const bool compactOledStartup = (tft.width() == 256 && tft.height() == 64);

  int yTitle = 0;
  int yBody  = 0;
  int ySsid  = 0;
  int yInfo  = 0;
  int xBody  = 0;
  int xSsid  = 0;
  int xInfo  = 0;

  if (compactOledStartup) {
    const int boxY    = tft.height() - 34;
    const int lineGap = 1;
    const int totalH  = hBody + lineGap + hSsid + (infoLine.length() ? (lineGap + hInfo) : 0);

    yBody = boxY + (34 - totalH) / 2;
    ySsid = yBody + hBody + lineGap;
    yInfo = ySsid + hSsid + lineGap;

    xBody = (tft.width() - (wBase + wDots)) / 2;
    xSsid = (tft.width() - wSsid) / 2;
    xInfo = (tft.width() - wInfo) / 2;
  } else {
    const int gap1 = 8;
    const int gap2 = 10;
    const int gap3 = infoLine.length() ? 6 : 0;
    const int totalH = hTitle + gap1 + hBody + gap2 + hSsid + (infoLine.length() ? (gap3 + hInfo) : 0);

    yTitle = (tft.height() - totalH) / 2;
    yBody  = yTitle + hTitle + gap1;
    ySsid  = yBody + hBody + gap2;
    yInfo  = ySsid + hSsid + gap3;

    xBody = (tft.width() - (wBase + wDots)) / 2;
    xSsid = (tft.width() - wSsid) / 2;
    xInfo = (tft.width() - wInfo) / 2;
  }

  if (!compactOledStartup) {
    { String fp = fontTitle; applyFontPath(tft, &fp); }
    tft.setCursor((tft.width() - wTitle) / 2, yTitle);
    tft.print(title);
  }

  { String fp = fontBody; applyFontPath(tft, &fp); }
  tft.setCursor(xBody, yBody);
  tft.print(base);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
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


  Serial.begin(460800);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  Serial0.begin(460800);
  serial_spiffs_begin(Serial, &Serial0);
#else
  serial_spiffs_begin(Serial);
#endif
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
#if defined(SSD1322)
  tft.setFont((const GFXfont*)nullptr);
  tft.setTextSize(1);
  tft.setTextWrap(false);
#else
  tft.setFont((const GFXfont*)nullptr);
  tft.setTextSize(1);
  tft.setTextWrap(false);
#endif

// Boot logo 3 másodpercig
{
#if defined(SSD1322)
  const int logoW = MYRADIO_LOGO_OLED_200X30_W;
  const int logoH = MYRADIO_LOGO_OLED_200X30_H;
  const int x = (tft.width()  - logoW) / 2;
  const int y = (tft.height() - logoH) / 2;

  tft.fillScreen(TFT_BLACK);
  drawGray4Bitmap(x, y, logoW, logoH, myradio_logo_oled_200x30);
#else
  const int logoW = LOGO_WIDTH;
  const int logoH = LOGO_HEIGHT;
  const int x = (tft.width()  - logoW) / 2;
  const int y = (tft.height() - logoH) / 2;

  tft.fillScreen(TFT_BLACK);
  tft.pushImage(x, y, logoW, logoH, myradiologo_240);
#endif
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


  // Serial SPIFFS maintenance pre-portal window:
  // This must happen BEFORE wifi_manager_begin_or_portal(), because that path can
  // become blocking when SPIFFS is empty and the device falls into Wi-Fi setup.
  {
    const uint32_t maintWindowStart = millis();
    while (millis() - maintWindowStart < 3000) {
      serial_spiffs_poll();
      if (serial_spiffs_is_active()) {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextDatum(top_left);
        tft.setFont((const GFXfont*)nullptr);
        tft.setTextSize(1);
        tft.drawString("Serial maintenance mode", 6, 6);
        tft.drawString("SPIFFS access active", 6, 22);
        Serial.println("[MRSPIFS] pre-portal maintenance override");
        return;
      }
      delay(10);
    }
  }

  wifi_manager_begin_or_portal();
}

  
uint8_t phase = 0;
while (WiFi.status() != WL_CONNECTED) {
  serial_spiffs_poll();
  if (serial_spiffs_is_active()) {
    g_startupConnectScreenActive = false;
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(top_left);
    tft.setFont((const GFXfont*)nullptr);
    tft.setTextSize(1);
    tft.drawString("Serial maintenance mode", 6, 6);
    tft.drawString("SPIFFS access active", 6, 22);
    Serial.println("[MRSPIFS] startup WiFi wait overridden by serial maintenance");
    return;
  }

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

#if defined(SSD1322)
    // Font paths (LGFX loadFont) - smaller OLED baseline
    u.FP_20    = uiRegularFontPtr(10);
    u.FP_SB_20 = uiSemiboldFontPtr(12);
#else
    // Font paths (LGFX loadFont)
    u.FP_20    = uiRegularFontPtr(UI_FONT_LABEL);
    u.FP_SB_20 = uiSemiboldFontPtr(20);
#endif

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
    sctx.screenH = &H;
#if defined(SSD1322)
    sctx.labelFontPath = uiRegularFontPtr(9);
    sctx.activeFontPath = uiRegularFontPtr(10);
#else
    sctx.labelFontPath = uiRegularFontPtr(UI_FONT_LABEL);
    sctx.activeFontPath = uiSemiboldFontPtr(24);
#endif
    sctx.menuScrollIntervalMs = MENU_MS;
    sctx.colorBg = TFT_BLACK;
    sctx.colorActiveBg = TFT_WHITE;
    sctx.colorActiveBorder = TFT_GOLD;
    sctx.colorActiveText = TFT_WHITE;
    sctx.colorSideText = 0x8C51;
    sctx.colorSideFarText = 0x4208;
    ui_stationSelectorInit(sctx);
  }

  logMemorySnapshot("after sprites");
  drawStaticUI();

  // VU meter init (audio hook fogja etetni)
  vu_init();
#if defined(SSD1322)
  oledInvalidateVuMeter();
  drawOledIpLine();
#else
  ui_drawVuMeter(0, 0, 0, 0);
#endif

  if (I2S_MCLK >= 0) audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  else               audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);

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
  serial_spiffs_poll();
  if (serial_spiffs_is_active()) {
    delay(2);
    return;
  }

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

  InputTouchCtx tctx;
  tctx.enabled = &g_touchEnabled;
  tctx.state = &g_touchState;
  tctx.screenW = W;
  tctx.screenH = H;
  tctx.onTap = onTouchTap;
  tctx.onLongPress = onTouchLongPress;
  tctx.onDrag = onTouchDrag;
  input_touch_apply(tctx);
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

  // ---- Buffer kijelzés (OLED-en ritkított frissítéssel) ----
  static uint32_t lastBufferPollMs = 0;
  uint32_t nowBuf = millis();
  if (g_mode == MODE_PLAY && !ui_stationSelectorActive() && (nowBuf - lastBufferPollMs >= 2000)) {
    lastBufferPollMs = nowBuf;
    StreamWatchdogBufferCtx bctx{};
    bctx.stationUrl = &g_stationUrl;
    bctx.lastBufferCheckMs = &lastBufferCheck;
    bctx.bufferFilled = &g_bufferFilled;
    bctx.bufferFree = &g_bufferFree;
    bctx.bufferTotal = &g_bufferTotal;
    bctx.bufferPercent = &g_bufferPercent;
    bctx.readBufferFilledFn = []() -> size_t { return audio.inBufferFilled(); };
    bctx.readBufferFreeFn = []() -> size_t { return audio.inBufferFree(); };
    bctx.needStreamReconnect = &g_needStreamReconnect;
    bctx.paused = &g_paused;
    bctx.connectRequestedAtMs = &g_connectRequestedAt;
    bctx.refreshMs = 0;
    bctx.startupGraceMs = 6000;
    bctx.lowBufferHoldMs = 7000;
    bctx.lowBufferPercent = 8;
    stream_watchdog_updateBuffer(bctx);
#if !defined(SSD1322)
    updateBufferIndicatorOnly();
#endif
  }

  // ---- VU meter frissítés (UI oldalon) ----
  static uint32_t lastVuMs = 0;
  uint32_t nowVu = millis();
#if defined(SSD1322)
  const uint32_t vuUiIntervalMs = 85;
#else
  const uint32_t vuUiIntervalMs = 100;
#endif
  if (g_mode == MODE_PLAY && (nowVu - lastVuMs >= vuUiIntervalMs)) {
    lastVuMs = nowVu;
#if defined(SSD1322)
    oledUpdateVuMeterOnly(vu_getL(), vu_getR(), vu_getPeakL(), vu_getPeakR());
#else
    ui_updateVuMeterOnly(vu_getL(), vu_getR(), vu_getPeakL(), vu_getPeakR());
#endif
  }

#if !defined(SSD1322)
  static uint32_t lastBottomUiTickMs = 0;
  uint32_t nowBottomUi = millis();
  if (g_mode == MODE_PLAY && !ui_stationSelectorActive() && (nowBottomUi - lastBottomUiTickMs >= 1500)) {
    lastBottomUiTickMs = nowBottomUi;
    ui_updateWifiIconOnly();
  }
#endif

  delay(1);
}
