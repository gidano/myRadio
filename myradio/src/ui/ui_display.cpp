#include "ui_display.h"
#include "ui_bottom_bar.h"
#include "logo_rgb565_60x60.h"

static UIDisplayCtx C;

static void vu_area(int& x, int& y, int& w, int& h);
static inline int clamp100_i(int v){ if (v < 0) return 0; if (v > 100) return 100; return v; }
static void draw_vu_boombox_static();
static void draw_vu_boombox_dynamic(int lvlL, int lvlR, int peakL, int peakR);
static void draw_vu_boombox(int lvlL, int lvlR, int peakL, int peakR);

static bool ok() {
  return (C.tft && C.W && C.H &&
          C.wifiX && C.wifiY && C.wifiW && C.wifiH &&
          C.yVol && C.FP_20 && C.FP_SB_20 &&
          C.wifiConnectedAtMs);
}

static void clearRect(int x, int y, int w, int h) {
  if (!ok()) return;
  C.tft->fillRect(x, y, w, h, TFT_BLACK);
}

void ui_display_bind(const UIDisplayCtx& ctx) {
  C = ctx;
  ui_bottom_bar_bind(ctx);
}

void ui_drawWifiIcon(bool connected) {
  ui_bottom_bar_drawWifiIcon(connected);
}

void ui_updateWifiIconOnly() {
  ui_bottom_bar_updateWifiIconOnly();
}

void ui_drawBufferIndicator(int percent) {
  ui_bottom_bar_drawBufferIndicator(percent);
}

void ui_updateBufferIndicatorOnly(int percent) {
  ui_bottom_bar_updateBufferIndicatorOnly(percent);
}

void ui_drawBottomBar(int volume, int bufferPercent, bool wifiConnected) {
  ui_bottom_bar_drawBottomBar(volume, bufferPercent, wifiConnected);
}

static void vu_area(int& x, int& y, int& w, int& h) {
  const int bufW = 40;
  const int gap = 10;
  const int sidePad = 10;

  int bufX = *C.wifiX - bufW - gap;

  int pW = ui_display_miniPufWidth();
  int pX = bufX - pW - 3;
  if (pX < 0) pX = 0;

  int rightLimit = pX - sidePad;
  if (rightLimit < 0) rightLimit = 0;

  const int iconW = 24;
  const int gapV = 4;
  C.tft->loadFont(C.FP_SB_20->c_str());
  int valueW = C.tft->textWidth("100");
  int volRight = iconW + gapV + valueW + 8;

  int leftLimit = volRight + sidePad;

  int avail = rightLimit - leftLimit;
  if (avail < 0) avail = 0;

  int targetW = 140;
  if (targetW > avail) targetW = avail;

  const int minW = 60;
  if (targetW < minW) targetW = (avail >= minW ? minW : avail);

  w = targetW;
  x = leftLimit + (avail - targetW) / 2 - 5;

  C.tft->loadFont(C.FP_20->c_str());
  int h20 = C.tft->fontHeight();
  h = h20 + 4;
  y = *C.yVol - 2;

  if (x < 0) x = 0;
  if (x + w > *C.W) w = *C.W - x;
  if (y < 0) y = 0;
  if (y + h > *C.H) h = *C.H - y;
}

struct VuBoomGeom {
  int x=0,y=0,w=0,h=0;
  int innerX=0, innerW=0;
  int barX=0, barW=0;
  int y0=0;
  int barH=0, barGap=0;
  bool valid=false;
};
static VuBoomGeom g_vu;

int ui_display_bottomBaseline() {
  if (g_vu.valid) {
    int b = g_vu.y + g_vu.h - 2;
    return (b > 0) ? b : 0;
  }
  return (*C.H) ? (*C.H - 1) : 0;
}

int ui_display_miniPufWidth() {
  return 14;
}

