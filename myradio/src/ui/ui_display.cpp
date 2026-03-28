#include "ui_display.h"
#include "ui_bottom_bar.h"
#if !defined(SSD1322)
#include "../../logo_rgb565_60x60.h"
#endif

static UIDisplayCtx C;

#if !defined(SSD1322)
static void vu_area(int& x, int& y, int& w, int& h);
static inline int clamp100_i(int v){ if (v < 0) return 0; if (v > 100) return 100; return v; }
static void draw_vu_boombox_static();
static void draw_vu_boombox_dynamic(int lvlL, int lvlR, int peakL, int peakR);
static void draw_vu_boombox(int lvlL, int lvlR, int peakL, int peakR);
#endif

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

#if defined(SSD1322)
template <typename T>
static void applyDisplayFont(T& dev, const String* fontPath) {
  if (fontPath && fontPath->length()) dev.loadFont(fontPath->c_str());
  else dev.setFont((const GFXfont*)nullptr);
  dev.setTextSize(1);
  dev.setTextWrap(false);
}
#endif

void ui_display_bind(const UIDisplayCtx& ctx) {
  C = ctx;
  ui_bottom_bar_bind(ctx);
}

void ui_drawWifiIcon(bool connected) { ui_bottom_bar_drawWifiIcon(connected); }
void ui_updateWifiIconOnly() { ui_bottom_bar_updateWifiIconOnly(); }
void ui_drawBufferIndicator(int percent) { ui_bottom_bar_drawBufferIndicator(percent); }
void ui_updateBufferIndicatorOnly(int percent) { ui_bottom_bar_updateBufferIndicatorOnly(percent); }
void ui_drawBottomBar(int volume, int bufferPercent, bool wifiConnected) { ui_bottom_bar_drawBottomBar(volume, bufferPercent, wifiConnected); }

#if !defined(SSD1322)
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
#endif

int ui_display_bottomBaseline() {
#if defined(SSD1322)
  return (C.H && *C.H > 0) ? (*C.H - 1) : 0;
#else
  if (g_vu.valid) {
    int b = g_vu.y + g_vu.h - 2;
    return (b > 0) ? b : 0;
  }
  return (*C.H) ? (*C.H - 1) : 0;
#endif
}

int ui_display_miniPufWidth() {
#if defined(SSD1322)
  return 12;
#else
  return 14;
#endif
}

void ui_display_drawMiniPuf(int x, int yTop, uint16_t col) {
  if (!C.tft) return;
#if defined(SSD1322)
  for (int i = 0; i < 6; ++i) {
    int h = (i % 2 == 0) ? 4 : 6;
    C.tft->fillRect(x + i * 2, yTop + (6 - h), 1, h, col);
  }
#else
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
#endif
}

