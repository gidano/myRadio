#define MYRADIO_SKIP_MODEL_SELECT_CHECK 1
#include "../../../Lovyan_config.h"

#if defined(SSD1322)

#include <SPIFFS.h>
#include <stdarg.h>
#include <string.h>

#include "../../core/text_utils.h"

namespace {

static uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

struct VlwGlyph {
  uint16_t unicode;
  uint8_t height;
  uint8_t width;
  int8_t xAdvance;
  int8_t dY;
  int8_t dX;
  uint32_t bitmapOffset;
};

struct VlwFont {
  String path;
  int glyphCount = 0;
  int fontSize = 0;
  int ascent = 0;
  int descent = 0;
  int maxAscent = 0;
  int maxDescent = 0;
  int yAdvance = 0;
  int spaceAdvance = 3;
  VlwGlyph* glyphs = nullptr;
  uint8_t* bitmap = nullptr;
};

struct TextMetrics {
  int width = 0;
  int top = 0;
  int bottom = 0;
  int height = 0;
};

static VlwFont* g_fontCache[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

static String normalizePath(const char* name) {
  if (!name || !*name) return String();
  String p(name);
  if (p.startsWith("/spiffs/")) p.remove(0, 7);
  else if (p == "/spiffs") p = "/";
  if (!p.startsWith("/")) p = String("/") + p;
  return p;
}



static uint16_t nextCodepoint(const char*& s) {
  if (!s || !*s) return 0;
  const uint8_t c0 = (uint8_t)*s++;
  if (c0 < 0x80) return c0;
  if ((c0 & 0xE0) == 0xC0) {
    const uint8_t c1 = (uint8_t)*s;
    if ((c1 & 0xC0) == 0x80) {
      ++s;
      return (uint16_t)(((c0 & 0x1F) << 6) | (c1 & 0x3F));
    }
    return '?';
  }
  if ((c0 & 0xF0) == 0xE0) {
    const uint8_t c1 = (uint8_t)s[0];
    const uint8_t c2 = (uint8_t)s[1];
    if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80)) {
      s += 2;
      const uint16_t cp = (uint16_t)(((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F));
      return cp <= 0xFFFF ? cp : (uint16_t)'?';
    }
    return '?';
  }
  if ((c0 & 0xF8) == 0xF0) {
    const uint8_t c1 = (uint8_t)s[0];
    const uint8_t c2 = (uint8_t)s[1];
    const uint8_t c3 = (uint8_t)s[2];
    if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
      s += 3;
    }
    return '?';
  }
  return '?';
}

static VlwGlyph* findGlyph(VlwFont* f, uint16_t code) {
  if (!f || !f->glyphs) return nullptr;
  for (int i = 0; i < f->glyphCount; ++i) {
    if (f->glyphs[i].unicode == code) return &f->glyphs[i];
  }
  if (code != '?') {
    for (int i = 0; i < f->glyphCount; ++i) {
      if (f->glyphs[i].unicode == '?') return &f->glyphs[i];
    }
  }
  return nullptr;
}

