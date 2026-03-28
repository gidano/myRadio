#include "ui_bottom_bar.h"
#include "audio_icons/icon_speaker_24.h"
#include "ui_station_selector.h"

#if !defined(SSD1322)
#include <WiFi.h>
#endif

static UIDisplayCtx C;

static bool ok() {
  return (C.tft && C.W && C.H &&
          C.wifiX && C.wifiY && C.wifiW && C.wifiH &&
          C.yVol && C.FP_20 && C.FP_SB_20 &&
          C.wifiConnectedAtMs);
}

static void clearRect(int x, int y, int w, int h) {
  if (!ok() || w <= 0 || h <= 0) return;
  C.tft->fillRect(x, y, w, h, TFT_BLACK);
}

void ui_bottom_bar_bind(const UIDisplayCtx& ctx) { C = ctx; }

#if !defined(SSD1322)
static int wifiBarsFromRSSI(int rssi) {
  if (rssi <= -90) return 0;
  if (rssi <= -80) return 1;
  if (rssi <= -70) return 2;
  if (rssi <= -60) return 3;
  return 4;
}

static void ui_drawVolumeOnly(int volume) {
  if (ui_stationSelectorActive() || !ok()) return;

  if (volume < 0) volume = 0;
  if (volume > 100) volume = 100;

  C.tft->loadFont(C.FP_20->c_str());
  int h20 = C.tft->fontHeight() + 2;

  const int iconW = 24;
  const int iconH = 24;
  const int gap = 6;

  String volStr = String(volume < 10 ? "0" : "") + String(volume);
  C.tft->loadFont(C.FP_SB_20->c_str());
  int valueW = C.tft->textWidth(volStr.c_str());

  int iconX = 0;
  const int baseline = ui_display_bottomBaseline();
  int iconY = baseline - iconH;
  if (iconY < 0) iconY = 0;

  int valueX = iconX + iconW + gap;

  int clearX = 0;
  int clearY = (iconY - 2 < 0) ? 0 : (iconY - 2);
  int clearH = (h20 > iconH ? h20 : iconH) + 4;
  int clearW = valueX + valueW + 6;
  clearRect(clearX, clearY, clearW, clearH);

  C.tft->pushImage(iconX, iconY, iconW, iconH, icon_speaker_24);

  C.tft->loadFont(C.FP_SB_20->c_str());
  C.tft->setTextColor(TFT_WHITE, TFT_BLACK);
  int valueY = baseline - (C.tft->fontHeight());
  if (valueY < 0) valueY = 0;
  C.tft->setCursor(valueX, valueY);
  C.tft->print(volStr);
}
#endif

void ui_bottom_bar_drawWifiIcon(bool connected) {
#if defined(SSD1322)
  (void)connected;
#else
  if (ui_stationSelectorActive() || !ok()) return;

  clearRect(*C.wifiX, *C.wifiY, *C.wifiW, *C.wifiH);

  if (!connected) {
    C.tft->drawLine(*C.wifiX + 2, *C.wifiY + 2, *C.wifiX + *C.wifiW - 3, *C.wifiY + *C.wifiH - 3, TFT_RED);
    C.tft->drawLine(*C.wifiX + *C.wifiW - 3, *C.wifiY + 2, *C.wifiX + 2, *C.wifiY + *C.wifiH - 3, TFT_RED);
    return;
  }

  int rssi = WiFi.RSSI();
  int bars = wifiBarsFromRSSI(rssi);

  int baseX = *C.wifiX + 2;
  const int baseline = ui_display_bottomBaseline() - UI_DISPLAY_RIGHT_RAISE_PX;
  int baseY = baseline - 1;
  int barW  = 6;
  int gap   = 2;

  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 3;
    int x = baseX + i * (barW + gap);
    int y = baseY - h;
    uint16_t col = (i < bars) ? TFT_GREEN : TFT_WHITE;
    C.tft->fillRect(x, y, barW, h, col);
  }
#endif
}

