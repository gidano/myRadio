// app_impl.cpp
#include <Arduino.h>
#include <WiFi.h>
#include "net_server.h"
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
#include "input_rotary.h"
#include "state_meta.h"
#include "ui_display.h"

// VU meter hooks (audio_process_i2s -> vu_feedStereoISR)
#include "vu_meter.h"


// Fallback színek, ha a platform nem definiálná ezeket a neveket
#ifndef TFT_LIGHTGREY
  #define TFT_LIGHTGREY 0xC618
#endif
#ifndef TFT_DARKGREY
  #define TFT_DARKGREY  0x7BEF
#endif

using LGFX_Sprite = lgfx::LGFX_Sprite;

// ------------------ Forward declarations (needed because some helpers are static) ------------------
static void saveLastStationToNVS();
static void saveLastStationToSPIFFS();
static bool loadLastStationFromSPIFFS(String& url, String& name);
static void renderLine(LGFX_Sprite& spr, const String& text, int32_t x);
static void recalcTextMetrics();
static void reserveHotStrings();
static void logMemorySnapshot(const char* tag);
static void serialLogf(const char* fmt, ...);
static void serialLogln(const String& s);
static void serialLogln(const char* s);

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




// ------------------ PIN KIOSZTÁS ------------------ //
#define TFT_DC    9
#define TFT_RST   8
#define TFT_CS    10
#define TFT_BL    7
// Backlight PWM (Arduino-ESP32 v2/v3 kompatibilis)
#define BL_PWM_FREQ  5000
#define BL_PWM_RES   8
#define BL_PWM_CH    0

static uint8_t g_brightness = 255;

static void bl_init_pwm() {
  pinMode(TFT_BL, OUTPUT);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // v3.x: csatorna kezelést a core intézi
  ledcAttach(TFT_BL, BL_PWM_FREQ, BL_PWM_RES);
  ledcWrite(TFT_BL, g_brightness);
#else
  ledcSetup(BL_PWM_CH, BL_PWM_FREQ, BL_PWM_RES);
  ledcAttachPin(TFT_BL, BL_PWM_CH);
  ledcWrite(BL_PWM_CH, g_brightness);
#endif
}

static void bl_set(uint8_t v) {
  g_brightness = v;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(TFT_BL, g_brightness);
#else
  ledcWrite(BL_PWM_CH, g_brightness);
#endif
}
#define TFT_MOSI  11
#define TFT_SCLK  12

#define I2S_DOUT  5
#define I2S_BCLK  4
#define I2S_LRC   6
#define I2S_MCLK 15

#define ENC_A     3
#define ENC_B     1
#define ENC_BTN   2   // enkóder nyomógomb (PULLUP)

// ---- Encoder (interruptos, érzékenyebb / nem veszít lépést) ----
#ifndef ENC_PULSES_PER_STEP
// 4 = tipikus (1 detent = 1 volume lépés), 2 = érzékenyebb (1 detent = 2 lépés)
#define ENC_PULSES_PER_STEP 4
#endif

volatile int32_t g_encDelta = 0;
volatile uint8_t g_encHist = 0;

static const int8_t ENC_LUT[16] = {
  0, -1,  1,  0,
  1,  0,  0, -1,
 -1,  0,  0,  1,
  0,  1, -1,  0
};

void IRAM_ATTR encoderISR() {
  uint8_t s = (uint8_t)((digitalRead(ENC_A) ? 1 : 0) << 1) | (uint8_t)(digitalRead(ENC_B) ? 1 : 0);
  g_encHist = (uint8_t)((g_encHist << 2) | s);
  g_encDelta += ENC_LUT[g_encHist & 0x0F];
}
// ---------------------------------------------------------------


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

// WiFi (SPIFFS-ben tárolt SSID/Jelszó)
static const char* WIFI_CRED_FILE = "/wifi.txt"; // 1. sor SSID, 2. sor PASS

static String g_savedSsid;
static String g_savedPass;
static bool   g_haveWiFiCreds = false;

// Futás közbeni reconnect (backoff)
static bool     g_wifiWasConnected    = false;
static uint32_t g_wifiDownSince       = 0;
static uint32_t g_wifiLastAttempt     = 0;
static uint32_t g_wifiAttemptInterval = 5000;   // ms
static uint32_t g_wifiAttemptCount    = 0;
static uint32_t g_connectRequestedAt = 0;
static bool     g_needStreamReconnect = false;

// Ha 0: csak próbálkozik, nem dob portált automatikusan
#define WIFI_FALLBACK_TO_PORTAL 0
static const uint32_t WIFI_RETRY_TO_PORTAL_MS = 120000; // 2 perc


static uint32_t g_wifiConnectedAt = 0; // millis() when WiFi connected

// NVS (flash) - utolsó állomás megjegyzése
Preferences prefs;

// Default station (ha nincs file / üres)
static String g_stationName = "Zebrádió";
static String g_stationUrl  = "https://stream.zebradio.hu:8443/zebradio";


// ---- Playlist (M3U) support for local PC server ----
static std::vector<String> g_playlistUrls;
static int g_playlistIndex = -1;
static String g_playlistSourceUrl = "";   // the .m3u URL that produced g_playlistUrls
static String g_playUrl = "";             
// Last URL we asked the audio task to connect to (for watchdog recovery)
static String g_lastConnectUrl = "";
// the actual currently-playing URL (track or stream)


// Auto-advance for M3U playlists (when a track ends)
static volatile bool g_autoNextRequested = false;
static volatile uint32_t g_autoNextRequestedAt = 0;
int g_Volume = 5;

// lejátszás szüneteltetése (enkóder rövid nyomás)
static bool g_paused = false;

// Web server
WebServer server(80);

// ------------------ SPIFFS Web Uploader (always available) ------------------ //
static File   g_uploadFile;
static String g_uploadPath;

static String sanitizePath(String p) {
  p.trim();
  if (p.length() == 0) return String("/");
  if (!p.startsWith("/")) p = "/" + p;
  // collapse backslashes
  p.replace("\\", "/");
  // remove .. segments (basic safety)
  while (p.indexOf("..") >= 0) p.replace("..", "");
  // collapse double slashes
  while (p.indexOf("//") >= 0) p.replace("//", "/");
  return p;
}

