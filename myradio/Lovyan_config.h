#pragma once

//====================================================
// KIJELZŐ VÁLASZTÁS (CSAK egyet válassz)
//====================================================

//#define ILI9488
//#define ST7789_XPT2046
//#define SSD1322
#define ST7789
//#define ILI9341
//#define ST7796

#include "conf/internal_model_select.h"

#if defined(SSD1322)
  #include "src/hw/ssd1322/lgfx_compat_ssd1322.h"
#else
  #include <LovyanGFX.hpp>
#endif

//====================================================
// myRadio - központi hardver/config fájl
//====================================================

//====================================================
// VERZIÓ
//====================================================
#ifndef MYRADIO_VERSION
  #define MYRADIO_VERSION "0.2.6-station_logo"
#endif

// SSD1322 preview debug
#ifndef SSD1322_BOOT_TEST_PATTERN
  #define SSD1322_BOOT_TEST_PATTERN 1
#endif

//====================================================
// NYELV VÁLASZTÁS
//====================================================
#define MYRADIO_LANG_HU 1
#define MYRADIO_LANG_EN 2
#define MYRADIO_LANG_DE 3
#define MYRADIO_LANG_PL 4

#ifndef MYRADIO_LANG
  #define MYRADIO_LANG MYRADIO_LANG_HU
#endif

#if (MYRADIO_LANG != MYRADIO_LANG_HU) && (MYRADIO_LANG != MYRADIO_LANG_EN) && (MYRADIO_LANG != MYRADIO_LANG_DE) && (MYRADIO_LANG != MYRADIO_LANG_PL)
  #error "MYRADIO_LANG csak MYRADIO_LANG_HU, MYRADIO_LANG_EN, MYRADIO_LANG_DE vagy MYRADIO_LANG_PL lehet"
#endif

//====================================================
// DISPLAY / SPI PINEK
//====================================================
#if defined(ST7789_XPT2046)
  #ifndef TFT_DC
    #define TFT_DC 2
  #endif
  #ifndef TFT_CS
    #define TFT_CS 15
  #endif
  #ifndef TFT_RST
    #define TFT_RST -1
  #endif
  #ifndef TFT_BL
    #define TFT_BL 21
  #endif
  #ifndef BRIGHTNESS_PIN
    #define BRIGHTNESS_PIN TFT_BL
  #endif

  #ifndef PIN_MOSI
    #define PIN_MOSI 13
  #endif
  #ifndef PIN_SCLK
    #define PIN_SCLK 14
  #endif
  #ifndef PIN_MISO
    #define PIN_MISO -1
  #endif
#else
  #ifndef TFT_DC
    #define TFT_DC 9
  #endif
  #ifndef TFT_CS
    #define TFT_CS 10
  #endif
  #ifndef TFT_RST
    #define TFT_RST -1
  #endif
  #if defined(SSD1322)
  #ifndef TFT_BL
    #define TFT_BL -1
  #endif
  #ifndef BRIGHTNESS_PIN
    #define BRIGHTNESS_PIN -1
  #endif
#else
  #ifndef TFT_BL
    #define TFT_BL 7
  #endif
  #ifndef BRIGHTNESS_PIN
    #define BRIGHTNESS_PIN TFT_BL
  #endif
#endif

  #ifndef PIN_MOSI
    #define PIN_MOSI 11
  #endif
  #ifndef PIN_SCLK
    #define PIN_SCLK 12
  #endif
  #ifndef PIN_MISO
    #define PIN_MISO -1
  #endif
#endif

// régi kompatibilitási aliasok
#ifndef TFT_MOSI
  #define TFT_MOSI PIN_MOSI
#endif
#ifndef TFT_SCLK
  #define TFT_SCLK PIN_SCLK
#endif

//====================================================
// AUDIO / DAC / I2S PINEK
//====================================================
#if defined(ST7789_XPT2046)
  #ifndef I2S_DOUT
    #define I2S_DOUT 27
  #endif
  #ifndef I2S_LRC
    #define I2S_LRC 22
  #endif
  #ifndef I2S_BCLK
    #define I2S_BCLK 4
  #endif
  #ifndef I2S_MCLK
    #define I2S_MCLK -1
  #endif
#else
  #ifndef I2S_DOUT
    #define I2S_DOUT 5
  #endif
  #ifndef I2S_BCLK
    #define I2S_BCLK 4
  #endif
  #ifndef I2S_LRC
    #define I2S_LRC 6
  #endif
  #ifndef I2S_MCLK
    #define I2S_MCLK 15
  #endif
#endif

//====================================================
// ENCODER PINEK
//====================================================
#if defined(ST7789_XPT2046)
  #ifndef ENC_A
    #define ENC_A 3
  #endif
  #ifndef ENC_B
    #define ENC_B 1
  #endif
  #ifndef ENC_BTN
    #define ENC_BTN 0
  #endif
  #ifndef ENC_PULSES_PER_STEP
    #define ENC_PULSES_PER_STEP 4
  #endif
#else
  #ifndef ENC_A
    #define ENC_A 3
  #endif
  #ifndef ENC_B
    #define ENC_B 1
  #endif
  #ifndef ENC_BTN
    #define ENC_BTN 2
  #endif
  #ifndef ENC_PULSES_PER_STEP
    #define ENC_PULSES_PER_STEP 4
  #endif
#endif

//====================================================
// TOUCHSCREEN (csak a touchos ST7789-hez kell)
//====================================================
#define TS_MODEL_NONE     0
#define TS_MODEL_XPT2046  1