static VlwFont* parseVlwFont(const String& path) {
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return nullptr;
  const size_t fileSize = f.size();
  if (fileSize < 24) {
    f.close();
    return nullptr;
  }

  uint8_t header[24];
  if (f.read(header, sizeof(header)) != (int)sizeof(header)) {
    f.close();
    return nullptr;
  }

  VlwFont* vf = new VlwFont();
  vf->path = path;
  vf->glyphCount = (int)be32(header + 0);
  vf->fontSize = (int)be32(header + 8);
  vf->ascent = (int)((int32_t)be32(header + 16));
  vf->descent = (int)((int32_t)be32(header + 20));
  if (vf->glyphCount <= 0 || vf->glyphCount > 2048) {
    delete vf;
    f.close();
    return nullptr;
  }

  vf->glyphs = new VlwGlyph[vf->glyphCount];
  const size_t tableBytes = (size_t)vf->glyphCount * 28;
  uint8_t* table = new uint8_t[tableBytes];
  if (f.read(table, tableBytes) != (int)tableBytes) {
    delete[] table;
    delete[] vf->glyphs;
    delete vf;
    f.close();
    return nullptr;
  }

  uint32_t bitmapOffset = 0;
  for (int i = 0; i < vf->glyphCount; ++i) {
    const uint8_t* rec = table + (size_t)i * 28;
    VlwGlyph& g = vf->glyphs[i];
    g.unicode = (uint16_t)be32(rec + 0);
    g.height = (uint8_t)be32(rec + 4);
    g.width = (uint8_t)be32(rec + 8);
    g.xAdvance = (int8_t)((int32_t)be32(rec + 12));
    g.dY = (int8_t)((int32_t)be32(rec + 16));
    g.dX = (int8_t)((int32_t)be32(rec + 20));
    g.bitmapOffset = bitmapOffset;
    bitmapOffset += (uint32_t)g.width * (uint32_t)g.height;
    if (g.dY > vf->maxAscent) vf->maxAscent = g.dY;
    const int desc = (int)g.height - (int)g.dY;
    if (desc > vf->maxDescent) vf->maxDescent = desc;
  }
  delete[] table;

  const size_t expectedBitmapBytes = bitmapOffset;
  if (24 + tableBytes + expectedBitmapBytes > fileSize) {
    delete[] vf->glyphs;
    delete vf;
    f.close();
    return nullptr;
  }

  vf->bitmap = new uint8_t[expectedBitmapBytes];
  if (expectedBitmapBytes && f.read(vf->bitmap, expectedBitmapBytes) != (int)expectedBitmapBytes) {
    delete[] vf->bitmap;
    delete[] vf->glyphs;
    delete vf;
    f.close();
    return nullptr;
  }
  f.close();

  if (vf->maxAscent <= 0) vf->maxAscent = vf->ascent > 0 ? vf->ascent : vf->fontSize;
  if (vf->maxDescent < 0) vf->maxDescent = 0;
  vf->yAdvance = vf->maxAscent + vf->maxDescent;
  if (vf->yAdvance <= 0) vf->yAdvance = vf->fontSize > 0 ? vf->fontSize : 10;
  VlwGlyph* gSpace = findGlyph(vf, ' ');
  if (gSpace) vf->spaceAdvance = gSpace->xAdvance;
  else {
    VlwGlyph* gi = findGlyph(vf, 'i');
    if (gi) vf->spaceAdvance = gi->xAdvance;
    else vf->spaceAdvance = vf->fontSize > 7 ? 3 : 2;
  }
  if (vf->spaceAdvance <= 0) vf->spaceAdvance = 3;
  return vf;
}

static VlwFont* acquireFont(const char* name) {
  const String path = normalizePath(name);
  if (!path.length()) return nullptr;
  for (VlwFont* f : g_fontCache) {
    if (f && f->path == path) return f;
  }
  VlwFont* parsed = parseVlwFont(path);
  if (!parsed) return nullptr;
  for (int i = 0; i < 6; ++i) {
    if (!g_fontCache[i]) {
      g_fontCache[i] = parsed;
      return parsed;
    }
  }
  return parsed;
}

static TextMetrics measureText(VlwFont* font, const char* s) {
  TextMetrics m;
  if (!font || !s) {
    return m;
  }
  const String fixed = text_fix(s);
  const char* p = fixed.c_str();
  int width = 0;
  int top = 0;
  int bottom = 0;
  bool seen = false;
  while (p && *p) {
    if (*p == '\n') break;
    const uint16_t cp = nextCodepoint(p);
    if (!cp) break;
    if (cp == ' ') {
      width += font->spaceAdvance;
      seen = true;
      continue;
    }
    VlwGlyph* g = findGlyph(font, cp);
    if (!g) {
      width += font->spaceAdvance;
      seen = true;
      continue;
    }
    if (g->dY > top) top = g->dY;
    const int desc = (int)g->height - (int)g->dY;
    if (desc > bottom) bottom = desc;
    width += g->xAdvance > 0 ? g->xAdvance : g->width;
    seen = true;
  }
  m.width = width;
  m.top = seen ? top : font->maxAscent;
  m.bottom = seen ? bottom : font->maxDescent;
  m.height = m.top + m.bottom;
  if (m.height <= 0) m.height = font->yAdvance;
  return m;
}

static uint8_t blend4(uint8_t bg, uint8_t fg, uint8_t a) {
  return (uint8_t)((bg * (255 - a) + fg * a + 127) / 255);
}

static uint8_t sharpenAlpha(uint8_t a) {
  if (a < 10) return 0;
  if (a < 48) return (uint8_t)(a / 2);
  if (a < 160) {
    return (uint8_t)(72 + ((uint16_t)(a - 48) * 128) / 112);  // 72..200
  }
  return (uint8_t)(200 + ((uint16_t)(a - 160) * 55) / 95);    // 200..255
}

