#include "ui_display.h"
#include "logo_rgb565_60x60.h"
#include "audio_icons/icon_speaker_24.h"


#include <WiFi.h>

static UIDisplayCtx C;

// Forward declarations for bottom bar helpers (defined later in this file)
static inline int bottomBaseline();
static inline int mini_puf_width();
static void drawMiniPUF(int x, int yTop, uint16_t col);


// Right-side (PUF/Buffer/WiFi) baseline correction to avoid bottom underline
static constexpr int RIGHT_RAISE_PX = 2;
static constexpr int PUF_RAISE_PX = 2; // PUF mini felirat finom emelés


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
}

static int wifiBarsFromRSSI(int rssi) {
  if (rssi <= -90) return 0;
  if (rssi <= -80) return 1;
  if (rssi <= -70) return 2;
  if (rssi <= -60) return 3;
  return 4;
}

void ui_drawWifiIcon(bool connected) {
  if (!ok()) return;

  clearRect(*C.wifiX, *C.wifiY, *C.wifiW, *C.wifiH);

  if (!connected) {
    C.tft->drawLine(*C.wifiX + 2, *C.wifiY + 2, *C.wifiX + *C.wifiW - 3, *C.wifiY + *C.wifiH - 3, TFT_RED);
    C.tft->drawLine(*C.wifiX + *C.wifiW - 3, *C.wifiY + 2, *C.wifiX + 2, *C.wifiY + *C.wifiH - 3, TFT_RED);
    return;
  }

  int rssi = WiFi.RSSI();
  int bars = wifiBarsFromRSSI(rssi);

  int baseX = *C.wifiX + 2;
    const int baseline = bottomBaseline() - RIGHT_RAISE_PX;
  int baseY = baseline - 1;
  int barW  = 6;
  int gap   = 2;

  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 3;
    int x = baseX + i * (barW + gap);
    int y = baseY - h;
    uint16_t col = (i < bars) ? TFT_GREEN : TFT_DARKGRAY;
    C.tft->fillRect(x, y, barW, h, col);
  }
}

void ui_updateWifiIconOnly() {
  if (!ok()) return;

  static int lastConnected = -1;
  static int lastBars = -1;
  static int rssiAvg = -70;

  bool connected = (WiFi.status() == WL_CONNECTED);
  int bars = -1;

  if (connected) {
    // Az első ~1.5s-ben az RSSI gyakran ugrál / -127-et ad. Ilyenkor ne "rögtön" pálcázzunk.
    if (*C.wifiConnectedAtMs != 0 && (millis() - *C.wifiConnectedAtMs) < 1500) {
      int rssi = WiFi.RSSI();
      if (rssi > -120) rssiAvg = rssi;
      bars = (lastBars >= 0) ? lastBars : wifiBarsFromRSSI(rssiAvg);
    } else {
      int rssi = WiFi.RSSI();
      if (rssi > -120) {
        // Low-pass: kisimítjuk az ugrálást, hogy a pálcák ne "idegesek" legyenek
        rssiAvg = (rssiAvg * 7 + rssi) / 8;
      }
      bars = wifiBarsFromRSSI(rssiAvg); // 0..4
    }
  }

  if ((int)connected == lastConnected && bars == lastBars) return;

  lastConnected = (int)connected;
  lastBars = bars;

  if (!connected) {
    ui_drawWifiIcon(false);
    return;
  }

  // Ugyanaz a layout, de a már kisimított bars értékkel rajzolunk.
  clearRect(*C.wifiX, *C.wifiY, *C.wifiW, *C.wifiH);

  int baseX = *C.wifiX + 2;
    const int baseline = bottomBaseline() - RIGHT_RAISE_PX;
  int baseY = baseline - 1;
  int barW  = 6;
  int gap   = 2;

  for (int i = 0; i < 4; i++) {
    int h = 4 + i * 3;
    int x = baseX + i * (barW + gap);
    int y = baseY - h;
    uint16_t col = (i < bars) ? TFT_GREEN : TFT_DARKGRAY;
    C.tft->fillRect(x, y, barW, h, col);
  }
}