static void ensureParentDirs(const String& fullPath) {
  int slash = fullPath.lastIndexOf('/');
  if (slash <= 0) return; // "/" or no parent
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

static const char UPLOAD_PAGE_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>SPIFFS feltöltés</title>
<style>
body{font-family:system-ui,Arial;margin:16px;background:#111;color:#eee}
.card{max-width:640px;margin:0 auto;padding:16px;border:1px solid #333;border-radius:12px;background:#161616}
input,button{width:100%;padding:10px;margin:8px 0;border-radius:10px;border:1px solid #333;background:#0f0f0f;color:#eee}
button{background:#2b6cff;border:0;font-weight:600}
small{color:#aaa}
a{color:#7db1ff}
</style>
</head><body>
<div class="card">
<h2>Fájl feltöltés (SPIFFS)</h2>
<p><small>Ha még nincs web UI feltöltve, először töltsd fel ide: <b>/web/index.html</b></small></p>
<form method="POST" action="/upload" enctype="multipart/form-data">
<label>Cél útvonal a SPIFFS-en (pl. /web/index.html, /fonts/test_24.vlw)</label>
<input name="path" placeholder="/web/index.html" required />
<label>Fájl</label>
<input type="file" name="file" required />
<button type="submit">Feltöltés</button>
</form>
<p><a href="/api/fs/list">Fájllista (JSON)</a></p>
<p><a href="/">Vissza a rádióhoz</a></p>
</div>
</body></html>
)HTML";

static void handleUploadPage() {
  server.send(200, "text/html; charset=utf-8", FPSTR(UPLOAD_PAGE_HTML));
}

static void handleFsList() {
  String json = "[";
  bool first = true;

  File root = SPIFFS.open("/");
  if (!root) { server.send(500, "text/plain", "FS open failed"); return; }
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

static void handleUploadDone() {
  server.send(200, "text/plain; charset=utf-8", "OK");
}

static void handleFileUpload() {
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
static bool g_webClientConnected = false;

// Web reboot (reset) request flag (so the HTTP response can be sent before reboot)
static volatile bool g_restartRequested = false;
static volatile uint32_t g_restartAtMs = 0;
static String g_webResponseBuffer = "";

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

  // Címsor: SemiBold 20
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.loadFont(fontTitle);
  tft.setCursor(x + 60, y);
  tft.println("A WiFi beállításáról:");
  y += tft.fontHeight() + 6;

  // Törzsszöveg: 20
  tft.loadFont(fontBody);

  // 1)
  tft.setCursor(x, y);
  tft.println("1) Csatlakozz a WiFi-hez:");
  y += tft.fontHeight() + 1;
  tft.setCursor(x + 18, y);
  tft.print(" ");
  tft.println(apSsid);
  y += tft.fontHeight() + 3;

  // 2)
  tft.setCursor(x, y);
  tft.println("2) Böngészőben nyisd meg:");
  y += tft.fontHeight() + 1;
  tft.setCursor(x + 22, y);
  tft.print("http://");
  tft.println(ip.toString());
  y += tft.fontHeight() + 3;

  // 3)
  tft.setCursor(x, y);
  tft.println("3) SSID + jelszó páros, majd");
  y += tft.fontHeight() + 1;
  tft.setCursor(x + 22, y);
  tft.println("nyomj a Mentés-re");
  y += tft.fontHeight() + 3;

  // Footer
  tft.setCursor(x, y);
  tft.setCursor(x + 68, y);
  tft.println("A rádió újraindul!");
  y += tft.fontHeight() + 1;

  // Ha nem kell tovább, vissza lehet később váltani másik fontra máshol.
  // (Nem unloadFont-olunk, mert a UI is VLW-t használ.)
}

Audio audio;


Rotary EncL = Rotary(ENC_A, ENC_B);

// ------------------ Audio task (queue, no mutex) ------------------ //
TaskHandle_t audioTaskHandle = nullptr;

// Audio parancsok: csak az audioTask nyúl az Audio objektumhoz.
enum AudioCmdType : uint8_t { ACMD_SET_VOL, ACMD_CONNECT_URL, ACMD_STOP };

struct AudioCmd {
  AudioCmdType type;
  uint8_t vol;
  char url[256];
};

static QueueHandle_t audioQ = nullptr;

static inline void audioSendVolume(uint8_t v) {
  if (!audioQ) return;
  AudioCmd c{};
  c.type = ACMD_SET_VOL;
  c.vol = v;
  xQueueSend(audioQ, &c, 0);
}

static inline void audioSendStop() {
  if (!audioQ) return;
  AudioCmd c{};
  c.type = ACMD_STOP;
  xQueueSend(audioQ, &c, 0);
}

static inline void audioSendConnect(const String& u) {
  g_lastConnectUrl = u;
  g_connectRequestedAt = millis();
  if (!audioQ) return;
  AudioCmd c{};
  c.type = ACMD_CONNECT_URL;
  strlcpy(c.url, u.c_str(), sizeof(c.url));
  xQueueSend(audioQ, &c, 0);
}

void audioTask(void* param) {
  AudioCmd cmd;

  // ---- Audio watchdog / underrun diagnostics ----
  uint32_t lastFilledNonZeroAt = millis();
  uint32_t lastUnderrunPrintAt = 0;
  uint32_t lastRecoveryAt      = 0;
  uint32_t emptySince          = 0;
  uint32_t underrunCount       = 0;
  const uint32_t connectGraceMs = 2500;

  for (;;) {
    // parancsok gyors ledolgozása
    while (audioQ && xQueueReceive(audioQ, &cmd, 0) == pdTRUE) {
      if (cmd.type == ACMD_SET_VOL) {
        audio.setVolume(cmd.vol);
      } else if (cmd.type == ACMD_STOP) {
        audio.stopSong();
      } else if (cmd.type == ACMD_CONNECT_URL) {
        emptySince = 0;
        lastFilledNonZeroAt = millis();
        lastUnderrunPrintAt = millis();
        serialLogf("[AUDIO] Connecting to: %s\n", cmd.url);

        // HTTP vs HTTPS ellenőrzés
        if (strncmp(cmd.url, "https://", 8) == 0) {
          serialLogln("[AUDIO] HTTPS URL detected");
        } else {
          serialLogln("[AUDIO] HTTP URL detected");
        }
        
        audio.connecttohost(cmd.url);
      }
    }

    audio.loop();

        // Ha éppen kapcsolódni próbálunk, de még nem sikerült
    if (g_connectRequestedAt > 0 && !audio.isRunning()) {
      uint32_t elapsed = millis() - g_connectRequestedAt;
      if (elapsed > 3000 && elapsed < 10000) {
        static uint32_t lastPrint = 0;
        if (millis() - lastPrint > 2000) {
          lastPrint = millis();
          serialLogf("[AUDIO] Még mindig kapcsolódik... (%lu mp)\n", elapsed/1000);
        }
      }
    }

    #if AUDIO_WATCHDOG
    // --- Watchdog: detect prolonged empty input buffer while running ---
    // This is intentionally conservative: it should NEVER trigger during normal playback.
    if (audio.isRunning()) {
      const size_t filled = audio.inBufferFilled();

      if (filled > 0) {
        lastFilledNonZeroAt = millis();
        emptySince = 0;
      } else {
        if (emptySince == 0) emptySince = millis();

        // Underrun edge logging: station váltás után adjunk rövid türelmi időt,
        // hogy a természetes reconnect ne szemetelje tele a logot.
        const uint32_t now = millis();
        const bool inConnectGrace = (g_connectRequestedAt > 0) && ((now - g_connectRequestedAt) < connectGraceMs);
        if (!inConnectGrace && now - lastUnderrunPrintAt > 2000) {
          underrunCount++;
          lastUnderrunPrintAt = now;
          serialLogf("[AUDIO] underrun: inBufferFilled=0 (count=%lu)\n", (unsigned long)underrunCount);
        }

        // Ha üres a buffer, de még kapcsolódunk, ne csináljunk semmit
        if ((now - emptySince) > 8000 && (now - lastRecoveryAt) > 30000 && g_lastConnectUrl.length()) {
          // Csak akkor próbáljunk újra, ha a kapcsolódás óta eltelt már legalább 10 másodperc
          if (g_connectRequestedAt == 0 || (now - g_connectRequestedAt) > 10000) {
            lastRecoveryAt = now;
            serialLogf("[AUDIO] watchdog recovery: reconnecting to %s\n", g_lastConnectUrl.c_str());
            audio.stopSong();
            vTaskDelay(pdMS_TO_TICKS(20));
            audio.connecttohost(g_lastConnectUrl.c_str());
            g_connectRequestedAt = millis();
          }
        }
      }
    } else {
      // reset when not running
      emptySince = 0;
    }

    #endif

    vTaskDelay(1);
  }
}

// ------------------ Sprites ------------------ //

LGFX_Sprite sprStation(&tft);
LGFX_Sprite sprArtist(&tft);
LGFX_Sprite sprTitle(&tft);
LGFX_Sprite sprMenu(&tft);

// ------------------ Fontok (SPIFFS) ------------------ //
// FONT FÁJLOK A SPIFFS-BEN:
//  - /fonts/test_XX.vlw  (és /fonts/test_sb_XX.vlw)

// SPIFFS útvonal (SPIFFS API-hoz: exists/open)
static String FS_20, FS_24, FS_28, FS_36, FS_42, FS_48, FS_54;
static String FS_SB_20, FS_SB_24, FS_SB_28, FS_SB_36;

// LGFX útvonal (LGFX loadFont-hoz: /spiffs/...)
static String FP_20, FP_24, FP_28, FP_36, FP_42, FP_48, FP_54;
static String FP_SB_20, FP_SB_24, FP_SB_28, FP_SB_36;

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
  FS_36 = resolveFSPath("test_36.vlw");
  FS_42 = resolveFSPath("test_42.vlw");
  FS_48 = resolveFSPath("test_48.vlw");
  FS_54 = resolveFSPath("test_54.vlw");

  // SemiBold
  FS_SB_20 = resolveFSPath("test_sb_20.vlw");
  FS_SB_24 = resolveFSPath("test_sb_24.vlw");
  FS_SB_28 = resolveFSPath("test_sb_28.vlw");
  FS_SB_36 = resolveFSPath("test_sb_36.vlw");

  // LGFX pathok
  FP_20 = toLGFXPath(FS_20);
  FP_24 = toLGFXPath(FS_24);
  FP_28 = toLGFXPath(FS_28);
  FP_36 = toLGFXPath(FS_36);
  FP_42 = toLGFXPath(FS_42);
  FP_48 = toLGFXPath(FS_48);
  FP_54 = toLGFXPath(FS_54);

  FP_SB_20 = toLGFXPath(FS_SB_20);
  FP_SB_24 = toLGFXPath(FS_SB_24);
  FP_SB_28 = toLGFXPath(FS_SB_28);
  FP_SB_36 = toLGFXPath(FS_SB_36);

  Serial.println("[FONT] SPIFFS files:");
  Serial.printf("  24: %s (%s)\n", FS_24.c_str(), SPIFFS.exists(FS_24) ? "OK" : "MISSING");
  Serial.printf("  SB24: %s (%s)\n", FS_SB_24.c_str(), SPIFFS.exists(FS_SB_24) ? "OK" : "MISSING");
  Serial.println("[FONT] LGFX loadFont paths:");
  Serial.printf("  24: %s\n", FP_24.c_str());
  Serial.printf("  SB24: %s\n", FP_SB_24.c_str());
}
// ------------------ Encoding fix (UTF-8 / Latin-2) ------------------ //
static void appendUTF8(String &out, uint16_t cp) {
  if (cp < 0x80) out += (char)cp;
  else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
  else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
}

static bool looks_like_utf8(const char* s) {
  const uint8_t* p = (const uint8_t*)s;
  while (*p) {
    if (*p < 0x80) { p++; continue; }
    if ((*p & 0xE0) == 0xC0) { if ((p[1] & 0xC0) != 0x80) return false; p += 2; continue; }
    if ((*p & 0xF0) == 0xE0) { if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return false; p += 3; continue; }
    if ((*p & 0xF8) == 0xF0) { if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80) return false; p += 4; continue; }
    return false;
  }
  return true;
}

static const uint16_t latin2_00A0_FF[96] = {
  0x00A0,0x0104,0x02D8,0x0141,0x00A4,0x013D,0x015A,0x00A7,0x00A8,0x0160,0x015E,0x0164,0x0179,0x00AD,0x017D,0x017B,
  0x00B0,0x0105,0x02DB,0x0142,0x00B4,0x013E,0x015B,0x02C7,0x00B8,0x0161,0x015F,0x0165,0x017A,0x02DD,0x017E,0x017C,
  0x0154,0x00C1,0x00C2,0x0102,0x00C4,0x0139,0x0106,0x00C7,0x010C,0x00C9,0x0118,0x00CB,0x011A,0x00CD,0x00CE,0x010E,
  0x0110,0x0143,0x0147,0x00D3,0x00D4,0x0150,0x00D6,0x00D7,0x0158,0x016E,0x00DA,0x0170,0x00DC,0x00DD,0x0162,0x00DF,
  0x0155,0x00E1,0x00E2,0x0103,0x00E4,0x013A,0x0107,0x00E7,0x010D,0x00E9,0x0119,0x00EB,0x011B,0x00ED,0x00EE,0x010F,
  0x0111,0x0144,0x0148,0x00F3,0x00F4,0x0151,0x00F6,0x00F7,0x0159,0x016F,0x00FA,0x0171,0x00FC,0x00FD,0x0163,0x02D9
};

static String latin2_to_utf8(const char* in) {
  String out;
  while (*in) {
    uint8_t c = (uint8_t)*in++;
    if (c < 0x80) { out += (char)c; continue; }
    if (c >= 0xA0) { appendUTF8(out, latin2_00A0_FF[c - 0xA0]); continue; }
    if (c == 0x96) { appendUTF8(out, 0x2013); continue; }
    if (c == 0x97) { appendUTF8(out, 0x2014); continue; }
    if (c == 0x85) { appendUTF8(out, 0x2026); continue; }
    if (c == 0x91) { appendUTF8(out, 0x2018); continue; }
    if (c == 0x92) { appendUTF8(out, 0x2019); continue; }
    if (c == 0x93) { appendUTF8(out, 0x201C); continue; }
    if (c == 0x94) { appendUTF8(out, 0x201D); continue; }
    if (c == 0x84) { appendUTF8(out, 0x201E); continue; }
  }
  return out;
}

static String fixText(const char* s) {
  if (!s) return "";
  if (looks_like_utf8(s)) return String(s);
  return latin2_to_utf8(s);
}

// ------------------ Station list ------------------ //
struct Station { String name; String url; };
static const int MAX_STATIONS = 120;
static Station g_stations[MAX_STATIONS];
static int g_stationCount = 0;
static int g_currentIndex = 0;
static int g_menuIndex = 0;
static int g_menuNameY = 0; // menu sprite Y (auto-layout)

static bool parseStationLine(const String& lineRaw, String& name, String& url) {
  String line = lineRaw;
  line.trim();
  if (line.length() == 0) return false;
  if (line[0] == '#') return false;

  int sep = line.indexOf('\t');
  if (sep < 0) sep = line.indexOf('|');
  if (sep < 0) return false;

  name = line.substring(0, sep);  name.trim();
  url  = line.substring(sep + 1); url.trim();
  return (name.length() && url.length());
}

static void loadStationsFromSPIFFS() {
  g_stationCount = 0;
  File f = SPIFFS.open("/stations.txt", "r");
  if (!f) {
    Serial.println("Nincs /stations.txt (SPIFFS). Default station marad.");
    return;
  }
  while (f.available() && g_stationCount < MAX_STATIONS) {
    String line = f.readStringUntil('\n');
    String name, url;
    if (parseStationLine(line, name, url)) {
      g_stations[g_stationCount].name = name;
      g_stations[g_stationCount].url  = url;
      g_stationCount++;
    }
  }
  f.close();

  Serial.printf("Stations loaded: %d\n", g_stationCount);

  for (int i = 0; i < g_stationCount; i++) {
    if (g_stations[i].url == g_stationUrl || g_stations[i].name == g_stationName) {
      g_currentIndex = i;
      break;
    }
  }
  g_menuIndex = g_currentIndex;

  if (g_stationCount > 0) {
    g_stationName = g_stations[g_currentIndex].name;
    g_stationUrl  = g_stations[g_currentIndex].url;

          saveLastStationToNVS();
          saveLastStationToSPIFFS();
  }
}

static bool saveStationsToSPIFFS() {
  File f = SPIFFS.open("/stations.txt", "w");
  if (!f) {
    Serial.println("[SPIFFS] Cannot open /stations.txt for write");
    return false;
  }
  for (int i = 0; i < g_stationCount; i++) {
    // name \t url \n
    String name = g_stations[i].name;
    name.replace('\t', ' '); // separator safety
    f.print(name);
    f.print("\t");
    f.println(g_stations[i].url);
  }
  f.close();
  Serial.println("[SPIFFS] Stations saved to /stations.txt");
  return true;
}


// ------------------ Last station persistence (NVS) ------------------ //
static void loadLastStationFromNVS() {
  // Olvassuk ki a legutóbbi URL-t; ha megvan, próbáljuk megkeresni a stations listában.
  prefs.begin("myradio", true);
  String lastUrl  = prefs.getString("url", "");
  String lastName = prefs.getString("name", "");
  prefs.end();

  if (lastUrl.length() == 0 && lastName.length() == 0) {
    // NVS üres lehet (pl. full flash erase feltöltéskor). Próbáljuk SPIFFS fájlból.
    String u2, n2;
    if (loadLastStationFromSPIFFS(u2, n2)) {
      lastUrl = u2;
      lastName = n2;
      Serial.println("[SPIFFS] Last station loaded from /last_station.txt");
    } else {
      return;
    }
  }

  // Ha van lista, keressünk benne URL vagy név alapján.
  if (g_stationCount > 0) {
    for (int i = 0; i < g_stationCount; i++) {
      if ((lastUrl.length() && g_stations[i].url == lastUrl) ||
          (lastName.length() && g_stations[i].name == lastName)) {
        g_currentIndex = i;
        g_menuIndex = i;
        g_stationName = g_stations[i].name;
        g_stationUrl  = g_stations[i].url;
        Serial.printf("[NVS] Last station restored: %s\n", g_stationName.c_str());
        return;
      }
    }
  }

  // Ha nincs lista (vagy nem találjuk), de van URL, akkor legalább az URL-t használjuk.
  if (lastUrl.length()) {
    g_stationUrl = lastUrl;
    if (lastName.length()) g_stationName = lastName;
    Serial.printf("[NVS] Last station URL restored (no match in list): %s\n", g_stationUrl.c_str());
  }
}

static void saveLastStationToNVS() {
  prefs.begin("myradio", false);
  prefs.putString("url", g_stationUrl);
  prefs.putString("name", g_stationName);
  prefs.end();
}

// ------------------ Last station persistence (SPIFFS) ------------------ //
static const char* LAST_STATION_FILE = "/last_station.txt";
// Formátum: első sor = URL, második sor = NAME (UTF-8)
static void saveLastStationToSPIFFS() {
  File f = SPIFFS.open(LAST_STATION_FILE, "w");
  if (!f) { Serial.println("[SPIFFS] Cannot open last_station.txt for write"); return; }
  f.println(g_stationUrl);
  f.println(g_stationName);
  f.close();
  Serial.println("[SPIFFS] Last station saved to /last_station.txt");
}

static bool loadLastStationFromSPIFFS(String& outUrl, String& outName) {
  File f = SPIFFS.open(LAST_STATION_FILE, "r");
  if (!f) return false;
  outUrl = f.readStringUntil('\n'); outUrl.trim();
  outName = f.readStringUntil('\n'); outName.trim();
  f.close();
  return (outUrl.length() || outName.length());
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
static String g_codec = "";          // "FLAC" / "MP3" / "OGG" / "AAC" / "OPUS"
static int    g_bitrateK = 0;        // kbps (kbit/s)
static String g_pendingCodec = "";
static int    g_pendingBitrateK = 0;


// ------------------ Codec ikon (60x60 RGB565) ------------------ //
// Várjuk, hogy az ikon .h fájlokban *egyedi* tömbnevek legyenek.
// Ha a Marlin konverter mindegyikben ugyanazt a nevet generálta (pl. image_data_60x60x16),
// akkor nevezd át őket ezekre:
//   image_data_aac_60x60x16, image_data_flac_60x60x16, image_data_mp3_60x60x16,
//   image_data_ogg_60x60x16, image_data_opus_60x60x16
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
  if (u.indexOf("VORBIS") >= 0) return image_data_ogg_60x60x16;
  if (u.indexOf("OGG") >= 0) return image_data_ogg_60x60x16;
  if (u.indexOf("AAC") >= 0) return image_data_aac_60x60x16;
  if (u.indexOf("MP3") >= 0) return image_data_mp3_60x60x16;
  return nullptr;
}

static void drawCodecIconTopLeft() {
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
static int g_ch = 0;                 // 1=mono, 2=stereo, ...
static int g_sampleRate = 0;         // Hz (pl. 44100)
static int g_bitsPerSample = 0;      // bit (pl. 16)
static int g_pendingCh = 0;
static int g_pendingSampleRate = 0;
static int g_pendingBitsPerSample = 0;

// ------------------ Buffer kijelző ------------------ //
static size_t g_bufferFilled = 0;
static size_t g_bufferFree = 0;
static size_t g_bufferTotal = 0;
static int g_bufferPercent = 0;

// UI-callback wrappers (input_rotary / net_server expects void() callbacks)
// Keep these thin wrappers in app_impl to avoid modifying callback signatures.
static void updateVolumeOnly() {
  ui_drawBottomBar(g_Volume, g_bufferPercent, (WiFi.status() == WL_CONNECTED));
}

static void updateBufferIndicatorOnly() {
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

static String g_artist = "";
static String g_title  = "";


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
  g_savedSsid.reserve(64);
  g_savedPass.reserve(96);
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

static int textWidth24(const String& s) { tft.loadFont(FP_24.c_str()); return tft.textWidth(s.c_str()); }

static void recomputeLayout() {
  W = tft.width();
  H = tft.height();

  // Header font
  tft.loadFont(FP_24.c_str());
  int hHeader = tft.fontHeight();
  yHeader = 6;

  // Small label font
  tft.loadFont(FP_20.c_str());
  int h20 = tft.fontHeight();

  // Big text lines (station name / artist / title) are 24px font,
  // but the station name itself uses a bigger bold 28px font.
  tft.loadFont(FP_24.c_str());
  int h24 = tft.fontHeight();
  hArtistLine = h24 + 2;
  hTitleLine  = h24 + 2;

  tft.loadFont(FP_SB_28.c_str());
  int h28sb = tft.fontHeight();
  hStationLine = h28sb + 6; // extra room for descenders like g/y/p/j

  // Layout positions
  yStationLabel = yHeader + hHeader + 10;
  yStationName  = yStationLabel + h20 + 4 - 5;

  yStreamLabel  = yStationName + hStationLine + 16 + 8;

  // 480-as kijelzőn az előadó + dalcím blokk kerüljön lejjebb (profilból)
  yArtist = yStreamLabel + h20 + 6 - 5 + UI_ARTIST_TITLE_Y_SHIFT;
  yTitle  = yArtist + hArtistLine + 2;

  int marginBottom = 6;
  yVol = H - h20 - marginBottom;

  wifiW = 34; wifiH = 18;
  wifiX = W - wifiW - 4;
  wifiY = H - wifiH - 4;
}

//
// ------------------ Sprites ------------------ //

static void initSprites() {
  const bool havePsram = psramFound();

  sprStation.setColorDepth(16);
  if (havePsram) sprStation.setPsram(true);
  sprStation.createSprite(W, hStationLine);
  sprStation.fillScreen(TFT_BLACK);
  sprStation.loadFont(FP_SB_28.c_str());
  sprStation.setTextWrap(false);
  sprStation.setTextColor(TFT_ORANGE, TFT_BLACK); // ÁLLOMÁS SZÍNE

  sprArtist.setColorDepth(16);
  if (havePsram) sprArtist.setPsram(true);
  sprArtist.createSprite(W, hArtistLine);
  sprArtist.fillScreen(TFT_BLACK);
  sprArtist.loadFont(FP_SB_24.c_str());
  sprArtist.setTextWrap(false);
  sprArtist.setTextColor(TFT_CYAN, TFT_BLACK); // ELŐADÓ SZÍNE

  sprTitle.setColorDepth(16);
  if (havePsram) sprTitle.setPsram(true);
  sprTitle.createSprite(W, hTitleLine);
  sprTitle.fillScreen(TFT_BLACK);
  sprTitle.loadFont(FP_SB_24.c_str());
  sprTitle.setTextWrap(false);
  sprTitle.setTextColor(TFT_SILVER, TFT_BLACK); // DAL CÍM SZÍNE

  sprMenu.setColorDepth(16);
  if (havePsram) sprMenu.setPsram(true);
  sprMenu.createSprite(W, 28);
  sprMenu.fillScreen(TFT_BLACK);
  sprMenu.loadFont(FP_SB_24.c_str());
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
static void drawBufferIndicator() {
  // Csak akkor frissítjük, ha van stream
  if (g_stationUrl.length() == 0) return;
  
  // Buffer lekérése (500ms-ként)
  uint32_t now = millis();
  if (now - lastBufferCheck > 500) {
    lastBufferCheck = now;
    g_bufferFilled = audio.inBufferFilled();
    g_bufferFree = audio.inBufferFree();
    
    // Teljes buffer méret = foglalt + szabad
    g_bufferTotal = g_bufferFilled + g_bufferFree;
    
    if (g_bufferTotal > 0) {
      g_bufferPercent = (g_bufferFilled * 100) / g_bufferTotal;
    } else {
      g_bufferPercent = 0;
    }
  }
  // Nincs rajzolás itt: a UI modul kapja meg a g_bufferPercent értéket.
}



// ------------------ Bottom bar: Volume + buffer + WiFi ------------------ //
static void drawBottomBar() {
  // Buffer % frissítés (500ms-enként), a tényleges rajzolás a ui_display modulban van
  drawBufferIndicator();

  ui_drawBottomBar(g_Volume, g_bufferPercent, (WiFi.status() == WL_CONNECTED));
}


// ------------------ UI ------------------ //
static void drawStreamLabelLine() {
  tft.loadFont(FP_20.c_str()); // test_20
  int th = tft.fontHeight();
  int lineY = yStreamLabel;
  int lineH = th + 2;

  // teljes sor frissítés (villogás nélkül)
  clearRect(0, lineY - 4, W, lineH + 8);

  // "Stream: 2ch | 44KHz | 16bit | 320k" középre igazítva
  String line = fixText("Stream: ");
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

  String txt = fixText("SZÜNET");
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

static void setPaused(bool paused) {
  if (g_paused == paused) return;
  g_paused = paused;

  if (g_paused) {
    audioSendStop();
  } else {
    // visszaállítjuk a hangerőt és újracsatlakozunk a jelenlegi URL-re
    audioSendVolume(g_Volume);
    const String url = (g_playUrl.length() ? g_playUrl : g_stationUrl);
    if (url.length()) audioSendConnect(url);
  }
  drawStreamLabelLine();
}


static void togglePaused() { setPaused(!g_paused); }



// ---------- M3U playlist helpers ----------
static bool endsWithIgnoreCase(const String& s, const char* suffix) {
  String a = s; a.toLowerCase();
  String b = String(suffix); b.toLowerCase();
  return a.endsWith(b);
}
// --- URL helper: percent-decode (UTF-8 bytes stay UTF-8 in Arduino String) ---
static String urlPercentDecode(const String& in) {
  String out;
  out.reserve(in.length());
  for (int i = 0; i < (int)in.length(); i++) {
    char c = in[i];
    if (c == '%' && i + 2 < (int)in.length()) {
      char h1 = in[i + 1];
      char h2 = in[i + 2];
      auto hex = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'a' && h <= 'f') return 10 + (h - 'a');
        if (h >= 'A' && h <= 'F') return 10 + (h - 'A');
        return -1;
      };
      int a = hex(h1);
      int b = hex(h2);
      if (a >= 0 && b >= 0) {
        out += char((a << 4) | b);
        i += 2;
        continue;
      }
    }
    if (c == '+') { out += ' '; }
    else { out += c; }
  }
  return out;
}

// --- Best-effort "now playing" from URL when no ICY metadata is available ---
static void setNowPlayingFromUrl(const String& url) {
  // Strip scheme/host/query
  int q = url.indexOf('?');
  String u = (q >= 0) ? url.substring(0, q) : url;

  // Take path part after first single slash following "://"
  int p = u.indexOf("://");
  if (p >= 0) {
    p = u.indexOf('/', p + 3);
  } else {
    p = u.indexOf('/');
  }
  String path = (p >= 0) ? u.substring(p + 1) : u;

  // Percent-decode for display parsing
  String decoded = urlPercentDecode(path);

  // Split into segments
  int lastSlash = decoded.lastIndexOf('/');
  String file = (lastSlash >= 0) ? decoded.substring(lastSlash + 1) : decoded;
  String parent = "";
  if (lastSlash > 0) {
    int prevSlash = decoded.lastIndexOf('/', lastSlash - 1);
    parent = (prevSlash >= 0) ? decoded.substring(prevSlash + 1, lastSlash) : decoded.substring(0, lastSlash);
  }

  // Remove extension
  int dot = file.lastIndexOf('.');
  if (dot > 0) file = file.substring(0, dot);

  // Remove common track number prefixes: "01 - ", "01. ", "01_", etc.
  while (file.length() >= 3 && isDigit(file[0]) && isDigit(file[1]) &&
         (file[2] == ' ' || file[2] == '-' || file[2] == '.' || file[2] == '_')) {
    // drop first 3 chars, then optional separators/spaces
    file = file.substring(3);
    file.trim();
    if (file.startsWith("-")) { file = file.substring(1); file.trim(); }
    if (file.startsWith(".")) { file = file.substring(1); file.trim(); }
  }

  String artist = "";
  String title  = file;

  // Heuristic: if parent looks like "Artist - Album", take Artist
  if (parent.length()) {
    String a, t;
    splitArtistTitle(parent, a, t);
    if (a.length()) artist = a;
  }

  // If title itself contains "Artist - Title", prefer that
  {
    String a, t;
    splitArtistTitle(title, a, t);
    if (a.length() && t.length()) { artist = a; title = t; }
  }

  String display = artist.length() ? (artist + " - " + title) : title;

  g_pendingTitle = fixText(display.c_str());
  g_newTitleFlag = true;
}


static bool loadM3U(const String& m3uUrl) {
  g_playlistUrls.clear();
  g_playlistIndex = -1;
  g_playlistSourceUrl = "";

  if (!WiFi.isConnected()) return false;

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(m3uUrl)) return false;

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  body.replace("\r", "");
  int start = 0;
  while (start < (int)body.length()) {
    int nl = body.indexOf('\n', start);
    if (nl < 0) nl = body.length();
    String line = body.substring(start, nl);
    line.trim();
    start = nl + 1;

    if (line.length() == 0) continue;
    if (line.startsWith("#")) continue;

    // Accept absolute URLs. (Relative support could be added later.)
    if (line.indexOf("://") < 0) continue;

    g_playlistUrls.push_back(line);
  }

  if (g_playlistUrls.empty()) return false;

  g_playlistSourceUrl = m3uUrl;
  g_playlistIndex = 0;
  return true;
}


static bool startPlaybackCurrent(bool allowReloadPlaylist = true) {
  // Decide what URL to actually play: direct stream or current playlist track
  if (endsWithIgnoreCase(g_stationUrl, ".m3u")) {
    bool needReload = (g_playlistSourceUrl != g_stationUrl) || g_playlistUrls.empty() || (g_playlistIndex < 0);
    if (allowReloadPlaylist && needReload) {
      if (!loadM3U(g_stationUrl)) {
        // fallback: try to let Audio lib handle it (some builds can)
        g_playUrl = g_stationUrl;
      } else {
        g_playUrl = g_playlistUrls[g_playlistIndex];
      }
    } else if (!g_playlistUrls.empty() && g_playlistIndex >= 0 && g_playlistIndex < (int)g_playlistUrls.size()) {
      g_playUrl = g_playlistUrls[g_playlistIndex];
    } else {
      g_playUrl = g_stationUrl;
    }
  } else {
    g_playlistUrls.clear();
    g_playlistIndex = -1;
    g_playlistSourceUrl = "";
    g_playUrl = g_stationUrl;
  }

  // Itt csak kiszámoljuk a lejátszandó URL-t és reseteljük a meta állapotot.
  // A tényleges connect parancsot az audioTask kapja meg queue-n keresztül (audioSendConnect).
  g_id3Artist = "";
  g_id3Title  = "";
  g_id3SeenAt = 0;
  // For local HTTP file playback there is often no ICY metadata; we show a fallback derived from the URL.
  // If ID3 arrives, it will override via my_audio_info().
  setNowPlayingFromUrl(g_playUrl);
  return true;
}

static void requestAutoNextTrack() {
  // Debounce: avoid repeated EOF notifications
  const uint32_t now = millis();
  if ((int32_t)(now - g_autoNextRequestedAt) < 500) return;
  g_autoNextRequestedAt = now;
  g_autoNextRequested = true;
}

static bool advancePlaylistAndPlay() {
  if (g_paused) return false;
  if (!endsWithIgnoreCase(g_stationUrl, ".m3u")) return false;

  // Ensure playlist is loaded
  if (g_playlistSourceUrl != g_stationUrl || g_playlistUrls.empty()) {
    if (!loadM3U(g_stationUrl)) return false;
  }
  if (g_playlistUrls.empty()) return false;

  g_playlistIndex = (g_playlistIndex + 1) % (int)g_playlistUrls.size();
  g_playUrl = g_playlistUrls[g_playlistIndex];

  // Reset metadata & show fallback title from URL (ID3 may overwrite later)
  g_id3Artist = "";
  g_id3Title  = "";
  g_id3SeenAt = 0;
  setNowPlayingFromUrl(g_playUrl);

  audioSendConnect(g_playUrl);
  return true;
}

static void updateStationNameUI() {
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

  tft.loadFont(FP_24.c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  String header = fixText("myRadio");

// Codec ikon bal felső sarok
  drawCodecIconTopLeft();

  // Fejléc + jobb felső LOGO (UI modul)
  ui_drawHeaderAndLogo(header, yHeader, CODEC_ICON_W, LOGO_W);
  tft.loadFont(FP_20.c_str());
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
enum UIMode { MODE_PLAY, MODE_MENU };
static UIMode g_mode = MODE_PLAY;

static void drawMenuScreen() {
  tft.loadFont(FP_20.c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int h = tft.fontHeight();

  // Töröljük a menü teljes régióját (a képernyő aljáig), hogy ne "üssön át" semmi
  int menuTop    = yStreamLabel;
  int menuBottom = tft.height();
  clearRect(0, menuTop, W, (menuBottom - menuTop));

  // --- 5 sor egyenletes elosztása a rendelkezésre álló magasságon ---
  // Sorok: 1) "Válassz állomást:"  2) "52/71"  3) sárga állomásnév (sprite)
  //       4) OK/Kilép             5) IP
  int sprH = sprMenu.height();
  int contentH = (h /*header*/ + h /*counter*/ + sprH /*name*/ + h /*ok*/ + h /*ip*/);
  int gaps = 4;

  int availH = menuBottom - menuTop;
  int gap = 2;
  if (availH > contentH) {
    gap = (availH - contentH) / gaps;
    if (gap < 2) gap = 2;
    if (gap > 18) gap = 18;
  }

  int yHeader  = menuTop;
  int yCounter = yHeader + h + gap;
  int yName    = yCounter + h + gap;
  int yOk      = yName + sprH + gap;
  int yIp      = yOk + h + gap;

  // Biztonsági igazítás, ha valamiért mégis lelógna (kijelző/Font eltérés)
  if (yIp + h > menuBottom) {
    yIp = menuBottom - h;
    yOk = yIp - h - gap;
    yName = yOk - sprH - gap;
    yCounter = yName - h - gap;
    yHeader = yCounter - h - gap;
    if (yHeader < menuTop) yHeader = menuTop;
  }

  // 1) fejléc
  tft.setCursor(0, yHeader);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("Válassz állomást:");

  // 2) számláló sor
  tft.setCursor(0, yCounter);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (g_stationCount > 0) tft.printf("%d / %d", (g_menuIndex + 1), g_stationCount);
  else tft.print("Nincs lista (stations.txt)");

  // 3) állomásnév sprite (mindig külön "sor" – így nem vághat bele a 52/71-be)
  g_menuNameY = yName;

  menuScroll = false;
  xMenu = 0;
  lastMenuTick = millis();

  // sprite terület törlése + kirajzolás
  clearRect(0, g_menuNameY, W, sprH);
  sprMenu.fillScreen(TFT_BLACK);
  sprMenu.setTextColor(TFT_GOLD, TFT_BLACK);

  if (g_stationCount > 0) {
    String name = g_stations[g_menuIndex].name;
    int w = tft.textWidth(name.c_str());
    if (w <= W) {
      sprMenu.setCursor((W - w) / 2 - 24, 0);
      sprMenu.print(name);
    } else {
      menuScroll = true;
      xMenu = 0;
      sprMenu.setCursor(0, 0);
      sprMenu.print(name);
    }
  }
  sprMenu.pushSprite(0, g_menuNameY);

  // 4) OK sor
  tft.setCursor(0, yOk);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.print("OK: nyom | Kilép: hosszan nyom");

  // 5) IP sor
  tft.setCursor(0, yIp);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    tft.print("IP: ");
    tft.print(WiFi.localIP());
  } else {
    tft.print("IP: - (nincs WiFi)");
  }
}

static void updateMenuNameScroll() {
  if (!menuScroll || g_stationCount <= 0) return;

  uint32_t now = millis();
  if (now - lastMenuTick < MENU_MS) return;
  lastMenuTick = now;

  String name = g_stations[g_menuIndex].name;
  int w = tft.textWidth(name.c_str());

  xMenu -= 2;
  if (xMenu < -(w + 20)) xMenu = W;

  sprMenu.fillScreen(TFT_BLACK);
  sprMenu.setCursor(xMenu, 0);
  sprMenu.print(name);
  sprMenu.pushSprite(0, g_menuNameY);
}


static void exitMenuRedrawPlayUI() {
  g_mode = MODE_PLAY;

  tft.loadFont(FP_20.c_str());
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  // Menüből visszalépésnél töröljük a teljes tartalmi régiót egészen az alsó sáv tetejéig,
  // hogy ne maradjanak ott "menü-maradvány" pixelek.
  clearRect(0, yStreamLabel, W, (H - yStreamLabel));
  drawStreamLabelLine();

  updateMarquee();
  drawBottomBar();

  // A menü teljes törlése lenullázza a VU statikus keretét is, ezért a cache-t
  // érvénytelenítjük és azonnal újrarajzoljuk a teljes VU-t (keret + sávok).
  ui_invalidateVuMeter();
  ui_drawVuMeter(vu_getL(), vu_getR(), vu_getPeakL(), vu_getPeakR());
}

// ------------------ Codec/bitrate parse helpers ------------------ //
static int parseFirstInt(const char* s) {
  int val = 0;
  bool found = false;
  while (*s) {
    if (*s >= '0' && *s <= '9') { found = true; val = val * 10 + (*s - '0'); }
    else if (found) break;
    s++;
  }
  return found ? val : 0;
}

static String detectCodecFromText(const String& s) {
  String u = s; u.toUpperCase();
  if (u.indexOf("OPUS") >= 0) return "OPUS";
  if (u.indexOf("FLAC") >= 0) return "FLAC";
  if (u.indexOf("VORBIS") >= 0) return "OGG";
  if (u.indexOf("OGG") >= 0) return "OGG";
  if (u.indexOf("AAC") >= 0) return "AAC";
  if (u.indexOf("MP3") >= 0) return "MP3";
  if (u.indexOf("MPEG-1 LAYER III") >= 0) return "MP3";
  return "";
}


// --- ID3 parsing helpers (best effort, library-agnostic) ---
static String trimCopy(String s) { s.trim(); return s; }

static String extractAfterColon(const String& s) {
  int p = s.indexOf(':');
  if (p < 0) return "";
  String v = s.substring(p + 1);
  v.trim();
  return v;
}

static bool startsWithNoCase(const String& s, const char* prefix) {
  String a = s; a.toLowerCase();
  String b = String(prefix); b.toLowerCase();
  return a.startsWith(b);
}

static void maybePublishId3() {
  // publish when we have at least a title; artist optional
  if (g_id3Title.length() == 0) return;

  String display = g_id3Artist.length()
    ? (g_id3Artist + " - " + g_id3Title)
    : g_id3Title;

  g_pendingTitle = fixText(display.c_str());
  g_newTitleFlag = true;
}

// ------------------ Audio callback (NEM RAJZOL!) ------------------ //
void my_audio_info(Audio::msg_t m) {
  serialLogf("%s: %s\n", m.s, m.msg);

  // Track ended detection (local HTTP MP3 playback). Many builds report EOF as a text message.
  {
    String tag = String(m.s);
    String msg = String(m.msg);
    String tagL = tag; tagL.toLowerCase();
    String msgL = msg; msgL.toLowerCase();
    bool isEof = (tagL.indexOf("eof") >= 0) || (msgL.indexOf("eof") >= 0) ||
                 (msgL.indexOf("end of file") >= 0) || (msgL.indexOf("stream end") >= 0) ||
                 (msgL.indexOf("end") == 0 && msgL.indexOf("end") >= 0);
    if (isEof && endsWithIgnoreCase(g_stationUrl, ".m3u")) {
      requestAutoNextTrack();
      return;
    }
  }

  if (m.e == Audio::evt_streamtitle) {
    String t = String(m.msg);
    t.trim();

    // ignore junk metadata
    if (!t.length()) return;
    if (t.equalsIgnoreCase("PMEDIA")) return;
    if (t.equalsIgnoreCase("NA")) return;

    // ignore if local file playback (we prefer ID3)
    if (g_playUrl.startsWith("http://192.") ||
        g_playUrl.startsWith("http://10.")  ||
        g_playUrl.startsWith("http://172."))
        return;

    // ignore short ALLCAPS spam
    bool allCaps = true;
    for (char c : t) {
      if (isalpha(c) && islower(c)) { allCaps = false; break; }
    }
    if (allCaps && t.length() < 12) return;

    g_pendingTitle = fixText(t.c_str());
    g_newTitleFlag = true;
    return;
  }

  String tag = String(m.s);
  String msg = String(m.msg);

// ID3 tags for local files: many Audio libs report them via the info callback as text messages
// Examples seen in the wild:
//   "ID3: TIT2: Title"
//   "ID3: TPE1: Artist"
//   "ID3: Title: ..."
//   "ID3: Artist: ..."
{
  String tagL = tag; tagL.toLowerCase();
  String msgL = msg; msgL.toLowerCase();

  bool looksId3 = (tagL.indexOf("id3") >= 0) || (msgL.indexOf("id3") >= 0) ||
                  (msg.indexOf("TIT2") >= 0) || (msg.indexOf("TPE1") >= 0);

  if (looksId3) {
    String val;

    // Case: "ID3" tag with payload containing frame id
    if (msg.indexOf("TPE1") >= 0) { // artist
      val = extractAfterColon(msg);
      if (val.length() == 0) {
        int p = msg.indexOf("TPE1");
        if (p >= 0) { val = msg.substring(p); val = extractAfterColon(val); }
      }
      if (val.length()) { g_id3Artist = trimCopy(val); g_id3SeenAt = millis(); maybePublishId3(); return; }
    }
    if (msg.indexOf("TIT2") >= 0) { // title
      val = extractAfterColon(msg);
      if (val.length() == 0) {
        int p = msg.indexOf("TIT2");
        if (p >= 0) { val = msg.substring(p); val = extractAfterColon(val); }
      }
      if (val.length()) { g_id3Title = trimCopy(val); g_id3SeenAt = millis(); maybePublishId3(); return; }
    }

    // Case: human readable
    if (startsWithNoCase(msg, "title") || startsWithNoCase(msg, "id3 title") || startsWithNoCase(msg, "tit2")) {
      val = extractAfterColon(msg);
      if (val.length()) { g_id3Title = trimCopy(val); g_id3SeenAt = millis(); maybePublishId3(); return; }
    }
    if (startsWithNoCase(msg, "artist") || startsWithNoCase(msg, "id3 artist") || startsWithNoCase(msg, "tpe1")) {
      val = extractAfterColon(msg);
      if (val.length()) { g_id3Artist = trimCopy(val); g_id3SeenAt = millis(); maybePublishId3(); return; }
    }
  }
}


  String c = detectCodecFromText(msg);
  if (c.length() == 0) c = detectCodecFromText(tag);
  if (c.length()) {
    g_pendingCodec = c;
    g_newStatusFlag = true;
  }

  if (tag.equalsIgnoreCase("bitrate") || msg.indexOf("Bitrate") >= 0 || msg.indexOf("bitrate") >= 0) {
    int bps = parseFirstInt(m.msg);
    if (bps > 0) {
      int kbps = (bps + 500) / 1000;
      g_pendingBitrateK = kbps;
      g_newStatusFlag = true;
    }
  }


  // ---- Audio format parse (csatorna / mintavétel / bitmélység) ----
  {
    String tagL = tag; tagL.toLowerCase();
    String msgL = msg; msgL.toLowerCase();

    // Channels
    int ch = 0;
    if (msgL.indexOf("stereo") >= 0) ch = 2;
    else if (msgL.indexOf("mono") >= 0) ch = 1;
    else if (tagL.indexOf("channels") >= 0 || msgL.indexOf("channels") >= 0 || msgL.indexOf(" ch") >= 0) {
      int v = parseFirstInt(m.msg);
      if (v >= 1 && v <= 8) ch = v;
    }
    if (ch > 0) { g_pendingCh = ch; g_newStatusFlag = true; }

    // Sample rate (Hz)
    if (tagL.indexOf("samplerate") >= 0 || msgL.indexOf("samplerate") >= 0 ||
        msgL.indexOf("sample rate") >= 0 || msgL.indexOf("sample_rate") >= 0 ||
        tagL.indexOf("sample") >= 0) {
      int sr = parseFirstInt(m.msg);
      // néha kHz-ben jön: 44/48/96
      if (sr > 0 && sr < 1000) sr *= 1000;
      if (sr >= 8000 && sr <= 384000) { g_pendingSampleRate = sr; g_newStatusFlag = true; }
    } else if (msgL.indexOf("hz") >= 0) {
      int sr = parseFirstInt(m.msg);
      if (sr > 0 && sr < 1000) sr *= 1000;
      if (sr >= 8000 && sr <= 384000) { g_pendingSampleRate = sr; g_newStatusFlag = true; }
    }

    // Bits per sample
    if (tagL.indexOf("bits") >= 0 || msgL.indexOf("bits") >= 0 ||
        msgL.indexOf("bit depth") >= 0 || msgL.indexOf("bits per sample") >= 0 ||
        msgL.indexOf("bps") >= 0) {
      int b = parseFirstInt(m.msg);
      if (b >= 8 && b <= 32) { g_pendingBitsPerSample = b; g_newStatusFlag = true; }
    }
  }
}

// ------------------ Button ------------------ //
static bool btnLast = true;
static uint32_t btnDownAt = 0;
static bool longFired = false;
static const uint32_t LONG_MS = 650;

static void handleButton() {
  bool btnNow = digitalRead(ENC_BTN);
  uint32_t now = millis();

  if (btnLast == true && btnNow == false) {
    btnDownAt = now;
    longFired = false;
  }

  if (btnNow == false && !longFired && (now - btnDownAt >= LONG_MS)) {
    longFired = true;
    if (g_mode == MODE_PLAY) {
      g_mode = MODE_MENU;
      g_menuIndex = g_currentIndex;
      ui_invalidateVuMeter();
      drawMenuScreen();
    } else {
      exitMenuRedrawPlayUI();
    }
  }

  if (btnLast == false && btnNow == true) {
    if (!longFired) {
      if (g_mode == MODE_PLAY) {
        togglePaused();
      } else if (g_mode == MODE_MENU) {
        if (g_stationCount > 0) {
          g_currentIndex = g_menuIndex;
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
          tft.loadFont(FP_20.c_str());
          renderLine(sprArtist, "", 0);
          renderLine(sprTitle,  "", 0);
          sprArtist.pushSprite(0, yArtist);
          sprTitle.pushSprite(0, yTitle);

          updateStationNameUI();

          saveLastStationToNVS();
          saveLastStationToSPIFFS();

          startPlaybackCurrent(true);
          if (g_playUrl.length()) audioSendConnect(g_playUrl);

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
  }

  btnLast = btnNow;
}

// ------------------ WiFi függvények ------------------ //

static bool loadWiFiCredsFromSPIFFS(String& outSsid, String& outPass) {
  if (!SPIFFS.exists(WIFI_CRED_FILE)) return false;
  File f = SPIFFS.open(WIFI_CRED_FILE, "r");
  if (!f) return false;
  outSsid = f.readStringUntil('\n'); outSsid.trim();
  outPass = f.readStringUntil('\n'); outPass.trim();
  f.close();
  return outSsid.length() > 0;
}

static bool saveWiFiCredsToSPIFFS(const String& ssid, const String& pass) {
  File f = SPIFFS.open(WIFI_CRED_FILE, "w");
  if (!f) return false;
  f.println(ssid);
  f.println(pass);
  f.close();
  return true;
}

static bool connectWiFi(const String& ssid, const String& pass, uint32_t timeoutMs) {
  if (!ssid.length()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  Serial.printf("[WiFi] Connecting to '%s'...\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
      g_wifiConnectedAt = millis();
      return true;
    }
    delay(150);
  }

  Serial.printf("[WiFi] Connect timeout. status=%d\n", (int)WiFi.status());
  return false;
}

static void startWiFiConfigPortal() {
  const char* apSsid = "WebRadio-Setup";
  const char* apPass = "";

  WiFi.disconnect(true, true);
  delay(100);

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(apSsid, apPass);
  IPAddress ip = WiFi.softAPIP();
  drawWiFiPortalHelp(apSsid, ip);


  Serial.printf("[WiFi] Config portal AP: %s  IP: %s  (%s)\n", apSsid, ip.toString().c_str(), ok ? "OK" : "FAIL");

  WebServer server(80);

  auto page = []() -> String {
    String html;
    html += "<!doctype html><html><head><meta charset='utf-8'/>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'/>";
    html += "<title>WiFi setup</title></head><body style='font-family:sans-serif;max-width:520px;margin:24px auto;'>";
    html += "<h2>WiFi beállítás</h2>";
    html += "<form method='POST' action='/save'>";
    html += "<label>SSID<br><input name='s' style='width:100%;padding:10px' required></label><br><br>";
    html += "<label>Jelszó<br><input name='p' type='password' style='width:100%;padding:10px'></label><br><br>";
    html += "<button style='padding:10px 16px'>Mentés</button>";
    html += "</form>";
    html += "<p style='opacity:.7'>Mentés után az eszköz újraindul.</p>";
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
      server.send(400, "text/plain; charset=utf-8", "Hiányzó SSID.");
      return;
    }

    bool saved = saveWiFiCredsToSPIFFS(ssid, pass);
    if (saved) {
      server.send(200, "text/plain; charset=utf-8", "Mentve. Újraindítás...");
      delay(400);
      ESP.restart();
    } else {
      server.send(500, "text/plain; charset=utf-8", "Nem sikerült menteni SPIFFS-be.");
    }
  });

  server.begin();

  while (true) {
    server.handleClient();
    delay(2);
  }
}

static void onWiFiRestored() {
  Serial.println("[WiFi] Restored.");
  g_needStreamReconnect = true;
}

static void handleWiFiReconnect() {
  wl_status_t st = WiFi.status();
  uint32_t now = millis();

  if (st == WL_CONNECTED) {
    if (!g_wifiWasConnected) {
      g_wifiWasConnected = true;
      g_wifiDownSince = 0;
      g_wifiLastAttempt = 0;
      g_wifiAttemptCount = 0;
      g_wifiAttemptInterval = 5000;
      onWiFiRestored();
    }
    return;
  }

  if (g_wifiWasConnected) {
    g_wifiWasConnected = false;
    g_wifiDownSince = now;
    g_wifiLastAttempt = 0;
    g_wifiAttemptCount = 0;
    g_wifiAttemptInterval = 5000;
    Serial.println("[WiFi] Disconnected.");
  }

  if (!g_haveWiFiCreds) return;

#if WIFI_FALLBACK_TO_PORTAL
  if (g_wifiDownSince != 0 && (now - g_wifiDownSince) > WIFI_RETRY_TO_PORTAL_MS) {
    Serial.println("[WiFi] Down too long -> opening config portal.");
    startWiFiConfigPortal();
    return;
  }
#endif

  if (g_wifiLastAttempt == 0 || (now - g_wifiLastAttempt) >= g_wifiAttemptInterval) {
    g_wifiLastAttempt = now;
    g_wifiAttemptCount++;

    Serial.printf("[WiFi] Reconnect attempt #%u...\n", (unsigned)g_wifiAttemptCount);

    bool ok = connectWiFi(g_savedSsid, g_savedPass, 8000);
    if (ok) {
      g_wifiWasConnected = true;
      g_wifiDownSince = 0;
      g_wifiAttemptCount = 0;
      g_wifiAttemptInterval = 5000;
      onWiFiRestored();
    } else {
      g_wifiAttemptInterval = min((uint32_t)60000, (uint32_t)(g_wifiAttemptInterval * 2));
    }
  }
}

// ------------------ Web Server Functions ------------------ //

static void handleRoot() {
  if (!SPIFFS.exists("/web/index.html")) {
    // Empty SPIFFS: show setup/upload page so you can upload UI/fonts
    handleUploadPage();
    return;
  }
  File file = SPIFFS.open("/web/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Web interface not found");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

// Serve the Radio-Browser based search page from SPIFFS.
// This is intentionally separate from the main UI so we don't touch index.html logic.
static void handleSearch() {
  if (!SPIFFS.exists("/web/search.html")) {
    server.send(404, "text/plain", "search.html not found");
    return;
  }
  File file = SPIFFS.open("/web/search.html", "r");
  if (!file) {
    server.send(500, "text/plain", "search.html open failed");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

// --- helpers: accept both x-www-form-urlencoded params and JSON bodies ---
static String _getBodyPlain() {
  // WebServer stores raw request body under arg("plain") for POST requests.
  if (server.hasArg("plain")) return server.arg("plain");
  return String();
}

// Very small JSON extractor for {"key":"value"} and {"key":123}
// Not a full JSON parser; sufficient for our simple API payloads.
static bool _jsonFindString(const String& body, const char* key, String& out) {
  String k = String("\"") + key + "\"";
  int p = body.indexOf(k);
  if (p < 0) return false;
  p = body.indexOf(':', p + k.length());
  if (p < 0) return false;
  // skip whitespace
  while (p < (int)body.length() && (body[p] == ':' || body[p] == ' ' || body[p] == '\t' || body[p] == '\r' || body[p] == '\n')) p++;
  if (p >= (int)body.length() || body[p] != '"') return false;
  p++; // after opening quote
  String v;
  for (int i = p; i < (int)body.length(); i++) {
    char c = body[i];
    if (c == '\\\\') { // basic escapes
      if (i + 1 < (int)body.length()) {
        char n = body[i + 1];
        if (n == '"' || n == '\\\\' || n == '/') { v += n; i++; continue; }
        if (n == 'n') { v += '\n'; i++; continue; }
        if (n == 'r') { v += '\r'; i++; continue; }
        if (n == 't') { v += '\t'; i++; continue; }
      }
      // fallback: keep slash
      v += c;
      continue;
    }
    if (c == '"') { out = v; return true; }
    v += c;
  }
  return false;
}

static bool _jsonFindInt(const String& body, const char* key, int& out) {
  String k = String("\"") + key + "\"";
  int p = body.indexOf(k);
  if (p < 0) return false;
  p = body.indexOf(':', p + k.length());
  if (p < 0) return false;
  while (p < (int)body.length() && (body[p] == ':' || body[p] == ' ' || body[p] == '\t' || body[p] == '\r' || body[p] == '\n')) p++;
  if (p >= (int)body.length()) return false;

  // Allow numbers encoded as JSON strings too: "5"
  bool quoted = false;
  if (body[p] == '"') { quoted = true; p++; }

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


static bool _getParamString(const char* key, String& out) {
  if (server.hasArg(key)) { out = server.arg(key); return true; }
  String body = _getBodyPlain();
  if (!body.length()) return false;
  return _jsonFindString(body, key, out);
}

static bool _getParamInt(const char* key, int& out) {
  if (server.hasArg(key)) { out = server.arg(key).toInt(); return true; }
  String body = _getBodyPlain();
  if (!body.length()) return false;
  return _jsonFindInt(body, key, out);
}
// --- end helpers ---
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

static void handleSetStation() {
  if (!server.hasArg("index")) {
    server.send(400, "text/plain", "Missing index");
    return;
  }
  
  int index = server.arg("index").toInt();
  if (index < 0 || index >= g_stationCount) {
    server.send(400, "text/plain", "Invalid index");
    return;
  }
  
  g_currentIndex = index;
  g_stationName = g_stations[g_currentIndex].name;
  g_stationUrl = g_stations[g_currentIndex].url;
  
  updateStationNameUI();
  saveLastStationToNVS();
  saveLastStationToSPIFFS();
  
  startPlaybackCurrent(true);
  if (g_playUrl.length()) audioSendConnect(g_playUrl);
  
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
  
  if (g_mode == MODE_MENU) {
    exitMenuRedrawPlayUI();
  }
  
  server.send(200, "text/plain", "OK");
}


static void handleAddStation() {
  String name, url;
  if (!_getParamString("name", name) || !_getParamString("url", url)) {
    server.send(400, "text/plain", "Missing name or url");
    return;
  }

  if (g_stationCount >= MAX_STATIONS) {
    server.send(400, "text/plain", "Station list full");
    return;
  }

  name.trim();
  url.trim();

  if (name.length() == 0 || url.length() == 0) {
    server.send(400, "text/plain", "Empty name or url");
    return;
  }

  // Minimal validation / safety
  name.replace("\t", " ");   // tab is our separator
  name.replace("\n", " ");
  name.replace("\r", " ");

  g_stations[g_stationCount].name = name;
  g_stations[g_stationCount].url  = url;
  g_stationCount++;

  if (!saveStationsToSPIFFS()) {
    server.send(500, "text/plain", "Failed to save");
    return;
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleDeleteStation() {
  int del = -1;
  if (!_getParamInt("index", del)) {
    server.send(400, "text/plain", "Missing index");
    return;
  }

  if (del < 0 || del >= g_stationCount) {
    server.send(400, "text/plain", "Invalid index");
    return;
  }

  // Shift left
  for (int i = del; i < g_stationCount - 1; i++) {
    g_stations[i] = g_stations[i + 1];
  }
  g_stationCount--;

  if (g_stationCount <= 0) {
    g_stationCount = 0;
    g_currentIndex = 0;
    g_menuIndex    = 0;
  } else {
    if (del < g_currentIndex) g_currentIndex--;
    if (g_currentIndex >= g_stationCount) g_currentIndex = g_stationCount - 1;
    g_menuIndex = g_currentIndex;

    // Keep current station consistent
    g_stationName = g_stations[g_currentIndex].name;
    g_stationUrl  = g_stations[g_currentIndex].url;

    updateStationNameUI();
    saveLastStationToNVS();
    saveLastStationToSPIFFS();

    // If playback is active, reconnect to the (possibly new) current station
    if (!g_paused) {
      startPlaybackCurrent(true);
      if (g_playUrl.length()) audioSendConnect(g_playUrl);
    }
  }

  if (!saveStationsToSPIFFS()) {
    server.send(500, "text/plain", "Failed to save");
    return;
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

// Accepts:
//  - x-www-form-urlencoded: index, name, url(optional)
//  - JSON body: {"index":N,"name":"...","url":"..."}
// Accepts:
//  - x-www-form-urlencoded: index, name, url(optional)
//  - JSON body: {"index":N,"name":"...","url":"..."}
void handleUpdateStation() {
  int idx = -1;
  if (!_getParamInt("index", idx) && !_getParamInt("idx", idx)) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Hiányzó index\"}");
    return;
  }
  if (idx < 0 || idx >= g_stationCount) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Érvénytelen index\"}");
    return;
  }

  String name, url;
  bool hasName = _getParamString("name", name);
  bool hasUrl  = _getParamString("url", url);

  // Some UIs might send 'title' instead of 'name'
  if (!hasName) hasName = _getParamString("title", name);

  if (!hasName && !hasUrl) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Hiányzó név vagy URL\"}");
    return;
  }

  if (hasName) {
    name.trim();
    if (!name.length()) {
      server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Üres név\"}");
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
      server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Üres URL\"}");
      return;
    }
    g_stations[idx].url = url;
  }

  // If the updated station is the current one, keep UI/state consistent
  if (idx == g_currentIndex) {
    g_stationName = g_stations[g_currentIndex].name;
    g_stationUrl  = g_stations[g_currentIndex].url;
    updateStationNameUI();
    saveLastStationToNVS();
    saveLastStationToSPIFFS();
  }

  if (!saveStationsToSPIFFS()) {
    server.send(500, "application/json", "{\"ok\":false,\"msg\":\"Mentés sikertelen (SPIFFS)\"}");
    return;
  }

  String out = "{\"ok\":true,\"msg\":\"Mentve\",\"index\":" + String(idx) + "}";
  server.send(200, "application/json", out);
}

// Accepts:
//  - x-www-form-urlencoded: from, to
//  - JSON body: {"from":N,"to":M}
// Accepts:
//  - x-www-form-urlencoded: from, to (and common aliases)
//  - JSON body: {"from":N,"to":M} (and common aliases)
void handleMoveStation() {
  int from = -1, to = -1;

  // accept multiple param names without touching the UI
  bool okFrom =
      _getParamInt("from", from) ||
      _getParamInt("fromIndex", from) ||
      _getParamInt("src", from) ||
      _getParamInt("oldIndex", from) ||
      _getParamInt("index", from);

  bool okTo =
      _getParamInt("to", to) ||
      _getParamInt("toIndex", to) ||
      _getParamInt("dst", to) ||
      _getParamInt("newIndex", to) ||
      _getParamInt("target", to);

  if (!okFrom || !okTo) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Hiányzó from/to\"}");
    return;
  }
  if (from < 0 || from >= g_stationCount || to < 0 || to >= g_stationCount) {
    server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Érvénytelen from/to\"}");
    return;
  }
  if (from == to) {
    server.send(200, "application/json", "{\"ok\":true,\"msg\":\"Nincs változás\"}");
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

  // Update current index mapping
  if (g_currentIndex == from) g_currentIndex = to;
  else if (from < to && g_currentIndex > from && g_currentIndex <= to) g_currentIndex--;
  else if (from > to && g_currentIndex >= to && g_currentIndex < from) g_currentIndex++;

  g_menuIndex = g_currentIndex;

  // keep current station strings correct
  if (g_stationCount > 0) {
    g_stationName = g_stations[g_currentIndex].name;
    g_stationUrl  = g_stations[g_currentIndex].url;
  }

  if (!saveStationsToSPIFFS()) {
    server.send(500, "application/json", "{\"ok\":false,\"msg\":\"Mentés sikertelen (SPIFFS)\"}");
    return;
  }

  String out = "{\"ok\":true,\"msg\":\"Sorrend mentve\",\"from\":" + String(from) + ",\"to\":" + String(to) +
               ",\"currentIndex\":" + String(g_currentIndex) + "}";
  server.send(200, "application/json", out);
}



void handleGetBrightness() {
  // JSON: { "value": 0..255 }
  String out = "{\"value\":" + String((int)g_brightness) + "}";
  server.send(200, "application/json", out);
}

void handleSetBrightness() {
  // Accept val in query or form body; clamp 0..255
  int v = -1;
  if (server.hasArg("val")) v = server.arg("val").toInt();
  else if (server.hasArg("value")) v = server.arg("value").toInt();
  if (v < 0) v = 0;
  if (v > 255) v = 255;
  bl_set((uint8_t)v);
  handleGetBrightness();
}

void handleSetVolume() {
  if (!server.hasArg("volume")) {
    server.send(400, "text/plain", "Missing volume");
    return;
  }
  
  int vol = server.arg("volume").toInt();
  if (vol < VOLUME_MIN || vol > VOLUME_MAX) {
    server.send(400, "text/plain", "Invalid volume");
    return;
  }
  
  g_Volume = vol;
  audioSendVolume(g_Volume);
      drawBottomBar();
  
  server.send(200, "text/plain", "OK");
}

static void handleTogglePause() {
  togglePaused();
  server.send(200, "text/plain", g_paused ? "paused" : "playing");
}

static void handleGetStatus() {
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
  if (!endsWithIgnoreCase(g_stationUrl, ".m3u")) {
    server.send(400, "text/plain", "Not a playlist station");
    return;
  }
  // Ensure playlist loaded
  if (g_playlistSourceUrl != g_stationUrl || g_playlistUrls.empty()) {
    if (!loadM3U(g_stationUrl)) {
      server.send(500, "text/plain", "Failed to load playlist");
      return;
    }
  }
  if (g_playlistUrls.empty()) {
    server.send(500, "text/plain", "Playlist empty");
    return;
  }
  g_playlistIndex = (g_playlistIndex + 1) % (int)g_playlistUrls.size();
  g_playUrl = g_playlistUrls[g_playlistIndex];
  audioSendConnect(g_playUrl);
  setNowPlayingFromUrl(g_playUrl);

  server.send(200, "application/json",
    String("{\"trackIndex\":") + g_playlistIndex + ",\"trackCount\":" + (int)g_playlistUrls.size() + "}"
  );
}

void handleTrackPrev() {
  if (!endsWithIgnoreCase(g_stationUrl, ".m3u")) {
    server.send(400, "text/plain", "Not a playlist station");
    return;
  }
  if (g_playlistSourceUrl != g_stationUrl || g_playlistUrls.empty()) {
    if (!loadM3U(g_stationUrl)) {
      server.send(500, "text/plain", "Failed to load playlist");
      return;
    }
  }
  if (g_playlistUrls.empty()) {
    server.send(500, "text/plain", "Playlist empty");
    return;
  }
  g_playlistIndex = (g_playlistIndex - 1 + (int)g_playlistUrls.size()) % (int)g_playlistUrls.size();
  g_playUrl = g_playlistUrls[g_playlistIndex];
  audioSendConnect(g_playUrl);
  setNowPlayingFromUrl(g_playUrl);

  server.send(200, "application/json",
    String("{\"trackIndex\":") + g_playlistIndex + ",\"trackCount\":" + (int)g_playlistUrls.size() + "}"
  );
}

void handleNextStation() {
  if (g_stationCount == 0) {
    server.send(400, "text/plain", "No stations");
    return;
  }
  
  g_currentIndex = (g_currentIndex + 1) % g_stationCount;
  g_stationName = g_stations[g_currentIndex].name;
  g_stationUrl = g_stations[g_currentIndex].url;
  
  updateStationNameUI();
  saveLastStationToNVS();
  saveLastStationToSPIFFS();
  
  startPlaybackCurrent(true);
  if (g_playUrl.length()) audioSendConnect(g_playUrl);
  
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
  
  server.send(200, "application/json", "{\"index\":" + String(g_currentIndex) + ",\"name\":\"" + g_stationName + "\"}");
}

static void handlePrevStation() {
  if (g_stationCount == 0) {
    server.send(400, "text/plain", "No stations");
    return;
  }
  
  g_currentIndex = (g_currentIndex - 1 + g_stationCount) % g_stationCount;
  g_stationName = g_stations[g_currentIndex].name;
  g_stationUrl = g_stations[g_currentIndex].url;
  
  updateStationNameUI();
  saveLastStationToNVS();
  saveLastStationToSPIFFS();
  
  startPlaybackCurrent(true);
  if (g_playUrl.length()) audioSendConnect(g_playUrl);
  
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
  
  server.send(200, "application/json", "{\"index\":" + String(g_currentIndex) + ",\"name\":\"" + g_stationName + "\"}");
}


// --- ESP32 reboot endpoint ---
// POST /api/reset  -> triggers ESP.restart() shortly after replying
static void handleReset() {
  server.send(200, "application/json", "{\"ok\":true}");
  g_restartRequested = true;
  g_restartAtMs = millis() + 300; // give the TCP stack time to flush
}

// --- Buffer status endpoint ---
// GET /api/buffer -> { percent, filled, free, total }
static void handleGetBuffer() {
  size_t filled = 0;
  size_t freeb  = 0;

  // Lekérdezzük a buffer állapotot (az Audio lib belül kezeli a szinkront)
  filled = audio.inBufferFilled();
  freeb  = audio.inBufferFree();

  size_t total = filled + freeb;
  int percent = 0;
  if (total > 0) {
    percent = (int)((filled * 100) / total);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
  }

  // Keep globals in sync with what we report (optional, but handy)
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

static void startWebServer() {
  server.on("/", handleRoot);
  // Radio search page (served from SPIFFS)
  server.on("/search", HTTP_GET, handleSearch);
  server.on("/search.html", HTTP_GET, handleSearch);
  server.on("/api/stations", HTTP_GET, handleGetStations);
  server.on("/api/stations/add", HTTP_POST, handleAddStation);
  server.on("/api/stations/delete", HTTP_POST, handleDeleteStation);
  server.on("/api/stations/update", HTTP_POST, handleUpdateStation);
  server.on("/api/stations/move", HTTP_POST, handleMoveStation);
  // Accept common reorder endpoints from different web UIs (drag-and-drop)
  server.on("/api/stations/reorder", HTTP_POST, handleMoveStation);
  server.on("/api/stations/order", HTTP_POST, handleMoveStation);
  server.on("/api/stations/sort", HTTP_POST, handleMoveStation);
  server.on("/api/stations/moveStation", HTTP_POST, handleMoveStation);

  server.on("/api/station", HTTP_POST, handleSetStation);
  server.on("/api/volume", HTTP_POST, handleSetVolume);
  server.on("/api/brightness", HTTP_GET, handleGetBrightness);
  server.on("/api/brightness", HTTP_POST, handleSetBrightness);
  server.on("/api/toggle", HTTP_POST, handleTogglePause);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/next", HTTP_POST, handleNextStation);
  server.on("/api/track_next", HTTP_POST, handleTrackNext);
  server.on("/api/track_prev", HTTP_POST, handleTrackPrev);
  server.on("/api/prev", HTTP_POST, handlePrevStation);
  server.on("/api/buffer", HTTP_GET, handleGetBuffer);
  server.on("/api/reset", HTTP_POST, handleReset);

server.on("/upload", HTTP_GET, handleUploadPage);
server.on("/upload", HTTP_POST, handleUploadDone, handleFileUpload);
server.on("/api/fs/list", HTTP_GET, handleFsList);

  
  server.begin();
  Serial.println("Web server started");
  Serial.print("Connect to http://");
  Serial.println(WiFi.localIP());
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

  const String title = "Indul a rádió...";
  const String base  = "WiFi csatlakozás";

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.loadFont(fontTitle.c_str());
  const int hTitle = tft.fontHeight();
  const int wTitle = tft.textWidth(title);

  tft.loadFont(fontBody.c_str());
  const int hBody  = tft.fontHeight();
  const int wBase  = tft.textWidth(base);
  const int wDots  = tft.textWidth("...");

  const int gap = 8;
  const int totalH = hTitle + gap + hBody;

  const int yTitle = (tft.height() - totalH) / 2;
  const int yBody  = yTitle + hTitle + gap;

  const int xBody = (tft.width() - (wBase + wDots)) / 2;

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

  const int xDots = xBody + wBase;
  tft.fillRect(xDots, yBody - 2, wDots + 4, hBody + 4, TFT_BLACK);

  tft.setCursor(xDots, yBody);
  tft.setTextColor(c1, TFT_BLACK); tft.print(".");
  tft.setTextColor(c2, TFT_BLACK); tft.print(".");
  tft.setTextColor(c3, TFT_BLACK); tft.print(".");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}
void app_setup() {
  WiFiClientSecure client;
  client.setInsecure();   // HTTPS tanúsítványok kikapcsolása
  
  bl_init_pwm();
  bl_init_pwm();
  input_rotary_init(ENC_A, ENC_B, ENC_BTN, encoderISR, &g_encHist);


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
  drawStartupScreen(0);


  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
{
  WiFi.persistent(false);
  esp_wifi_clear_fast_connect();
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);
  delay(50);

  String ssid, pass;
  g_haveWiFiCreds = loadWiFiCredsFromSPIFFS(ssid, pass);
  if (g_haveWiFiCreds) {
    g_savedSsid = ssid;
    g_savedPass = pass;
  }

  bool ok = false;
  if (g_haveWiFiCreds) {
    ok = connectWiFi(g_savedSsid, g_savedPass, 12000);
  }

  if (!ok) {
    Serial.println("[WiFi] No saved creds or connect failed -> starting config portal");
    startWiFiConfigPortal();
  }

  g_wifiWasConnected = (WiFi.status() == WL_CONNECTED);
}

  
uint8_t phase = 0;
while (WiFi.status() != WL_CONNECTED) {
  drawStartupScreen(phase);

  phase++;
  if (phase > 3) phase = 1;
  delay(350);
}
  Serial.println("\nWiFi csatlakozva");

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
    u.FP_20    = &FP_20;
    u.FP_SB_20 = &FP_SB_20;

    u.wifiConnectedAtMs = &g_wifiConnectedAt;
    ui_display_bind(u);
  }

  initSprites();
  logMemorySnapshot("after sprites");
  drawStaticUI();

  // VU meter init (audio hook fogja etetni)
  vu_init();
  ui_drawVuMeter(0, 0, 0, 0);

  #define I2S_MCLK 15   // válassz szabad GPIO-t

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  Audio::audio_info_callback = my_audio_info;

  // Audio parancs sor (queue)
  audioQ = xQueueCreate(6, sizeof(AudioCmd));

  xTaskCreatePinnedToCore(
    audioTask,
    "audioTask",
    AUDIO_TASK_STACK,
    nullptr,
    AUDIO_TASK_PRIORITY,
    &audioTaskHandle,
    AUDIO_TASK_CORE
  );

  serialLogf("[TASK] loopTask core=%d, audioTask core=%d\n", xPortGetCoreID(), AUDIO_TASK_CORE);
  logMemorySnapshot("after audio task");

  audioSendVolume(g_Volume);
  startPlaybackCurrent(true);
  if (g_playUrl.length()) audioSendConnect(g_playUrl);

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

  nctx.handleWiFiReconnect = handleWiFiReconnect;

  net_server_poll(nctx);

  InputRotaryCtx ictx;
  ictx.encDelta      = &g_encDelta;

  ictx.mode          = (uint8_t*)&g_mode;
  ictx.volume        = &g_Volume;
  ictx.menuIndex     = &g_menuIndex;
  ictx.stationCount  = &g_stationCount;

  ictx.modePlay      = (uint8_t)MODE_PLAY;
  ictx.volMin        = VOLUME_MIN;
  ictx.volMax        = VOLUME_MAX;
  ictx.pulsesPerStep = ENC_PULSES_PER_STEP;

  ictx.onVolumeChanged = updateVolumeOnly;
  ictx.onMenuChanged   = drawMenuScreen;
  ictx.sendVolume       = audioSendVolume;

  input_rotary_apply(ictx);

  handleButton();
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

static uint32_t lastReconnectAttemptMs = 0;

if (g_needStreamReconnect && WiFi.status() == WL_CONNECTED) {
  uint32_t now = millis();

  // max 1 próbálkozás 3 másodpercenként
  if (now - lastReconnectAttemptMs >= 3000) {
    lastReconnectAttemptMs = now;

    g_needStreamReconnect = false;

    if (!g_paused && g_stationUrl.length()) {
      startPlaybackCurrent(true);
      if (g_playUrl.length()) {
        Serial.printf("[AUDIO] reconnect -> %s\n", g_playUrl.c_str());
        audioSendConnect(g_playUrl);
      }
    }
  }
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