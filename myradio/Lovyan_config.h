#pragma once
#include <LovyanGFX.hpp>

//====================================================
// myRadio - EGYETLEN központi hardver/config fájl
//====================================================
//
// FONTOS SZABÁLY:
//   Minden fordítás előtti hardveres konfiguráció itt legyen állítható.
//   A kijelző, SPI, háttérvilágítás, encoder és DAC / I2S pin-ek
//   ne külön fájlokban legyenek szétszórva.
//
// Strukturális központosítás történt, működési logika változtatása nélkül.


//====================================================
// KÉPERNYŐ VÁLASZTÁS (pontosan egyet határozzon meg)
//====================================================

#define ILI9488
//#define ST7789
//#define ILI9341
//#define ST7796

#if (defined(ST7789) + defined(ILI9341) + defined(ST7796) + defined(ILI9488)) != 1
  #error "Pontosan EGYET határozzon meg a következők közül: ST7789 / ILI9341 / ST7796 / ILI9488"
#endif


//====================================================
// VERZIÓ (web footer és egyéb buildhez kötött kiírások)
//====================================================

#ifndef MYRADIO_VERSION
  #define MYRADIO_VERSION "0.2"
#endif

//====================================================
// NYELV VÁLASZTÁS (fordításkor, pontosan egy támogatott érték)
//====================================================

#define MYRADIO_LANG_HU 1
#define MYRADIO_LANG_EN 2
#define MYRADIO_LANG_DE 3
#define MYRADIO_LANG_PL 4

#ifndef MYRADIO_LANG
  #define MYRADIO_LANG MYRADIO_LANG_HU  // language setting
#endif

#if (MYRADIO_LANG != MYRADIO_LANG_HU) && (MYRADIO_LANG != MYRADIO_LANG_EN) && (MYRADIO_LANG != MYRADIO_LANG_DE) && (MYRADIO_LANG != MYRADIO_LANG_PL)
  #error "MYRADIO_LANG csak MYRADIO_LANG_HU, MYRADIO_LANG_EN, MYRADIO_LANG_DE vagy MYRADIO_LANG_PL lehet"
#endif

// ------------------ Display / SPI pin-ek ------------------
#ifndef TFT_DC
  #define TFT_DC 9
#endif
#ifndef TFT_CS
  #define TFT_CS 10
#endif
#ifndef TFT_RST
  #define TFT_RST -1
#endif
#ifndef TFT_BL
  #define TFT_BL 7
#endif
#ifndef BRIGHTNESS_PIN
  #define BRIGHTNESS_PIN TFT_BL
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

// régi kompatibilitási aliasok
#ifndef TFT_MOSI
  #define TFT_MOSI PIN_MOSI
#endif
#ifndef TFT_SCLK
  #define TFT_SCLK PIN_SCLK
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

// LovyanGFX backlight config kompatibilitás
#ifndef TFT_BL_PWM_CH
  #define TFT_BL_PWM_CH TFT_BL
#endif
#ifndef TFT_BL_PWM_FREQ
  #define TFT_BL_PWM_FREQ 44100
#endif
#ifndef TFT_BL_INVERT
  #define TFT_BL_INVERT false
#endif

// ------------------ Audio / DAC / I2S pin-ek ------------------
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

// ------------------ Encoder pin-ek ------------------
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

//====================================================
// Közös vezetékek / pin-ek
//====================================================
// Itt vannak a tényleges definíciók, ez az egyetlen forrás.

//====================================================
// SPI host / speeds
//====================================================
// If SPI2_HOST doesn't work on your board/core, try SPI3_HOST.
#ifndef TFT_SPI_HOST
  #define TFT_SPI_HOST SPI2_HOST
#endif

// Safe default; if stable you can try 60/80 MHz.
#ifndef TFT_SPI_FREQ_WRITE
  #define TFT_SPI_FREQ_WRITE 40000000
#endif

#ifndef TFT_SPI_FREQ_READ
  #define TFT_SPI_FREQ_READ  16000000