void ui_display_drawMiniPuf(int x, int yTop, uint16_t col) {
  auto vline = [&](int xx, int yy, int hh){ C.tft->drawFastVLine(xx, yy, hh, col); };
  auto hline = [&](int xx, int yy, int ww){ C.tft->drawFastHLine(xx, yy, ww, col); };
  int x0 = x;

  vline(x0,     yTop, 6);
  hline(x0,     yTop, 3);
  hline(x0,     yTop+2, 3);
  vline(x0+3,   yTop+1, 2);

  x0 += 5;

  vline(x0,     yTop, 5);
  vline(x0+3,   yTop, 5);
  hline(x0,     yTop+5, 4);

  x0 += 5;

  vline(x0,     yTop, 6);
  hline(x0,     yTop, 4);
  hline(x0,     yTop+2, 3);
}

static void draw_vu_boombox_static() {
  int x, y, w, h;
  vu_area(x, y, w, h);

  if (g_vu.valid && g_vu.x==x && g_vu.y==y && g_vu.w==w && g_vu.h==h) return;

  g_vu = VuBoomGeom{};
  g_vu.x=x; g_vu.y=y; g_vu.w=w; g_vu.h=h;
  int s100 = (w * 100) / 140;
  if (s100 < 65) s100 = 65;
  if (s100 > 120) s100 = 120;

  g_vu.barH   = max(3, (4 * s100) / 100);
  g_vu.barGap = max(2, (3 * s100) / 100);

  clearRect(x, y, w, h);

  int rr = max(3, (4 * s100) / 100);
  C.tft->drawRoundRect(x, y, w, h, rr, TFT_DARKGRAY);

  const int pad = max(2, (3 * s100) / 100);
  const int speakerPad = max(6, (10 * s100) / 100);
  const int speakerR = max(3, (4 * s100) / 100);

  int cy = y + h / 2;
  int spLX = x + pad + speakerPad;
  int spRX = x + w - pad - speakerPad;

  C.tft->drawCircle(spLX, cy, speakerR, TFT_DARKGRAY);
  C.tft->fillCircle(spLX, cy, 1, TFT_DARKGRAY);
  C.tft->drawCircle(spRX, cy, speakerR, TFT_DARKGRAY);
  C.tft->fillCircle(spRX, cy, 1, TFT_DARKGRAY);

  int innerGap = max(4, (6 * s100) / 100);
  int innerX = spLX + speakerR + innerGap;
  int innerW = (spRX - speakerR - innerGap) - innerX;
  if (innerW < 20) innerW = 20;

  const int labelW = max(7, (8 * s100) / 100);
  int barX = innerX + labelW;
  int barW = innerW - labelW;
  if (barW < 10) { barX = innerX; barW = innerW; }

  int totalBarsH = g_vu.barH + g_vu.barGap + g_vu.barH;
  int y0 = y + (h - totalBarsH) / 2;

  C.tft->drawRect(innerX - 1, y0 - 2, innerW + 2, totalBarsH + 4, TFT_DARKGRAY);

  auto drawLabel = [&](int yy, char c) {
    int lx = innerX + 1;
    int ly = yy - 1;
    if (c == 'L') {
      C.tft->drawFastVLine(lx, ly, 6, TFT_LIGHTGRAY);
      C.tft->drawFastHLine(lx, ly + 5, 4, TFT_LIGHTGRAY);
    } else {
      C.tft->drawFastVLine(lx, ly, 6, TFT_LIGHTGRAY);
      C.tft->drawFastHLine(lx, ly, 4, TFT_LIGHTGRAY);
      C.tft->drawFastHLine(lx, ly + 2, 4, TFT_LIGHTGRAY);
      C.tft->drawFastHLine(lx, ly + 5, 4, TFT_LIGHTGRAY);
      C.tft->drawPixel(lx + 3, ly + 3, TFT_LIGHTGRAY);
      C.tft->drawPixel(lx + 4, ly + 4, TFT_LIGHTGRAY);
      C.tft->drawPixel(lx + 5, ly + 5, TFT_LIGHTGRAY);
    }
  };
  drawLabel(y0, 'L');
  drawLabel(y0 + g_vu.barH + g_vu.barGap, 'R');

  g_vu.innerX = innerX;
  g_vu.innerW = innerW;
  g_vu.barX = barX;
  g_vu.barW = barW;
  g_vu.y0 = y0;
  g_vu.valid = true;
}

