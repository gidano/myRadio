#pragma once
#include <Arduino.h>
#include "../../Lovyan_config.h"  // LGFX

// ui_display: kijelzőrajzolás (első refaktor lépés: WiFi ikon kiszervezése)

struct UIDisplayCtx {
  LGFX* tft = nullptr;

  // screen metrics
  int* W = nullptr;
  int* H = nullptr;

  // WiFi icon layout
  int* wifiX = nullptr;
  int* wifiY = nullptr;
  int* wifiW = nullptr;
  int* wifiH = nullptr;

  // Bottom bar layout
  int* yVol = nullptr;

  // Font paths (LGFX loadFont)
  String* FP_20 = nullptr;
  String* FP_SB_20 = nullptr;

  // millis() amikor WiFi felcsatlakozott (RSSI "stabilizálás" miatt)
  uint32_t* wifiConnectedAtMs = nullptr;
};


// Shared bottom-bar helpers used by the split UI modules
constexpr int UI_DISPLAY_RIGHT_RAISE_PX = 2;
constexpr int UI_DISPLAY_PUF_RAISE_PX = 2;
int ui_display_bottomBaseline();
int ui_display_miniPufWidth();
void ui_display_drawMiniPuf(int x, int yTop, uint16_t col);

// Egyszer meghívod app_setup végén, miután a layout (wifiX, wifiY...) már ki van számolva.
void ui_display_bind(const UIDisplayCtx& ctx);

// WiFi ikon (belső clear + rajz)
void ui_drawWifiIcon(bool connected);

// Villogásmentes, csak akkor rajzol, ha változott a kapcsolat / pálcák száma.
void ui_updateWifiIconOnly();

// Buffer indicator (bottom bar)
// percent: 0..100
void ui_drawBufferIndicator(int percent);
// Villogásmentes frissítés: csak akkor rajzol, ha a százalék érdemben változott
void ui_updateBufferIndicatorOnly(int percent);


// Bottom bar (Volume + Buffer + WiFi)
void ui_drawBottomBar(int volume, int bufferPercent, bool wifiConnected);

// VU meter (kicsi L/R sávok a bottom bar felett)
// lvl/peak: 0..100
void ui_drawVuMeter(int lvlL, int lvlR, int peakL, int peakR);
// Villogásmentes frissítés: csak akkor rajzol, ha érdemben változott
void ui_updateVuMeterOnly(int lvlL, int lvlR, int peakL, int peakR);
// Érvényteleníti a VU cache-t, így a következő rajzoláskor a keret is újraépül.
void ui_invalidateVuMeter();

// Header + right-top TFT logo (static UI)
void ui_drawHeaderAndLogo(const String& header, int yHeader, int codecIconW);