// ------------------ Buffer indicator (bottom bar) ------------------
void ui_drawBufferIndicator(int percent) {
  if (!ok()) return;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  // Buffer ikon pozíciója (WiFi ikon bal oldalára) — ugyanaz mint a korábbi működő kód
  const int bufW = 40;
  const int bufH = 8;
  int bufX = *C.wifiX - bufW - 10;
  // NOTE: A WiFi pálcák alja egy pixel-lel feljebb “végződik”, ezért a puffer blokkot
  // 1px-szel még feljebb húzzuk, hogy vizuálisan egyvonalban legyen.
    // Right-side baseline (align buffer bottom to WiFi bars bottom)
    const int baseline = bottomBaseline() - RIGHT_RAISE_PX;
    const int bottomY = baseline - 2;  // WiFi bars visual bottom
    int bufY = bottomY - (bufH - 1);

  // Keret
  C.tft->drawRect(bufX, bufY, bufW, bufH, TFT_WHITE);

  // Belső törlés (csökkenéskor ne maradjon csík)
  C.tft->fillRect(bufX + 1, bufY + 1, bufW - 2, bufH - 2, TFT_BLACK);

  // Kitöltés
  int fillW = (bufW - 2) * percent / 100;
  if (fillW > 0) {
    uint16_t fillColor;
    if (percent > 75) fillColor = TFT_GREEN;
    else if (percent > 40) fillColor = TFT_YELLOW;
    else fillColor = TFT_RED;
    C.tft->fillRect(bufX + 1, bufY + 1, fillW, bufH - 2, fillColor);
  }

  
// "PUF" mini felirat balra (6px magas, mint a VU L/R jelzés)
const int pW = mini_puf_width();
int pX = bufX - pW - 3;
if (pX < 0) pX = 0;
int pY = baseline - 6 - PUF_RAISE_PX; // 6px magas (PUF feljebb)
// háttér törlés
C.tft->fillRect(pX, pY, pW, 6, TFT_BLACK);
drawMiniPUF(pX, pY, TFT_WHITE);
}

void ui_updateBufferIndicatorOnly(int percent) {
  if (!ok()) return;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  static int lastPct = -999;
  if (lastPct != -999 && abs(percent - lastPct) <= 1) return;
  lastPct = percent;

  // Buffer ikon pozíciója
  const int bufW = 40;
  const int bufH = 8;
  int bufX = *C.wifiX - bufW - 10;
    // Right-side baseline (align buffer bottom to WiFi bars bottom)
    const int baseline = bottomBaseline() - RIGHT_RAISE_PX;
    const int bottomY = baseline - 2;
    int bufY = bottomY - (bufH - 1);

  // "PUF" terület is legyen törölve
    int pW = mini_puf_width();
  int pX = bufX - pW - 3;
  if (pX < 0) pX = 0;

  int clearX = pX;
  // Clear a bit more vertically to remove any leftover underline pixels
  int clearY = bufY - 6;
  int clearW = (bufX + bufW) - clearX + 2;
  int clearH = bufH + 12;
  if (clearW < 0) clearW = 0;

  clearRect(clearX, clearY, clearW, clearH);
  ui_drawBufferIndicator(percent);
}

// ------------------ Bottom bar: Volume + Buffer + WiFi ------------------
static void ui_drawVolumeOnly(int volume) {
  if (!ok()) return;

  if (volume < 0) volume = 0;
  if (volume > 100) volume = 100;

  // Fonts
  C.tft->loadFont(C.FP_20->c_str());
  int h20 = C.tft->fontHeight() + 2;

  // Icon + value (replaces "Hangerő:" label)
  const int iconW = 24;
  const int iconH = 24;
  const int gap = 4;

  String volStr = String(volume < 10 ? "0" : "") + String(volume);
  C.tft->loadFont(C.FP_SB_20->c_str());
  int valueW = C.tft->textWidth(volStr.c_str());

  int iconX = 0;
    const int baseline = bottomBaseline();
  int iconY = baseline - iconH;
  if (iconY < 0) iconY = 0;

  int valueX = iconX + iconW + gap;

  int clearX = 0;
    int clearY = (iconY - 2 < 0) ? 0 : (iconY - 2);
  int clearH = (h20 > iconH ? h20 : iconH) + 4;
  int clearW = valueX + valueW + 6;
  clearRect(clearX, clearY, clearW, clearH);

  // Draw icon
  C.tft->pushImage(iconX, iconY, iconW, iconH, icon_speaker_24);

  // Draw value
  C.tft->loadFont(C.FP_SB_20->c_str());
  C.tft->setTextColor(TFT_GREEN, TFT_BLACK);
    int valueY = baseline - (C.tft->fontHeight());
  if (valueY < 0) valueY = 0;
  C.tft->setCursor(valueX, valueY);
  C.tft->print(volStr);
}