#if defined(ST7789_XPT2046)
  /* TOUCHSCREEN */
  #define TS_MODEL         TS_MODEL_XPT2046
  #define TS_SPIPINS       25, 39, 32    /* SCK, MISO, MOSI */
  #define TS_CS            33
#endif

//====================================================
// SPI host / speeds
//====================================================
#ifndef TFT_SPI_HOST
  #define TFT_SPI_HOST SPI2_HOST
#endif
#ifndef TFT_SPI_FREQ_WRITE
  #if defined(SSD1322)
    #define TFT_SPI_FREQ_WRITE 10000000
  #else
    #define TFT_SPI_FREQ_WRITE 40000000
  #endif
#endif
#ifndef TFT_SPI_FREQ_READ
  #define TFT_SPI_FREQ_READ 16000000
#endif

// ------------------ Háttérvilágítás PWM ------------------
#ifndef BL_PWM_FREQ
  #define BL_PWM_FREQ 5000
#endif
#ifndef BL_PWM_RES
  #define BL_PWM_RES 8
#endif
#ifndef BL_PWM_CH
  #define BL_PWM_CH 0
#endif

#ifndef TFT_BL_PWM_CH
  #define TFT_BL_PWM_CH TFT_BL
#endif
#ifndef TFT_BL_PWM_FREQ
  #define TFT_BL_PWM_FREQ 44100
#endif
#ifndef TFT_BL_INVERT
  #define TFT_BL_INVERT false
#endif

//====================================================
// PANEL ALAPÉRTELMEZÉSEK
//====================================================
#if defined(ST7789)
  #define TFT_WIDTH     240
  #define TFT_HEIGHT    320
  #define TFT_OFFSET_X  0
  #define TFT_OFFSET_Y  0
  #define TFT_INVERT    false
  #define TFT_RGB_ORDER false
  #define TFT_ROTATION  1
#endif

#if defined(ILI9341)
  #define TFT_WIDTH     240
  #define TFT_HEIGHT    320
  #define TFT_OFFSET_X  0
  #define TFT_OFFSET_Y  0
  #define TFT_INVERT    false
  #define TFT_RGB_ORDER false
  #define TFT_ROTATION  0
#endif

#if defined(ST7796)
  #define TFT_WIDTH     320
  #define TFT_HEIGHT    480
  #define TFT_OFFSET_X  0
  #define TFT_OFFSET_Y  0
  #define TFT_INVERT    false
  #define TFT_RGB_ORDER false
  #define TFT_ROTATION  1
#endif

#if defined(ILI9488)
  #define TFT_WIDTH     320
  #define TFT_HEIGHT    480
  #define TFT_OFFSET_X  0
  #define TFT_OFFSET_Y  0
  #define TFT_INVERT    true
  #define TFT_RGB_ORDER false
  #define TFT_ROTATION  1
#endif


#if defined(SSD1322)
  #define TFT_WIDTH     256
  #define TFT_HEIGHT    64
  #define TFT_OFFSET_X  0
  #define TFT_OFFSET_Y  0
  #define TFT_INVERT    false
  #define TFT_RGB_ORDER false
  #define TFT_ROTATION  0
#endif

#ifndef TOUCH_ROTATION
  #define TOUCH_ROTATION TFT_ROTATION
#endif

#include "conf/internal_touch_config.h"

//================================================================
// LGFX eszköz (megosztott busz, #define által kiválasztott panel)
//================================================================
#if defined(SSD1322)
// SSD1322-nél a LovyanGFX helyett egy kompatibilitási adaptert használunk,
// így a magasabb szintű UI kód nagy része változatlanul maradhat.
#else
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Bus_SPI   _bus;
  lgfx::Light_PWM _light;

#if defined(ST7789)
  lgfx::Panel_ST7789 _panel;
#elif defined(ILI9341)
  lgfx::Panel_ILI9341 _panel;
#elif defined(ST7796)
  lgfx::Panel_ST7796 _panel;
#elif defined(ILI9488)
  lgfx::Panel_ILI9488 _panel;
#endif

public:
  LGFX(void)
  {
    {
      auto cfg = _bus.config();
      cfg.spi_host   = TFT_SPI_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = TFT_SPI_FREQ_WRITE;
      cfg.freq_read  = TFT_SPI_FREQ_READ;
      cfg.spi_3wire  = true;
      cfg.use_lock   = true;
      cfg.pin_sclk = PIN_SCLK;
      cfg.pin_mosi = PIN_MOSI;
      cfg.pin_miso = PIN_MISO;
      cfg.pin_dc   = TFT_DC;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    {
      auto cfg = _panel.config();
      cfg.pin_cs   = TFT_CS;
      cfg.pin_rst  = TFT_RST;
      cfg.pin_busy = -1;
      cfg.panel_width  = TFT_WIDTH;
      cfg.panel_height = TFT_HEIGHT;
      cfg.offset_x = TFT_OFFSET_X;
      cfg.offset_y = TFT_OFFSET_Y;
      cfg.invert    = TFT_INVERT;
      cfg.rgb_order = TFT_RGB_ORDER;
      cfg.readable   = false;
      cfg.bus_shared = false;
      _panel.config(cfg);
    }

    if (BRIGHTNESS_PIN >= 0) {
      auto cfg = _light.config();
      cfg.pin_bl      = BRIGHTNESS_PIN;
      cfg.invert      = TFT_BL_INVERT;
      cfg.freq        = TFT_BL_PWM_FREQ;
      cfg.pwm_channel = TFT_BL_PWM_CH;
      _light.config(cfg);
      _panel.setLight(&_light);
    }

    setPanel(&_panel);
    setRotation(TFT_ROTATION);
  }
};
#endif