#endif

//====================================================
// Háttérvilágítás PWM
//====================================================
// Szintén ebben az egyetlen konfigurációs fájlban marad.

//====================================================
// PANEL ALAPÉRTELMEZÉSEK (kijelzőnként beállítva)
//====================================================

// --- ST7789 (a paneled: 320x240, és a tft.setRotation(1) parancsot használtad) ---
#if defined(ST7789)
  #define TFT_WIDTH     240
  #define TFT_HEIGHT    320
  #define TFT_OFFSET_X  0
  #define TFT_OFFSET_Y  0
  #define TFT_INVERT    false
  #define TFT_RGB_ORDER false
  #define TFT_ROTATION  1
#endif

// --- ILI9341 (tipikusan 240x320) ---
#if defined(ILI9341)
  #define TFT_WIDTH     240
  #define TFT_HEIGHT    320
  #define TFT_OFFSET_X  0
  #define TFT_OFFSET_Y  0
  #define TFT_INVERT    false
  #define TFT_RGB_ORDER false
  #define TFT_ROTATION  0
#endif

// --- ST7796 (tipikusan 320x480) ---
#if defined(ST7796)
  #define TFT_WIDTH     320
  #define TFT_HEIGHT    480
  #define TFT_OFFSET_X  0
  #define TFT_OFFSET_Y  0
  #define TFT_INVERT    false
  #define TFT_RGB_ORDER false
  #define TFT_ROTATION  0
#endif

// --- ILI9488 (az Ön által megadott ILI9488 kijelzőfájlok alapján; ott 1-es forgatás használatos) ---
// A legtöbb ILI9488 SPI modul 480x320-as; a LovyanGFX (320x480)-at használ forgatással,
// hogy 480x320-as tájképet kapjon.
// Az eredeti kód a setRotation(1) (vagy 3, ha flipscreen) parancsot használja,
// így itt az alapértelmezett érték 1.
#if defined(ILI9488)
  #define TFT_WIDTH     320
  #define TFT_HEIGHT    480
  #define TFT_OFFSET_X  0
  #define TFT_OFFSET_Y  0
  #define TFT_INVERT    true      // ha a színek fordítottnak tűnnek, legyen true
  #define TFT_RGB_ORDER false     // ha a piros/kék felcserélődött, legyen true
  #define TFT_ROTATION  1
#endif

//================================================================
// LGFX eszköz (megosztott busz, #define által kiválasztott panel)
//================================================================
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
    // ---------- BUS ----------
    {
      auto cfg = _bus.config();

      cfg.spi_host   = TFT_SPI_HOST;
      cfg.spi_mode   = 0;

      cfg.freq_write = TFT_SPI_FREQ_WRITE;
      cfg.freq_read  = TFT_SPI_FREQ_READ;

      cfg.spi_3wire  = true;   // no MISO (és a jegyzetedben az áll, hogy ne csatlakoztasd a MISO-t.)
      cfg.use_lock   = true;

      cfg.pin_sclk = PIN_SCLK;
      cfg.pin_mosi = PIN_MOSI;
      cfg.pin_miso = PIN_MISO;
      cfg.pin_dc   = TFT_DC;

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    // ---------- PANEL ----------
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

      // 3-wire SPI, no reads
      cfg.readable   = false;
      cfg.bus_shared = false;

      _panel.config(cfg);
    }

    // ---------- BACKLIGHT ----------
    {
      auto cfg = _light.config();
      cfg.pin_bl      = BRIGHTNESS_PIN;
      cfg.invert      = TFT_BL_INVERT;
      cfg.freq        = TFT_BL_PWM_FREQ;
      cfg.pwm_channel = TFT_BL_PWM_CH;

      _light.config(cfg);
      _panel.setLight(&_light);
    }

    setPanel(&_panel);

    // Alkalmazzuk a konfigurációból származó forgatást
    // (így az .ino fájlnak nincs szüksége a tft.setRotation() függvényre).)
    setRotation(TFT_ROTATION);
  }
};