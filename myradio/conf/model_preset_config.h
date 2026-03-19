#pragma once

// ====================================================
// Belső modell/preset beállítások
// ====================================================
// Ezt nem kell piszkálni a legtöbb felhasználónak.

// A touchos ST7789 opció ugyanazt a panelt használja, csak CYD + XPT2046
// alapértékekkel.
#if defined(ST7789_XPT2046)
  #ifndef ST7789
    #define ST7789
  #endif
#endif

// ------------------ Display / SPI pin-ek ------------------
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

  // ------------------ Audio / DAC / I2S pin-ek ------------------
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

  // ------------------ Encoder pin-ek ------------------
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

// ------------------ SPI host / speeds ------------------
#ifndef TFT_SPI_HOST
  #define TFT_SPI_HOST SPI2_HOST
#endif
#ifndef TFT_SPI_FREQ_WRITE
  #define TFT_SPI_FREQ_WRITE 40000000
#endif
#ifndef TFT_SPI_FREQ_READ
  #define TFT_SPI_FREQ_READ 16000000
#endif
