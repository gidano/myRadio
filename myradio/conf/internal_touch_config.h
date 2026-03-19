#pragma once

// ====================================================
// Belső touch-konfiguráció
// ====================================================
// A fő Lovyan_config.h-ban csak a könnyen áttekinthető touch sorok maradnak:
//   TS_MODEL
//   TS_SPIPINS
//   TS_CS
// A többi belső átalakítás és alapérték ide került.

#define TOUCH_NONE     0
#define TOUCH_XPT2046  1

#ifndef TS_MODEL
  #define TS_MODEL TS_MODEL_NONE
#endif

#ifndef TS_CS
  #define TS_CS -1
#endif

#ifndef TS_IRQ
  #define TS_IRQ -1
#endif

#define MYRADIO_TS_GET_1_IMPL(a,b,c) a
#define MYRADIO_TS_GET_2_IMPL(a,b,c) b
#define MYRADIO_TS_GET_3_IMPL(a,b,c) c
#define MYRADIO_TS_GET_1(...) MYRADIO_TS_GET_1_IMPL(__VA_ARGS__)
#define MYRADIO_TS_GET_2(...) MYRADIO_TS_GET_2_IMPL(__VA_ARGS__)
#define MYRADIO_TS_GET_3(...) MYRADIO_TS_GET_3_IMPL(__VA_ARGS__)

#if (TS_MODEL == TS_MODEL_XPT2046)
  #define TOUCH_MODEL TOUCH_XPT2046

  #ifndef TS_SPIPINS
    #define TS_SPIPINS -1, -1, -1
  #endif

  #ifndef TOUCH_CS
    #define TOUCH_CS TS_CS
  #endif
  #ifndef TOUCH_IRQ
    #define TOUCH_IRQ TS_IRQ
  #endif
  #ifndef TOUCH_SCLK
    #define TOUCH_SCLK MYRADIO_TS_GET_1(TS_SPIPINS)
  #endif
  #ifndef TOUCH_MISO
    #define TOUCH_MISO MYRADIO_TS_GET_2(TS_SPIPINS)
  #endif
  #ifndef TOUCH_MOSI
    #define TOUCH_MOSI MYRADIO_TS_GET_3(TS_SPIPINS)
  #endif
  #ifndef TOUCH_SPI_HOST
    #define TOUCH_SPI_HOST VSPI
  #endif
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
  #define TOUCH_MODEL TOUCH_NONE
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
#endif
