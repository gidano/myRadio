#include "../../../Lovyan_config.h"
#include "SSD1322.h"

#define ssd1322_swap(a, b) (((a) ^= (b)), ((b) ^= (a)), ((a) ^= (b)))
#define SSD1322_SELECT       digitalWrite(csPin, LOW)
#define SSD1322_DESELECT     digitalWrite(csPin, HIGH)
#define SSD1322_MODE_COMMAND digitalWrite(dcPin, LOW)
#define SSD1322_MODE_DATA    digitalWrite(dcPin, HIGH)
#define SPI_TRANSACTION_START spi->beginTransaction(spiSettings)
#define SPI_TRANSACTION_END   spi->endTransaction()
#define TRANSACTION_START { SPI_TRANSACTION_START; SSD1322_SELECT; }
#define TRANSACTION_END   { SSD1322_DESELECT; SPI_TRANSACTION_END; }

Jamis_SSD1322::Jamis_SSD1322(int16_t w, int16_t h, SPIClass *spi_,
  int8_t dc_pin, int8_t rst_pin, int8_t cs_pin, uint32_t bitrate)
: Adafruit_GFX(w, h), spi(spi_ ? spi_ : &SPI), buffer(nullptr),
  dcPin(dc_pin), csPin(cs_pin), rstPin(rst_pin),
  spiSettings(bitrate, MSBFIRST, SPI_MODE3) {}

Jamis_SSD1322::~Jamis_SSD1322() {
  if (buffer) free(buffer);
}

inline void Jamis_SSD1322::SPIwrite(uint8_t d) { spi->transfer(d); }
void Jamis_SSD1322::ssd1322_command1(uint8_t c) { SSD1322_MODE_COMMAND; SPIwrite(c); }
void Jamis_SSD1322::ssd1322_data1(uint8_t c) { SSD1322_MODE_DATA; SPIwrite(c); }

void Jamis_SSD1322::ssd1322_command(uint8_t c) {
  TRANSACTION_START; ssd1322_command1(c); TRANSACTION_END;
}

bool Jamis_SSD1322::begin(bool reset, bool periphBegin) {
  if (!buffer) buffer = (uint8_t*)malloc(WIDTH * (HEIGHT / 2));
  if (!buffer) return false;
  clearDisplay();

  pinMode(dcPin, OUTPUT);
  pinMode(csPin, OUTPUT);
  SSD1322_DESELECT;
  if (periphBegin) spi->begin(PIN_SCLK, PIN_MISO, PIN_MOSI, csPin);
  delay(20);

  if (reset && rstPin >= 0) {
    pinMode(rstPin, OUTPUT);
    digitalWrite(rstPin, HIGH); delay(1);
    digitalWrite(rstPin, LOW);  delay(10);
    digitalWrite(rstPin, HIGH);
  }

  TRANSACTION_START;
  ssd1322_command1(0xFD); ssd1322_data1(0x12);
  ssd1322_command1(0xA4);
  ssd1322_command1(0xB3); ssd1322_data1(0x91);
  ssd1322_command1(0xCA); ssd1322_data1(0x3F);
  ssd1322_command1(0xA2); ssd1322_data1(0x00);
  ssd1322_command1(0xA1); ssd1322_data1(0x00);
  ssd1322_command1(0xA0); ssd1322_data1(0x14); ssd1322_data1(0x11);
  ssd1322_command1(0xB5); ssd1322_data1(0x00);
  ssd1322_command1(0xAB); ssd1322_data1(0x01);
  ssd1322_command1(0xB4); ssd1322_data1(0xA0); ssd1322_data1(0xB5);
  ssd1322_command1(0xC1); ssd1322_data1(0x7F);
  ssd1322_command1(0xC7); ssd1322_data1(0x0F);
  ssd1322_command1(0xB9);
  ssd1322_command1(0xB1); ssd1322_data1(0xE2);
  ssd1322_command1(0xD1); ssd1322_data1(0xA2); ssd1322_data1(0x20);
  ssd1322_command1(0xBB); ssd1322_data1(0x1F);
  ssd1322_command1(0xB6); ssd1322_data1(0x08);
  ssd1322_command1(0xBE); ssd1322_data1(0x07);
  ssd1322_command1(0xA6);
  ssd1322_command1(0xA9);
  ssd1322_command1(0xAF);
  TRANSACTION_END;
  delay(120);

  return true;
}