static void draw_vu_boombox_dynamic(int lvlL, int lvlR, int peakL, int peakR) {
  if (!g_vu.valid) draw_vu_boombox_static();
  if (!g_vu.valid) return;

  lvlL  = clamp100_i(lvlL);
  lvlR  = clamp100_i(lvlR);
  peakL = clamp100_i(peakL);
  peakR = clamp100_i(peakR);

  const int barH = g_vu.barH;
  const int barGap = g_vu.barGap;
  const int barX = g_vu.barX;
  const int barW = g_vu.barW;
  const int y0 = g_vu.y0;

  C.tft->fillRect(barX, y0, barW, barH, TFT_BLACK);
  C.tft->fillRect(barX, y0 + barH + barGap, barW, barH, TFT_BLACK);

  int lW = (barW * lvlL) / 100;
  int rW = (barW * lvlR) / 100;

  auto fillLevel = [&](int yy, int fillW){
    int gW = min(fillW, (barW * 70) / 100);
    if (gW > 0) C.tft->fillRect(barX, yy, gW, barH, TFT_GREEN);
    int yW = min(max(fillW - gW, 0), (barW * 20) / 100);
    if (yW > 0) C.tft->fillRect(barX + gW, yy, yW, barH, TFT_YELLOW);
    int rW2 = fillW - gW - yW;
    if (rW2 > 0) C.tft->fillRect(barX + gW + yW, yy, rW2, barH, TFT_RED);
  };
  fillLevel(y0, lW);
  fillLevel(y0 + barH + barGap, rW);

  auto drawPeak = [&](int yy, int peak){
    int px = barX + (barW * peak) / 100;
    if (px < barX) px = barX;
    if (px > barX + barW - 1) px = barX + barW - 1;
    C.tft->drawFastVLine(px, yy, barH, TFT_WHITE);
  };
  drawPeak(y0, peakL);
  drawPeak(y0 + barH + barGap, peakR);
}

static void draw_vu_boombox(int lvlL, int lvlR, int peakL, int peakR) {
  draw_vu_boombox_static();
  draw_vu_boombox_dynamic(lvlL, lvlR, peakL, peakR);
}

void ui_drawVuMeter(int lvlL, int lvlR, int peakL, int peakR) {
  if (!ok()) return;
  draw_vu_boombox(lvlL, lvlR, peakL, peakR);
}

void ui_updateVuMeterOnly(int lvlL, int lvlR, int peakL, int peakR) {
  if (!ok()) return;
  static int lastL = -999, lastR = -999, lastPL = -999, lastPR = -999;
  if (lastL != -999 && abs(lvlL - lastL) <= 1 && abs(lvlR - lastR) <= 1 &&
      abs(peakL - lastPL) <= 1 && abs(peakR - lastPR) <= 1) {
    return;
  }
  lastL = lvlL; lastR = lvlR; lastPL = peakL; lastPR = peakR;
  draw_vu_boombox(lvlL, lvlR, peakL, peakR);
}

void ui_invalidateVuMeter() {
  g_vu.valid = false;
}

void ui_drawHeaderAndLogo(const String& header, int yHeader, int codecIconW, int logoW) {
  if (!C.tft || !C.W) return;
  const int W = *C.W;
  drawLogo60x60(*C.tft, W - logoW - 4, 4);

  int tw = C.tft->textWidth(header.c_str());
  int leftBound  = 4 + codecIconW + 4;
  int rightBound = W - (logoW + 4) - 4;
  int x = (W - tw) / 2;
  if (x < leftBound) x = leftBound;
  if (x + tw > rightBound) x = rightBound - tw;
  if (x < leftBound) x = leftBound;

  C.tft->setCursor(x, yHeader);
  C.tft->print(header);
}