void ui_drawBottomBar(int volume, int bufferPercent, bool wifiConnected) {
  if (!ok()) return;

  // Részenként rajzolunk (villogásmentesen)
  ui_drawVolumeOnly(volume);
  ui_drawBufferIndicator(bufferPercent);
  ui_drawWifiIcon(wifiConnected);

  // Visszaállítjuk a megszokott fontot (app oldalon gyakran erre számítanak)
  C.tft->loadFont(C.FP_20->c_str());
}

// ------------------ VU meter (boombox style, Volume és Buffer közé) ------------------
static void vu_area(int& x, int& y, int& w, int& h) {
  // Cél: a "Hangerő" (bal) és a "PUF"+buffer (jobb) közötti sávban legyen.
  // Fontos: 320px szélességnél a VU ne lógjon bele a "PUF" feliratba sem.

  // Buffer: 40x8, gap=10 (ui_drawBufferIndicator)
  const int bufW = 40;
  const int gap = 10;
  const int sidePad = 10;

  // Buffer bal széle:
  int bufX = *C.wifiX - bufW - gap;

  // "PUF" bal széle (ugyanazzal a méréssel, mint a buffer rajzolásnál)
    int pW = mini_puf_width();
  int pX = bufX - pW - 3;
  if (pX < 0) pX = 0;

  // A VU jobb oldali limitje: a PUF felirat előtt hagyunk helyet
  int rightLimit = pX - sidePad;
  if (rightLimit < 0) rightLimit = 0;

  // Volume right edge (max): icon + "100"
  const int iconW = 24;
  const int gapV = 4;
  C.tft->loadFont(C.FP_SB_20->c_str());
  int valueW = C.tft->textWidth("100");
  int volRight = iconW + gapV + valueW + 8;

  int leftLimit = volRight + sidePad;

  int avail = rightLimit - leftLimit;
  if (avail < 0) avail = 0;

  // Alap cél szélesség (480-on jól mutat), de alkalmazkodunk a rendelkezésre álló helyhez
  int targetW = 140;
  if (targetW > avail) targetW = avail;

  // Minimum: ha nagyon kevés a hely, akkor is próbáljunk meg egy keskeny VU-t betenni
  const int minW = 60;
  if (targetW < minW) targetW = (avail >= minW ? minW : avail);

  w = targetW;
  x = leftLimit + (avail - targetW) / 2;

  // Magasság/pozíció: alsó sávban, a volume szöveghez igazítva.
  C.tft->loadFont(C.FP_20->c_str());
  int h20 = C.tft->fontHeight();
  h = h20 + 4;
  y = *C.yVol - 2;

  // Clamp a képernyőre
  if (x < 0) x = 0;
  if (x + w > *C.W) w = *C.W - x;
  if (y < 0) y = 0;
  if (y + h > *C.H) h = *C.H - y;
}

// A villogás elkerüléséhez a boombox "keretet" (statikus részeket) csak akkor rajzoljuk újra,
// ha a VU terület geometriája megváltozik. A dinamikus részek (sávok + peak) külön frissülnek.
struct VuBoomGeom {
  int x=0,y=0,w=0,h=0;
  int innerX=0, innerW=0;
  int barX=0, barW=0;
  int y0=0;
  int barH=0, barGap=0;
  bool valid=false;
};
static VuBoomGeom g_vu;
// --- Bottom bar baseline: VU keret alja ---
// A VU boombox geometriából számoljuk (ha még nincs valid, essünk vissza).
static inline int bottomBaseline() {
  // Align bottom bar elements to the visible bottom of the VU frame.
  // g_vu.h includes the outer frame thickness; subtract 2px to match the left-side baseline.
  if (g_vu.valid) {
    int b = g_vu.y + g_vu.h - 2;
    return (b > 0) ? b : 0;
  }
  return (*C.H) ? (*C.H - 1) : 0;
}