#if !defined(SSD1322)
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
  C.tft->drawRoundRect(x, y, w, h, rr, TFT_WHITE);

  const int pad = max(2, (3 * s100) / 100);
  const int speakerPad = max(6, (10 * s100) / 100);
  const int speakerR = max(3, (4 * s100) / 100);

  int cy = y + h / 2;
  int spLX = x + pad + speakerPad;
  int spRX = x + w - pad - speakerPad;

  C.tft->drawCircle(spLX, cy, speakerR, TFT_WHITE);
  C.tft->fillCircle(spLX, cy, 1, TFT_WHITE);
  C.tft->drawCircle(spRX, cy, speakerR, TFT_WHITE);
  C.tft->fillCircle(spRX, cy, 1, TFT_WHITE);

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

  C.tft->drawRect(innerX - 1, y0 - 2, innerW + 2, totalBarsH + 4, TFT_WHITE);

  auto drawLabel = [&](int yy, char c) {
    int lx = innerX + 1;
    int ly = yy - 1;
    if (c == 'L') {
      C.tft->drawFastVLine(lx, ly, 6, TFT_WHITE);
      C.tft->drawFastHLine(lx, ly + 5, 4, TFT_WHITE);
    } else {
      C.tft->drawFastVLine(lx, ly, 6, TFT_WHITE);
      C.tft->drawFastHLine(lx, ly, 4, TFT_WHITE);
      C.tft->drawFastHLine(lx, ly + 2, 4, TFT_WHITE);
      C.tft->drawFastHLine(lx, ly + 5, 4, TFT_WHITE);
      C.tft->drawPixel(lx + 3, ly + 3, TFT_WHITE);
      C.tft->drawPixel(lx + 4, ly + 4, TFT_WHITE);
      C.tft->drawPixel(lx + 5, ly + 5, TFT_WHITE);
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
#endif

void ui_drawVuMeter(int lvlL, int lvlR, int peakL, int peakR) {
#if defined(SSD1322)
  (void)lvlL; (void)lvlR; (void)peakL; (void)peakR;
#else
  if (!ok()) return;
  draw_vu_boombox(lvlL, lvlR, peakL, peakR);
#endif
}

void ui_updateVuMeterOnly(int lvlL, int lvlR, int peakL, int peakR) {
#if defined(SSD1322)
  (void)lvlL; (void)lvlR; (void)peakL; (void)peakR;
#else
  if (!ok()) return;
  static int lastL = -999, lastR = -999, lastPL = -999, lastPR = -999;
  if (lastL != -999 && abs(lvlL - lastL) <= 1 && abs(lvlR - lastR) <= 1 &&
      abs(peakL - lastPL) <= 1 && abs(peakR - lastPR) <= 1) {
    return;
  }
  lastL = lvlL; lastR = lvlR; lastPL = peakL; lastPR = peakR;
  draw_vu_boombox(lvlL, lvlR, peakL, peakR);
#endif
}

void ui_invalidateVuMeter() {
#if !defined(SSD1322)
  g_vu.valid = false;
#endif
}

static void ui_drawTftLogoRebuilt() {
#if defined(SSD1322)
  return;
#else
  if (!C.tft || !C.W) return;
  constexpr int kLogoW = 60;
  constexpr int kLogoH = 60;
  constexpr int kPadRight = 4;
  constexpr int kPadTop = 4;

  const int logoX = *C.W - kLogoW - kPadRight;
  const int logoY = kPadTop;

  C.tft->fillRect(logoX, logoY, kLogoW, kLogoH, TFT_BLACK);

  uint16_t line[kLogoW];
  C.tft->startWrite();
  C.tft->setAddrWindow(logoX, logoY, kLogoW, kLogoH);
  for (int row = 0; row < kLogoH; ++row) {
    const int base = row * kLogoW;
    for (int col = 0; col < kLogoW; ++col) {
      line[col] = pgm_read_word(&logo_rgb565_60x60[base + col]);
    }
    C.tft->pushPixels(line, kLogoW, true);
  }
  C.tft->endWrite();
#endif
}

void ui_drawHeaderAndLogo(const String& header, int yHeader, int codecIconW) {
  if (!C.tft || !C.W || !C.H) return;

#if defined(SSD1322)
  applyDisplayFont(*C.tft, C.FP_SB_20);
  C.tft->setTextColor(TFT_WHITE, TFT_BLACK);
  const int textH = C.tft->fontHeight();
  const int leftEdge = codecIconW > 0 ? (codecIconW + 6) : 0;
  const int rightEdge = *C.W;
  const int clearW = rightEdge - leftEdge;
  if (clearW > 0) C.tft->fillRect(leftEdge, yHeader, clearW, textH + 4, TFT_BLACK);
  int drawX = leftEdge;
  int headerW = C.tft->textWidth(header.c_str());
  if (clearW > headerW) drawX = leftEdge + ((clearW - headerW) / 2);
  if (drawX < leftEdge) drawX = leftEdge;
  C.tft->setCursor(drawX, yHeader + textH);
  C.tft->print(header);
#else
  const int W = *C.W;
  constexpr int kLogoW = 60;
  constexpr int kLogoPadRight = 4;
  constexpr int kTextPad = 4;

  ui_drawTftLogoRebuilt();

  int tw = C.tft->textWidth(header.c_str());
  int leftBound  = 4 + codecIconW + 4;
  int rightBound = W - kLogoW - kLogoPadRight - kTextPad;
  int x = (W - tw) / 2;
  if (x < leftBound) x = leftBound;
  if (x + tw > rightBound) x = rightBound - tw;
  if (x < leftBound) x = leftBound;

  const int textClearX = leftBound;
  const int textClearW = rightBound - leftBound;
  const int textH = C.tft->fontHeight();
  if (textClearW > 0) C.tft->fillRect(textClearX, yHeader, textClearW, textH + 2, TFT_BLACK);

  C.tft->setCursor(x, yHeader);
  C.tft->print(header);
#endif
}