static void drawGlyphToDevice(oledgfx::LGFX_Device& dev, VlwFont* font, const VlwGlyph& g, int x, int y, uint16_t textColor, uint16_t textBgColor) {
  const uint8_t fg = oledgfx::ssd1322_gray4_from_rgb565(textColor);
  const uint8_t bg = oledgfx::ssd1322_gray4_from_rgb565(textBgColor);
  const uint8_t* bmp = font->bitmap + g.bitmapOffset;
  for (int yy = 0; yy < g.height; ++yy) {
    for (int xx = 0; xx < g.width; ++xx) {
      const uint8_t a = bmp[(size_t)yy * g.width + xx];
      const uint8_t aa = sharpenAlpha(a);
      if (!aa) continue;
      dev.Jamis_SSD1322::drawPixel(x + xx, y + yy, blend4(bg, fg, aa));
    }
  }
}

static uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a) {
  const uint8_t br = (bg >> 11) & 0x1F;
  const uint8_t bg6 = (bg >> 5) & 0x3F;
  const uint8_t bb = bg & 0x1F;
  const uint8_t fr = (fg >> 11) & 0x1F;
  const uint8_t fg6 = (fg >> 5) & 0x3F;
  const uint8_t fb = fg & 0x1F;
  const uint8_t rr = (uint8_t)((br * (255 - a) + fr * a + 127) / 255);
  const uint8_t rg = (uint8_t)((bg6 * (255 - a) + fg6 * a + 127) / 255);
  const uint8_t rb = (uint8_t)((bb * (255 - a) + fb * a + 127) / 255);
  return (uint16_t)((rr << 11) | (rg << 5) | rb);
}

static void drawGlyphToSprite(oledgfx::LGFX_Sprite& spr, VlwFont* font, const VlwGlyph& g, int x, int y, uint16_t textColor, uint16_t textBgColor) {
  uint16_t* buf = spr.getBuffer();
  if (!buf) return;
  const uint16_t fg = textColor;
  const uint16_t bg = textBgColor;
  const uint8_t* bmp = font->bitmap + g.bitmapOffset;
  for (int yy = 0; yy < g.height; ++yy) {
    const int py = y + yy;
    if (py < 0 || py >= spr.height()) continue;
    for (int xx = 0; xx < g.width; ++xx) {
      const int px = x + xx;
      if (px < 0 || px >= spr.width()) continue;
      const uint8_t a = bmp[(size_t)yy * g.width + xx];
      const uint8_t aa = sharpenAlpha(a);
      if (!aa) continue;
      buf[(size_t)py * spr.width() + px] = blend565(bg, fg, aa);
    }
  }
}

static uint8_t ssd1322_gray4_from_rgb565(uint16_t rgb, int x, int y) {
  static const uint8_t bayer4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
  };
  uint8_t g = oledgfx::ssd1322_gray4_from_rgb565(rgb);
  if (g >= 15 || rgb == 0) return g;
  const uint8_t thr = bayer4[y & 3][x & 3];
  const uint8_t r = (rgb >> 11) & 0x1F;
  const uint8_t gg = (rgb >> 5) & 0x3F;
  const uint8_t b = rgb & 0x1F;
  const int lum8 = ((r * 255 / 31) * 77 + (gg * 255 / 63) * 150 + (b * 255 / 31) * 29) >> 8;
  const int rem = lum8 - (g * 17);
  if (rem > thr && g < 15) ++g;
  return g;
}

static int boundsHeight(Adafruit_GFX& d) {
  int16_t x1, y1;
  uint16_t w, h;
  d.getTextBounds("Ag", 0, 0, &x1, &y1, &w, &h);
  return h == 0 ? 8 : (int)h;
}

static int boundsWidth(Adafruit_GFX& d, const char* s) {
  int16_t x1, y1;
  uint16_t w, h;
  d.getTextBounds(s ? s : "", 0, 0, &x1, &y1, &w, &h);
  return (int)w;
}

}  // namespace