void Jamis_SSD1322::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((x < 0) || (x >= width()) || (y < 0) || (y >= height())) return;
  switch (getRotation()) {
    case 1: ssd1322_swap(x, y); x = WIDTH - x - 1; break;
    case 2: x = WIDTH - x - 1; y = HEIGHT - y - 1; break;
    case 3: ssd1322_swap(x, y); y = HEIGHT - y - 1; break;
    default: break;
  }
  const size_t idx = (x >> 1) + y * WIDTH / 2;
  buffer[idx] &= (x & 1) ? 0xF0 : 0x0F;
  buffer[idx] |= (color & 0x0F) << (((x & 1) ? 0 : 1) * 4);
}

void Jamis_SSD1322::clearDisplay() {
  if (buffer) memset(buffer, 0, WIDTH * (HEIGHT / 2));
}

void Jamis_SSD1322::drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color) {
  if ((y < 0) || (y >= HEIGHT)) return;
  if (x < 0) { w += x; x = 0; }
  if (x + w > WIDTH) w = WIDTH - x;
  if (w <= 0) return;
  const uint16_t yOffset = y * WIDTH / 2;
  while (w--) {
    const int xx = x + w;
    const size_t idx = (xx >> 1) + yOffset;
    buffer[idx] &= (xx & 1) ? 0xF0 : 0x0F;
    buffer[idx] |= (color & 0x0F) << (((xx & 1) ? 0 : 1) * 4);
  }
}

void Jamis_SSD1322::drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t color) {
  if ((x < 0) || (x >= WIDTH)) return;
  if (y < 0) { h += y; y = 0; }
  if (y + h > HEIGHT) h = HEIGHT - y;
  if (h <= 0) return;
  while (h--) drawPixel(x, y + h, color);
}

void Jamis_SSD1322::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
  bool swap = false;
  switch (rotation) {
    case 1: swap = true; ssd1322_swap(x, y); x = WIDTH - x - 1; break;
    case 2: x = WIDTH - x - 1; y = HEIGHT - y - 1; x -= (w - 1); break;
    case 3: swap = true; ssd1322_swap(x, y); y = HEIGHT - y - 1; y -= (w - 1); break;
    default: break;
  }
  if (swap) drawFastVLineInternal(x, y, w, color); else drawFastHLineInternal(x, y, w, color);
}

void Jamis_SSD1322::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
  bool swap = false;
  switch (rotation) {
    case 1: swap = true; ssd1322_swap(x, y); x = WIDTH - x - 1; x -= (h - 1); break;
    case 2: x = WIDTH - x - 1; y = HEIGHT - y - 1; y -= (h - 1); break;
    case 3: swap = true; ssd1322_swap(x, y); y = HEIGHT - y - 1; break;
    default: break;
  }
  if (swap) drawFastHLineInternal(x, y, h, color); else drawFastVLineInternal(x, y, h, color);
}

void Jamis_SSD1322::display() {
  if (!buffer) return;
  TRANSACTION_START;
  ssd1322_command1(0x15); ssd1322_data1(0x1C); ssd1322_data1(0x5B);
  ssd1322_command1(0x75); ssd1322_data1(0x00); ssd1322_data1(0x3F);
  ssd1322_command1(0x5C);
  SSD1322_MODE_DATA;
  for (int i = 0; i < WIDTH * HEIGHT / 2; ++i) SPIwrite(buffer[i]);
  TRANSACTION_END;
}

void Jamis_SSD1322::invertDisplay(bool flag) {
  TRANSACTION_START;
  ssd1322_command1(flag ? 0xA7 : 0xA6);
  TRANSACTION_END;
}
