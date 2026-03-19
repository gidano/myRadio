#include "ui_station_selector.h"

#ifndef TFT_LIGHTGREY
  #define TFT_LIGHTGREY 0xC618
#endif
#ifndef TFT_DARKGREY
  #define TFT_DARKGREY  0x7BEF
#endif
#ifndef TFT_GOLD
  #define TFT_GOLD      0xFEA0
#endif

static UIStationSelectorCtx g_ctx;
static bool g_active = false;
static bool g_scroll = false;
static int32_t g_scrollX = 0;
static uint32_t g_lastTick = 0;

template <typename T>
static String clipTextOn(T* dev, const String& s, int maxW) {
  if (!dev || maxW <= 0) return "";
  if (dev->textWidth(s.c_str()) <= maxW) return s;

  const char* dots = "...";
  const int dotsW = dev->textWidth(dots);
  if (dotsW >= maxW) return "";

  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); ++i) {
    String trial = out + s[i];
    if (dev->textWidth((trial + dots).c_str()) > maxW) break;
    out += s[i];
  }
  return out + dots;
}

static int visibleRows() {
  if (!g_ctx.menuListHeight || !g_ctx.menuItemH || *g_ctx.menuItemH <= 0) return 1;
  int rows = *g_ctx.menuListHeight / *g_ctx.menuItemH;
  if (rows >= 5) return 5;
  if (rows >= 3) return 3;
  return 1;
}

static bool ensureMenuSprite() {
  if (!g_ctx.sprMenu || !g_ctx.screenW || !g_ctx.menuListHeight) return false;
  const int w = *g_ctx.screenW;
  const int h = *g_ctx.menuListHeight;
  if (w <= 0 || h <= 0) return false;

  if ((int)g_ctx.sprMenu->width() != w || (int)g_ctx.sprMenu->height() != h) {
    g_ctx.sprMenu->deleteSprite();
    g_ctx.sprMenu->setColorDepth(16);
    g_ctx.sprMenu->createSprite(w, h);
  }
  return (g_ctx.sprMenu->width() == (uint32_t)w && g_ctx.sprMenu->height() == (uint32_t)h);
}

void ui_stationSelectorInit(const UIStationSelectorCtx& ctx) {
  g_ctx = ctx;
}

void ui_stationSelectorBegin(int currentIndex) {
  g_active = true;
  g_scroll = false;
  g_scrollX = 0;
  g_lastTick = millis();
  if (g_ctx.menuIndex) *g_ctx.menuIndex = currentIndex;
  ensureMenuSprite();
}

void ui_stationSelectorEnd() {
  g_active = false;
  g_scroll = false;
  g_scrollX = 0;
}

bool ui_stationSelectorActive() { return g_active; }
int ui_stationSelectorSelected() { return g_ctx.menuIndex ? *g_ctx.menuIndex : 0; }

void ui_stationSelectorRotate(int steps) {
  if (!g_ctx.menuIndex || !g_ctx.stationCount || !g_active) return;
  const int count = *g_ctx.stationCount;
  if (count <= 0 || steps == 0) return;
  int idx = *g_ctx.menuIndex;
  while (steps > 0) { idx = (idx + 1) % count; --steps; }
  while (steps < 0) { idx = (idx - 1 + count) % count; ++steps; }
  *g_ctx.menuIndex = idx;
  g_scroll = false;
  g_scrollX = 0;
  g_lastTick = millis();
}

void ui_stationSelectorDraw() {
  if (!g_active || !g_ctx.tft || !g_ctx.stations || !g_ctx.stationCount || !g_ctx.menuIndex ||
      !g_ctx.screenW || !g_ctx.menuListTop || !g_ctx.menuListHeight || !g_ctx.menuItemH || !g_ctx.menuNameY) return;

  const int count = *g_ctx.stationCount;
  if (count <= 0 || *g_ctx.menuListHeight <= 0 || *g_ctx.menuItemH <= 0) {
    g_ctx.tft->fillRect(0, *g_ctx.menuListTop, *g_ctx.screenW, *g_ctx.menuListHeight, g_ctx.colorBg);
    return;
  }

  const int rows = visibleRows();
  const int halfRows = rows / 2;
  const int activeY = ((*g_ctx.menuListHeight - *g_ctx.menuItemH) / 2);
  *g_ctx.menuNameY = *g_ctx.menuListTop + activeY;

  const int sideMaxW = *g_ctx.screenW - 24;
  auto drawTo = [&](auto* surf) {
    surf->fillRect(0, 0, *g_ctx.screenW, *g_ctx.menuListHeight, g_ctx.colorBg);

    for (int row = -halfRows; row <= halfRows; ++row) {
      const int idx = (*g_ctx.menuIndex + row + count) % count;
      const int y = activeY + row * (*g_ctx.menuItemH);
      if (y + *g_ctx.menuItemH <= 0 || y >= *g_ctx.menuListHeight) continue;

      const int centerX = (*g_ctx.screenW) / 2;
      const int centerY = y + ((*g_ctx.menuItemH) / 2);

      if (row == 0) {
        if (g_ctx.activeFontPath) surf->loadFont(g_ctx.activeFontPath->c_str());
        String activeText = clipTextOn(surf, g_ctx.stations[idx].name, *g_ctx.screenW - 20);
        surf->setTextColor(g_ctx.colorActiveText, g_ctx.colorBg);
        surf->setTextDatum(middle_center);
        surf->drawString(activeText, centerX, centerY);
        surf->setTextDatum(top_left);
      } else {
        if (g_ctx.labelFontPath) surf->loadFont(g_ctx.labelFontPath->c_str());
        String name = clipTextOn(surf, g_ctx.stations[idx].name, sideMaxW);
        const uint16_t color = (abs(row) == 1) ? g_ctx.colorSideText : g_ctx.colorSideFarText;
        surf->setTextColor(color, g_ctx.colorBg);
        surf->setTextDatum(middle_center);
        surf->drawString(name, centerX, centerY);
        surf->setTextDatum(top_left);
      }
    }
  };

  const bool useSprite = ensureMenuSprite();
  if (useSprite) {
    drawTo(g_ctx.sprMenu);
    int yOff = *g_ctx.menuListTop;
    if (g_ctx.tft && g_ctx.tft->height() == 240) yOff -= *g_ctx.menuItemH;
    g_ctx.sprMenu->pushSprite(0, yOff);
  } else {
    drawTo(g_ctx.tft);
  }
}

void ui_stationSelectorTick() {
  // Menüben a középső sor nem görget, így nem villog újrarajzolás közben.
}