namespace oledgfx {

LGFX_Device::LGFX_Device()
    : Jamis_SSD1322(TFT_WIDTH, TFT_HEIGHT, &SPI, TFT_DC, TFT_RST, TFT_CS, TFT_SPI_FREQ_WRITE) {}

void LGFX_Device::init() {
  begin(true, true);
  delay(120);
  setRotation(TFT_ROTATION);
  cp437(true);
  setTextWrap(false);
  setTextColor(TFT_WHITE, TFT_BLACK);
  clearDisplay();
  display();
}

void LGFX_Device::loadFont(const char* name) {
  _vlw = acquireFont(name);
  if (!_vlw) Adafruit_GFX::setFont(nullptr);
}

void LGFX_Device::unloadFont() {
  _vlw = nullptr;
  Adafruit_GFX::setFont(nullptr);
}

void LGFX_Device::setFont(const GFXfont* f) {
  _vlw = nullptr;
  Adafruit_GFX::setFont(f);
}

void LGFX_Device::drawPixel(int16_t x, int16_t y, uint16_t color) {
  Jamis_SSD1322::drawPixel(x, y, ssd1322_gray4_from_rgb565(color));
  if (_autoFlush) display();
}

void LGFX_Device::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  Jamis_SSD1322::drawFastHLine(x, y, w, ssd1322_gray4_from_rgb565(color));
  if (_autoFlush) display();
}

void LGFX_Device::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  Jamis_SSD1322::drawFastVLine(x, y, h, ssd1322_gray4_from_rgb565(color));
  if (_autoFlush) display();
}

void LGFX_Device::fillScreen(uint16_t color) {
  _autoFlush = false;
  Adafruit_GFX::fillScreen(ssd1322_gray4_from_rgb565(color));
  _autoFlush = true;
  display();
}

void LGFX_Device::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  _autoFlush = false;
  Adafruit_GFX::fillRect(x, y, w, h, ssd1322_gray4_from_rgb565(color));
  _autoFlush = true;
  display();
}

void LGFX_Device::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  _autoFlush = false;
  Adafruit_GFX::drawRoundRect(x, y, w, h, r, ssd1322_gray4_from_rgb565(color));
  _autoFlush = true;
  display();
}

void LGFX_Device::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) {
  _autoFlush = false;
  Adafruit_GFX::fillRoundRect(x, y, w, h, r, ssd1322_gray4_from_rgb565(color));
  _autoFlush = true;
  display();
}

int LGFX_Device::textWidth(const char* s) {
  if (_vlw) return measureText((VlwFont*)_vlw, s).width;
  return boundsWidth(*this, s);
}

int LGFX_Device::fontHeight() {
  if (_vlw) return ((VlwFont*)_vlw)->yAdvance;
  return boundsHeight(*this);
}

void LGFX_Device::drawString(const String& s, int32_t x, int32_t y) {
  const String fixed = text_fix(s.c_str());
  if (!_vlw) {
    const int w = textWidth(fixed.c_str());
    const int h = fontHeight();
    if (_datum == middle_center) {
      x -= w / 2;
      y += h / 2 - 2;
    } else if (_datum == top_right) {
      x -= w;
    }
    setCursor(x, y + h - 2);
    _autoFlush = false;
    Adafruit_GFX::print(fixed);
    _autoFlush = true;
    display();
    return;
  }

  VlwFont* font = (VlwFont*)_vlw;
  TextMetrics m = measureText(font, fixed.c_str());
  if (_datum == middle_center) {
    x -= m.width / 2;
    y -= m.height / 2;
  } else if (_datum == top_right) {
    x -= m.width;
  }
  const int baseline = y + m.top;
  int penX = x;
  _autoFlush = false;
  const char* p = fixed.c_str();
  while (p && *p) {
    if (*p == '\n') break;
    const uint16_t cp = nextCodepoint(p);
    if (!cp) break;
    if (cp == ' ') {
      penX += font->spaceAdvance;
      continue;
    }
    VlwGlyph* g = findGlyph(font, cp);
    if (!g) {
      penX += font->spaceAdvance;
      continue;
    }
    drawGlyphToDevice(*this, font, *g, penX + g->dX, baseline - g->dY, textcolor, textbgcolor);
    penX += g->xAdvance > 0 ? g->xAdvance : g->width;
  }
  _autoFlush = true;
  display();
}

void LGFX_Device::pushImage(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
  if (!data) return;
  for (int yy = 0; yy < h; ++yy) {
    for (int xx = 0; xx < w; ++xx) {
      Jamis_SSD1322::drawPixel(x + xx, y + yy, ssd1322_gray4_from_rgb565(data[yy * w + xx]));
    }
  }
  display();
}