void ui_bottom_bar_updateWifiIconOnly() {
#if defined(SSD1322)
#else
  if (ui_stationSelectorActive() || !ok()) return;

  static int lastConnected = -1;
  static int lastBars = -1;
  static int rssiAvg = -70;

  bool connected = (WiFi.status() == WL_CONNECTED);
  int bars = -1;

  if (connected) {
    if (*C.wifiConnectedAtMs != 0 && (millis() - *C.wifiConnectedAtMs) < 1500) {
      int rssi = WiFi.RSSI();
      if (rssi > -120) rssiAvg = rssi;
      bars = (lastBars >= 0) ? lastBars : wifiBarsFromRSSI(rssiAvg);
    } else {
      int rssi = WiFi.RSSI();
      if (rssi > -120) {
        rssiAvg = (rssiAvg * 7 + rssi) / 8;
      }
      bars = wifiBarsFromRSSI(rssiAvg);
    }
  }

  if ((int)connected == lastConnected && bars == lastBars) return;

  lastConnected = (int)connected;
  lastBars = bars;

  if (!connected) {
    ui_bottom_bar_drawWifiIcon(false);
    return;
  }

  clearRect(*C.wifiX, *C.wifiY, *C.wifiW, *C.wifiH);

  int baseX = *C.wifiX + 2;
  const int baseline = ui_display_bottomBaseline() - UI_DISPLAY_RIGHT_RAISE_PX;
  int baseY = baseline - 1;
  int barW  = 6;
  int gap   = 2;

  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 3;
    int x = baseX + i * (barW + gap);
    int y = baseY - h;
    uint16_t col = (i < bars) ? TFT_GREEN : TFT_WHITE;
    C.tft->fillRect(x, y, barW, h, col);
  }
#endif
}

void ui_bottom_bar_drawBufferIndicator(int percent) {
#if defined(SSD1322)
  (void)percent;
#else
  if (ui_stationSelectorActive() || !ok()) return;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  const int bufW = 40;
  const int bufH = 8;
  int bufX = *C.wifiX - bufW - 10;
  const int baseline = ui_display_bottomBaseline() - UI_DISPLAY_RIGHT_RAISE_PX;
  const int bottomY = baseline - 2;
  int bufY = bottomY - (bufH - 1);

  C.tft->drawRect(bufX, bufY, bufW, bufH, TFT_WHITE);
  C.tft->fillRect(bufX + 1, bufY + 1, bufW - 2, bufH - 2, TFT_BLACK);

  int fillW = (bufW - 2) * percent / 100;
  if (fillW > 0) {
    uint16_t fillColor;
    if (percent > 75) fillColor = TFT_GREEN;
    else if (percent > 40) fillColor = TFT_YELLOW;
    else fillColor = TFT_RED;
    C.tft->fillRect(bufX + 1, bufY + 1, fillW, bufH - 2, fillColor);
  }

  const int pW = ui_display_miniPufWidth();
  int pX = bufX - pW - 3;
  if (pX < 0) pX = 0;
  int pY = baseline - 6 - UI_DISPLAY_PUF_RAISE_PX;
  C.tft->fillRect(pX, pY, pW, 6, TFT_BLACK);
  ui_display_drawMiniPuf(pX, pY, TFT_WHITE);
#endif
}

void ui_bottom_bar_updateBufferIndicatorOnly(int percent) {
#if defined(SSD1322)
  (void)percent;
#else
  if (ui_stationSelectorActive() || !ok()) return;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  static int lastPct = -999;
  if (lastPct != -999 && abs(percent - lastPct) <= 1) return;
  lastPct = percent;

  const int bufW = 40;
  const int bufH = 8;
  int bufX = *C.wifiX - bufW - 10;
  const int baseline = ui_display_bottomBaseline() - UI_DISPLAY_RIGHT_RAISE_PX;
  const int bottomY = baseline - 2;
  int bufY = bottomY - (bufH - 1);

  int pW = ui_display_miniPufWidth();
  int pX = bufX - pW - 3;
  if (pX < 0) pX = 0;

  const int pY = baseline - 6 - UI_DISPLAY_PUF_RAISE_PX;
  int clearX = pX;
  int clearY = (pY < bufY) ? pY : bufY;
  int clearW = (bufX + bufW) - clearX + 2;
  int clearBottom = ((pY + 6) > (bufY + bufH)) ? (pY + 6) : (bufY + bufH);
  int clearH = clearBottom - clearY;
  if (clearW < 0) clearW = 0;
  if (clearH < 0) clearH = 0;

  clearRect(clearX, clearY, clearW, clearH);
  ui_bottom_bar_drawBufferIndicator(percent);
#endif
}

void ui_bottom_bar_drawBottomBar(int volume, int bufferPercent, bool wifiConnected) {
#if defined(SSD1322)
  (void)volume; (void)bufferPercent; (void)wifiConnected;
#else
  if (ui_stationSelectorActive() || !ok()) return;

  ui_drawVolumeOnly(volume);

  if (ui_display_bottomBaseline() >= (*C.H - 1)) {
    C.tft->loadFont(C.FP_20->c_str());
    return;
  }

  ui_bottom_bar_drawBufferIndicator(bufferPercent);
  ui_bottom_bar_drawWifiIcon(wifiConnected);

  C.tft->loadFont(C.FP_20->c_str());
#endif
}
