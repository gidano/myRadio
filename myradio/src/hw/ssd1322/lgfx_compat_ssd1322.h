#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "SSD1322.h"

#ifndef TFT_BLACK
#define TFT_BLACK 0x0000
#endif
#ifndef TFT_WHITE
#define TFT_WHITE 0xFFFF
#endif
#ifndef TFT_RED
#define TFT_RED 0xF800
#endif
#ifndef TFT_GREEN
#define TFT_GREEN 0x07E0
#endif
#ifndef TFT_BLUE
#define TFT_BLUE 0x001F
#endif
#ifndef TFT_YELLOW
#define TFT_YELLOW 0xFFE0
#endif
#ifndef TFT_ORANGE
#define TFT_ORANGE 0xFD20
#endif
#ifndef TFT_CYAN
#define TFT_CYAN 0x07FF
#endif
#ifndef TFT_SILVER
#define TFT_SILVER 0xC618
#endif
#ifndef TFT_GOLD
#define TFT_GOLD 0xFEA0
#endif
#ifndef TFT_DARKGREY
#define TFT_DARKGREY 0x7BEF
#endif
#ifndef TFT_LIGHTGREY
#define TFT_LIGHTGREY 0xC618
#endif

namespace oledgfx {

enum textdatum_t { datum_top_left = 0, datum_middle_center = 1, datum_top_right = 2 };
static constexpr textdatum_t top_left = datum_top_left;
static constexpr textdatum_t middle_center = datum_middle_center;
static constexpr textdatum_t top_right = datum_top_right;

static inline uint8_t ssd1322_gray4_from_rgb565(uint16_t c) {
  const uint8_t r = ((c >> 11) & 0x1F) << 3;
  const uint8_t g = ((c >> 5) & 0x3F) << 2;
  const uint8_t b = (c & 0x1F) << 3;

  // Base luminance.
  int y = (r * 30 + g * 59 + b * 11) / 100;

  // OLED-specific contrast stretch so light UI elements separate more clearly.
  if (y <= 8) return 0;
  if (y >= 248) return 15;

  y = ((y - 8) * 255) / 240;        // normalize after black clamp
  y = (y * 9 + 128) >> 8;           // subtle gamma lift for darker mids
  y = (y * 255) / 9;                // back to 0..255-ish
  if (y < 0) y = 0;
  if (y > 255) y = 255;

  int g4 = (y * 15 + 127) / 255;

  // Keep a tiny deadband at the dark end to avoid gray haze.
  if (g4 <= 1) return 0;
  if (g4 >= 14) return 15;
  return (uint8_t)g4;
}

class LGFX_Device : public Jamis_SSD1322 {
 public:
  LGFX_Device();

  void init();
  void setBrightness(uint8_t value) { setContrast(value); }
  void setDisplayEnabled(bool on) { setDisplayOn(on); }
  void loadFont(const char* name);
  void unloadFont();
  void setFont(const GFXfont* f);
  int textWidth(const char* s);
  int textWidth(const String& s) { return textWidth(s.c_str()); }
  int fontHeight();
  void setTextDatum(textdatum_t d) { _datum = d; }
  void drawString(const String& s, int32_t x, int32_t y);
  void drawString(const char* s, int32_t x, int32_t y) { drawString(String(s), x, y); }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void fillScreen(uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
  void pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data);

  size_t print(const String& s);
  size_t print(const char* s);
  size_t print(int n);
  size_t println(const String& s);
  size_t println(const char* s);
  size_t println();
  size_t printf(const char* fmt, ...);

 private:
  bool _autoFlush = true;
  textdatum_t _datum = top_left;
  void* _vlw = nullptr;
};

class LGFX_Sprite : public GFXcanvas16 {
 public:
  explicit LGFX_Sprite(LGFX_Device* parent) : GFXcanvas16(1, 1), _parent(parent) {}
  void setColorDepth(int) {}
  void setPsram(bool) {}
  bool createSprite(int16_t w, int16_t h);
  void deleteSprite();
  void loadFont(const char* name);
  void unloadFont();
  void setFont(const GFXfont* f);
  int textWidth(const char* s);
  int textWidth(const String& s) { return textWidth(s.c_str()); }
  int fontHeight();
  void setTextDatum(textdatum_t d) { _datum = d; }
  void drawString(const String& s, int32_t x, int32_t y);
  void drawString(const char* s, int32_t x, int32_t y) { drawString(String(s), x, y); }
  void pushSprite(int32_t x, int32_t y);

 private:
  LGFX_Device* _parent = nullptr;
  textdatum_t _datum = top_left;
  void* _vlw = nullptr;
};

class LGFX : public LGFX_Device {};

}  // namespace oledgfx

using oledgfx::LGFX;
using oledgfx::LGFX_Device;
using oledgfx::LGFX_Sprite;
using oledgfx::middle_center;
using oledgfx::top_left;
using oledgfx::top_right;