// --- Mini 6px magas "PUF" felirat (ugyanaz a stílus/méret mint a VU L/R jelzés) ---
static inline int mini_puf_width() {
  // P, U, F: 4px betű + 1px gap, utolsó gap nélkül => 4+1 + 4+1 + 4 = 14
  return 14;
}

static void drawMiniPUF(int x, int yTop, uint16_t col) {
  // 6px magas, 4px széles betűk, 1px oszlopköz
  auto vline = [&](int xx, int yy, int hh){ C.tft->drawFastVLine(xx, yy, hh, col); };
  auto hline = [&](int xx, int yy, int ww){ C.tft->drawFastHLine(xx, yy, ww, col); };
  int x0 = x;

  // P
  vline(x0,     yTop, 6);
  hline(x0,     yTop, 3);
  hline(x0,     yTop+2, 3);
  vline(x0+3,   yTop+1, 2);

  x0 += 5; // 4 + gap

  // U
  vline(x0,     yTop, 5);
  vline(x0+3,   yTop, 5);
  hline(x0,     yTop+5, 4);

  x0 += 5;

  // F
  vline(x0,     yTop, 6);
  hline(x0,     yTop, 4);
  hline(x0,     yTop+2, 3);
}

static inline int clamp100_i(int v){ if (v < 0) return 0; if (v > 100) return 100; return v; }

static void draw_vu_boombox_static() {
  int x, y, w, h;
  vu_area(x, y, w, h);

  // ha a geometriánk ugyanaz, nincs dolgunk
  if (g_vu.valid && g_vu.x==x && g_vu.y==y && g_vu.w==w && g_vu.h==h) return;

  g_vu = VuBoomGeom{};
  g_vu.x=x; g_vu.y=y; g_vu.w=w; g_vu.h=h;
  // Skálázás: 480px szélességen ez a layout alapból jól néz ki.
  // 320px szélességnél a rendelkezésre álló VU szélesség kisebb -> arányosan csökkentünk.
  int s100 = (w * 100) / 140;           // 140 az "alap" cél szélesség
  if (s100 < 65) s100 = 65;             // ne legyen túl apró
  if (s100 > 120) s100 = 120;

  g_vu.barH   = max(3, (4 * s100) / 100);
  g_vu.barGap = max(2, (3 * s100) / 100);

  // teljes terület tiszta (csak geometriaváltáskor)
  clearRect(x, y, w, h);

  // boombox keret
  int rr = max(3, (4 * s100) / 100);
  C.tft->drawRoundRect(x, y, w, h, rr, TFT_DARKGRAY);

  const int pad = max(2, (3 * s100) / 100);
  const int speakerPad = max(6, (10 * s100) / 100);
  const int speakerR = max(3, (4 * s100) / 100);

  int cy = y + h / 2;
  int spLX = x + pad + speakerPad;
  int spRX = x + w - pad - speakerPad;

  // "hangszórók" (statikus)
  C.tft->drawCircle(spLX, cy, speakerR, TFT_DARKGRAY);
  C.tft->fillCircle(spLX, cy, 1, TFT_DARKGRAY);
  C.tft->drawCircle(spRX, cy, speakerR, TFT_DARKGRAY);
  C.tft->fillCircle(spRX, cy, 1, TFT_DARKGRAY);

  // "kazetta ablak" (statikus keretek + L/R felirat)
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

  // L/R jelzés (statikus)
  auto drawLabel = [&](int yy, char c) {
    int lx = innerX + 1;
    int ly = yy - 1;
    if (c == 'L') {
      C.tft->drawFastVLine(lx, ly, 6, TFT_LIGHTGRAY);
      C.tft->drawFastHLine(lx, ly + 5, 4, TFT_LIGHTGRAY);
    } else { // 'R'
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

  // cache
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

  // Csak a dinamikus területet töröljük (a keret/ikonok nem villognak)
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
  // Statikus részek csak geometriaváltáskor, dinamikus részek minden frissítéskor.
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
  // kis küszöb, hogy ne villogjon
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
  // jobb felső logo
  drawLogo60x60(*C.tft, W - logoW - 4, 4);

  // fejléc szöveg középre a két ikon közé (ne lógjon rá az ikonokra)
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