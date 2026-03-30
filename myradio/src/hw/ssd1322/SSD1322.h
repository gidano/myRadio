#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>

#define OLED_BLACK 0x0
#define OLED_WHITE 0xF
#define OLED_GRAY_D 0xD
#define OLED_GRAY_B 0xB
#define OLED_GRAY_9 0x9
#define OLED_GRAY_7 0x7
#define OLED_GRAY_5 0x5
#define OLED_GRAY_3 0x3
#define OLED_GRAY_2 0x2
#define OLED_GRAY_1 0x1

#define SSD1322_DISPLAYOFF 0xAE
#define SSD1322_DISPLAYON  0xAF

class Jamis_SSD1322 : public Adafruit_GFX {
 public:
  Jamis_SSD1322(int16_t w, int16_t h, SPIClass *spi, int8_t dc_pin,
                int8_t rst_pin, int8_t cs_pin, uint32_t bitrate = 16000000UL);
  ~Jamis_SSD1322() override;

  bool begin(bool reset = true, bool periphBegin = true);
  void display();
  void clearDisplay();
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void ssd1322_command(uint8_t c);
  void invertDisplay(bool flag);
  void setContrast(uint8_t contrast);
  void setDisplayPower(bool on);
  uint8_t* getBuffer() { return buffer; }

 private:
  inline void SPIwrite(uint8_t d) __attribute__((always_inline));
  void ssd1322_command1(uint8_t c);
  void ssd1322_data1(uint8_t c);
  void drawFastHLineInternal(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawFastVLineInternal(int16_t x, int16_t y, int16_t h, uint16_t color);

  SPIClass* spi;
  uint8_t* buffer;
  int8_t dcPin, csPin, rstPin;
  SPISettings spiSettings;
  uint8_t contrastValue = 0x7F;
  bool displayPowered = true;
};
