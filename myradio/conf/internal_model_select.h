#pragma once

// A touchos ST7789 opció ugyanazt a panelt használja, csak
// XPT2046 érintőképernyővel és eltérő alap pin-kiosztással.
// FONTOS: az ellenőrzést a nyers felhasználói definíciókon végezzük,
// és csak UTÁNA aliasoljuk ST7789-re, különben a ST7789_XPT2046 két
// kijelzőnek számolódna.
#if (defined(ST7789_XPT2046) + defined(ST7789) + defined(ILI9341) + defined(ST7796) + defined(ILI9488)) != 1
  #error "Pontosan EGYET határozzon meg a következők közül: ST7789_XPT2046 / ST7789 / ILI9341 / ST7796 / ILI9488"
#endif

#if defined(ST7789_XPT2046)
  #ifndef ST7789
    #define ST7789
  #endif
#endif
