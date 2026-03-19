#pragma once

// ====================================================
// myRadio - külön touch konfiguráció
// ====================================================
//
// Ez a fájl azért van, hogy a Lovyan_config.h ne nőjön túl nagyra.
// Itt vannak a touch vezérlőhöz tartozó pin-ek, kalibrációk és
// időzítések. A fő konfigurációból csak a TOUCH_MODEL kiválasztása
// történik, a részletek pedig innen jönnek.

#if (TOUCH_MODEL == TOUCH_NONE)

  #ifndef TOUCH_CS
    #define TOUCH_CS -1
  #endif
  #ifndef TOUCH_IRQ
    #define TOUCH_IRQ -1
  #endif
  #ifndef TOUCH_SCLK
    #define TOUCH_SCLK -1
  #endif
  #ifndef TOUCH_MISO
    #define TOUCH_MISO -1
  #endif
  #ifndef TOUCH_MOSI
    #define TOUCH_MOSI -1
  #endif
  #ifndef TOUCH_SPI_HOST
    #define TOUCH_SPI_HOST SPI1_HOST
  #endif

#elif (TOUCH_MODEL == TOUCH_XPT2046)

  // -------------------------
  // Board defaultok
  // -------------------------
  #if (MYRADIO_BOARD == MYRADIO_BOARD_CYD_28_ST7789_XPT2046)
    #ifndef TOUCH_CS
      #define TOUCH_CS 33
    #endif
    #ifndef TOUCH_IRQ
      #define TOUCH_IRQ -1
    #endif
    #ifndef TOUCH_SCLK
      #define TOUCH_SCLK 25
    #endif
    #ifndef TOUCH_MISO
      #define TOUCH_MISO 39
    #endif
    #ifndef TOUCH_MOSI
      #define TOUCH_MOSI 32
    #endif
    #ifndef TOUCH_SPI_HOST
      #define TOUCH_SPI_HOST VSPI
    #endif
  #else
    // Custom board fallback: itt direkt nincs erőltetett pin-kiosztás.
    #ifndef TOUCH_CS
      #define TOUCH_CS -1
    #endif
    #ifndef TOUCH_IRQ
      #define TOUCH_IRQ -1
    #endif
    #ifndef TOUCH_SCLK
      #define TOUCH_SCLK -1
    #endif
    #ifndef TOUCH_MISO
      #define TOUCH_MISO -1
    #endif
    #ifndef TOUCH_MOSI
      #define TOUCH_MOSI -1
    #endif
    #ifndef TOUCH_SPI_HOST
      #define TOUCH_SPI_HOST VSPI
    #endif
  #endif

  // -------------------------
  // Működési alapértékek
  // -------------------------
  #ifndef TOUCH_Z_THRESHOLD
    #define TOUCH_Z_THRESHOLD 80
  #endif
  #ifndef TOUCH_LONG_PRESS_MS
    #define TOUCH_LONG_PRESS_MS 650
  #endif
  #ifndef TOUCH_TAP_MOVE_TOLERANCE
    #define TOUCH_TAP_MOVE_TOLERANCE 18
  #endif
  #ifndef TOUCH_DEBOUNCE_MS
    #define TOUCH_DEBOUNCE_MS 120
  #endif

  // Ezek a CYD 2.8 + XPT2046 tipikus tartományai, de valós eszközön
  // finomhangolni kellhetnek.
  #ifndef TOUCH_CAL_X_MIN
    #define TOUCH_CAL_X_MIN 240
  #endif
  #ifndef TOUCH_CAL_X_MAX
    #define TOUCH_CAL_X_MAX 3800
  #endif
  #ifndef TOUCH_CAL_Y_MIN
    #define TOUCH_CAL_Y_MIN 240
  #endif
  #ifndef TOUCH_CAL_Y_MAX
    #define TOUCH_CAL_Y_MAX 3800
  #endif
  #ifndef TOUCH_SWAP_XY
    #define TOUCH_SWAP_XY false
  #endif
  #ifndef TOUCH_INVERT_X
    #define TOUCH_INVERT_X true
  #endif
  #ifndef TOUCH_INVERT_Y
    #define TOUCH_INVERT_Y false
  #endif

#else
  #error "Ismeretlen TOUCH_MODEL érték. Használj TOUCH_NONE vagy TOUCH_XPT2046 értéket."
#endif
