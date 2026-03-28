#include "ui_station_selector.h"
#include "../core/text_utils.h"
#include "../lang/lang.h"

#if defined(SSD1322)
using oledgfx::middle_center;
using oledgfx::top_left;
using oledgfx::top_right;
#else
using lgfx::middle_center;
using lgfx::top_left;
using lgfx::top_right;
#endif

#ifndef TFT_WHITE
  #define TFT_WHITE  0xC618
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
static void applySelectorFont(T* surf, const String* fontPath) {
  if (!surf) return;
#if defined(SSD1322)
  if (fontPath && fontPath->length()) surf->loadFont(fontPath->c_str());
  else surf->setFont((const GFXfont*)nullptr);
#else
  if (fontPath && fontPath->length()) surf->loadFont(fontPath->c_str());
  else surf->unloadFont();
#endif
  surf->setTextSize(1);
  surf->setTextWrap(false);
}


static size_t utf8CharLen(uint8_t c) {
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

template <typename T>
static String clipTextOn(T* dev, const String& s, int maxW) {
  if (!dev || maxW <= 0) return "";
  const String fixed = text_fix(s.c_str());
  if (dev->textWidth(fixed.c_str()) <= maxW) return fixed;

  const char* dots = "...";
  const int dotsW = dev->textWidth(dots);
  if (dotsW >= maxW) return "";

  String out;
  out.reserve(fixed.length());
  for (size_t i = 0; i < fixed.length();) {
    const size_t n = utf8CharLen((uint8_t)fixed[i]);
    String trial = out + fixed.substring(i, i + n);
    if (dev->textWidth((trial + dots).c_str()) > maxW) break;
    out = trial;
    i += n;
  }
  return out + dots;
}

#if defined(SSD1322)
static constexpr int OLED_SELECTOR_ROWS = 5;
static constexpr int OLED_SIDE_RESERVED_W = 74;

static int oledListX() {
  if (!g_ctx.screenW) return 0;
  const int w = *g_ctx.screenW;
  const int reserved = OLED_SIDE_RESERVED_W * 2;
  const int minW = max(84, w / 3);
  const int bandW = max(minW, w - reserved);
  return max(0, (w - bandW) / 2);
}

static int oledListW() {
  if (!g_ctx.screenW) return 0;
  const int w = *g_ctx.screenW;
  const int reserved = OLED_SIDE_RESERVED_W * 2;
  const int minW = max(84, w / 3);
  return min(w, max(minW, w - reserved));
}
#endif

static int visibleRows() {
#if defined(SSD1322)
  return OLED_SELECTOR_ROWS;
#else
  if (!g_ctx.menuListHeight || !g_ctx.menuItemH || *g_ctx.menuItemH <= 0) return 1;
  int rows = *g_ctx.menuListHeight / *g_ctx.menuItemH;
  if (rows >= 5) return 5;
  if (rows >= 3) return 3;
  return 1;
#endif
}

static bool ensureMenuSprite() {
  if (!g_ctx.sprMenu || !g_ctx.screenW || !g_ctx.menuListHeight) return false;
#if defined(SSD1322)
  const int w = oledListW();
  const int h = (g_ctx.screenH && *g_ctx.screenH > 0) ? *g_ctx.screenH : *g_ctx.menuListHeight;
#else
  const int w = *g_ctx.screenW;
  const int h = *g_ctx.menuListHeight;
#endif
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
#if defined(SSD1322)
  const int fullH = (g_ctx.screenH && *g_ctx.screenH > 0) ? *g_ctx.screenH : *g_ctx.menuListHeight;
  if (count <= 0 || fullH <= 0 || *g_ctx.menuItemH <= 0) {
    const int x = oledListX();
    const int w = oledListW();
    g_ctx.tft->fillRect(x, 0, w, fullH, g_ctx.colorBg);
    return;
  }
#else
  if (count <= 0 || *g_ctx.menuListHeight <= 0 || *g_ctx.menuItemH <= 0) {
    g_ctx.tft->fillRect(0, *g_ctx.menuListTop, *g_ctx.screenW, *g_ctx.menuListHeight, g_ctx.colorBg);
    return;
  }
#endif

  const int rows = visibleRows();
  const int halfRows = rows / 2;
#if defined(SSD1322)
  const int listX = oledListX();
  const int listW = oledListW();
  const int listH = (g_ctx.screenH && *g_ctx.screenH > 0) ? *g_ctx.screenH : *g_ctx.menuListHeight;
  const int activeY = 2 * (*g_ctx.menuItemH);
  *g_ctx.menuNameY = activeY;
  const int sideMaxW = listW - 6;
#else
  const int activeY = ((*g_ctx.menuListHeight - *g_ctx.menuItemH) / 2);
  *g_ctx.menuNameY = *g_ctx.menuListTop + activeY - 5;
  const int sideMaxW = *g_ctx.screenW - 24;
#endif
  auto drawTo = [&](auto* surf) {
#if defined(SSD1322)
    surf->fillRect(0, 0, listW, listH, g_ctx.colorBg);
#else
    surf->fillRect(0, 0, *g_ctx.screenW, *g_ctx.menuListHeight, g_ctx.colorBg);
#endif

    for (int row = -halfRows; row <= halfRows; ++row) {
      const int idx = (*g_ctx.menuIndex + row + count) % count;
#if defined(SSD1322)
      const int rowIndex = row + halfRows;
      const int y = rowIndex * (*g_ctx.menuItemH);
      if (y + *g_ctx.menuItemH <= 0 || y >= *g_ctx.menuListHeight) continue;
      const int textY = y + max(0, ((*g_ctx.menuItemH - surf->fontHeight()) / 2));
#else
      const int y = activeY + row * (*g_ctx.menuItemH);
      if (y + *g_ctx.menuItemH <= 0 || y >= *g_ctx.menuListHeight) continue;
      const int centerX = (*g_ctx.screenW) / 2;
      const int centerY = y + ((*g_ctx.menuItemH) / 2);
#endif

      if (row == 0) {
        applySelectorFont(surf, g_ctx.activeFontPath);
#if defined(SSD1322)
        String activeText = clipTextOn(surf, g_ctx.stations[idx].name, listW - 4);
        surf->setTextColor(g_ctx.colorActiveText, g_ctx.colorBg);
        surf->setTextDatum(middle_center);
        surf->drawString(activeText, listW / 2, y + ((*g_ctx.menuItemH) / 2));
        surf->setTextDatum(top_left);
#else
        String activeText = clipTextOn(surf, g_ctx.stations[idx].name, *g_ctx.screenW - 10);
        surf->setTextColor(g_ctx.colorActiveText, g_ctx.colorBg);
        surf->setTextDatum(middle_center);
        surf->drawString(activeText, centerX, centerY);
        surf->setTextDatum(top_left);
#endif
      } else {
        applySelectorFont(surf, g_ctx.labelFontPath);
        String name = clipTextOn(surf, g_ctx.stations[idx].name, sideMaxW);
        const uint16_t color = (abs(row) == 1) ? g_ctx.colorSideText : g_ctx.colorSideFarText;
        surf->setTextColor(color, g_ctx.colorBg);
#if defined(SSD1322)
        surf->setTextDatum(middle_center);
        surf->drawString(name, listW / 2, y + ((*g_ctx.menuItemH) / 2));
        surf->setTextDatum(top_left);
#else
        surf->setTextDatum(middle_center);
        surf->drawString(name, centerX, centerY);
        surf->setTextDatum(top_left);
#endif
      }
    }
  };

  const bool useSprite = ensureMenuSprite();
  if (useSprite) {
    drawTo(g_ctx.sprMenu);
#if defined(SSD1322)
    const int yOff = 0;
    g_ctx.sprMenu->pushSprite(listX, yOff);
    g_ctx.tft->display();
#else
    int yOff = *g_ctx.menuListTop - 9;
    if (g_ctx.tft && g_ctx.tft->height() == 240) yOff -= *g_ctx.menuItemH;
    g_ctx.sprMenu->pushSprite(0, yOff);
#endif
  } else {
    drawTo(g_ctx.tft);
  }
}

void ui_stationSelectorTick() {
  // Menüben a középső sor nem görget, így nem villog újrarajzolás közben.
}