size_t LGFX_Device::print(const String& s) {
  if (_vlw) {
    drawString(s, cursor_x, cursor_y);
    cursor_x += textWidth(s.c_str());
    return s.length();
  }
  const String fixed = text_fix(s.c_str());
  _autoFlush = false;
  const size_t n = Adafruit_GFX::print(fixed);
  _autoFlush = true;
  display();
  return n;
}
size_t LGFX_Device::print(const char* s) {
  if (_vlw) {
    drawString(s ? String(s) : String(), cursor_x, cursor_y);
    cursor_x += textWidth(s ? s : "");
    return s ? strlen(s) : 0;
  }
  const String fixed = text_fix(s ? s : "");
  _autoFlush = false;
  const size_t n = Adafruit_GFX::print(fixed);
  _autoFlush = true;
  display();
  return n;
}
size_t LGFX_Device::print(int n) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%d", n);
  return print(buf);
}
size_t LGFX_Device::println(const String& s) {
  const size_t n = print(s);
  cursor_x = 0;
  cursor_y += fontHeight();
  return n;
}
size_t LGFX_Device::println(const char* s) {
  const size_t n = print(s);
  cursor_x = 0;
  cursor_y += fontHeight();
  return n;
}
size_t LGFX_Device::println() {
  cursor_x = 0;
  cursor_y += fontHeight();
  return 1;
}

size_t LGFX_Device::printf(const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n < 0) return 0;
  return print(buf);
}

bool LGFX_Sprite::createSprite(int16_t w, int16_t h) {
  deleteSprite();
  WIDTH = w;
  HEIGHT = h;
  _width = w;
  _height = h;
  buffer = (uint16_t*)malloc((size_t)w * h * sizeof(uint16_t));
  if (!buffer) {
    WIDTH = HEIGHT = _width = _height = 0;
    return false;
  }
  fillScreen(TFT_BLACK);
  setTextWrap(false);
  return true;
}

void LGFX_Sprite::deleteSprite() {
  if (buffer) free(buffer);
  buffer = nullptr;
  WIDTH = HEIGHT = _width = _height = 0;
}

void LGFX_Sprite::loadFont(const char* name) {
  _vlw = acquireFont(name);
  if (!_vlw) Adafruit_GFX::setFont(nullptr);
}

void LGFX_Sprite::unloadFont() {
  _vlw = nullptr;
  Adafruit_GFX::setFont(nullptr);
}

void LGFX_Sprite::setFont(const GFXfont* f) {
  _vlw = nullptr;
  Adafruit_GFX::setFont(f);
}

int LGFX_Sprite::textWidth(const char* s) {
  if (_vlw) return measureText((VlwFont*)_vlw, s).width;
  return boundsWidth(*this, s);
}

int LGFX_Sprite::fontHeight() {
  if (_vlw) return ((VlwFont*)_vlw)->yAdvance;
  return boundsHeight(*this);
}

void LGFX_Sprite::drawString(const String& s, int32_t x, int32_t y) {
  const String fixed = text_fix(s.c_str());
  if (!_vlw) {
    const int w = textWidth(fixed.c_str());
    const int h = fontHeight();
    if (_datum == middle_center) {
      x -= w / 2;
      y += h / 2 - 2;
    } else if (_datum == top_right) {
      x -= w;
    }
    setCursor(x, y + h - 2);
    Adafruit_GFX::print(fixed);
    return;
  }

  VlwFont* font = (VlwFont*)_vlw;
  TextMetrics m = measureText(font, fixed.c_str());
  if (_datum == middle_center) {
    x -= m.width / 2;
    y -= m.height / 2;
  } else if (_datum == top_right) {
    x -= m.width;
  }
  const int baseline = y + m.top;
  int penX = x;
  const char* p = fixed.c_str();
  while (p && *p) {
    if (*p == '\n') break;
    const uint16_t cp = nextCodepoint(p);
    if (!cp) break;
    if (cp == ' ') {
      penX += font->spaceAdvance;
      continue;
    }
    VlwGlyph* g = findGlyph(font, cp);
    if (!g) {
      penX += font->spaceAdvance;
      continue;
    }
    drawGlyphToSprite(*this, font, *g, penX + g->dX, baseline - g->dY, textcolor, textbgcolor);
    penX += g->xAdvance > 0 ? g->xAdvance : g->width;
  }
}

void LGFX_Sprite::pushSprite(int32_t x, int32_t y) {
  if (!_parent || !buffer) return;
  uint16_t* p = getBuffer();
  for (int yy = 0; yy < height(); ++yy) {
    for (int xx = 0; xx < width(); ++xx) {
      _parent->Jamis_SSD1322::drawPixel(
        x + xx,
        y + yy,
        oledgfx::ssd1322_gray4_from_rgb565(p[yy * width() + xx])
      );
    }
  }
  _parent->display();
}

}  // namespace oledgfx

#endif  // defined(SSD1322)
