#pragma once

// Select a UI profile based on the chosen display driver.
// (Drivers are selected in Lovyan_config.h)

#if defined(ST7789) || defined(ILI9341)
  #include "display_profile_320.h"
#elif defined(ST7796) || defined(ILI9488)
  #include "display_profile_480.h"
#else
  #error "Nincs megfelelő kijelzőprofil. Határozza meg az ST7789 / ILI9341 / ST7796 / ILI9488 egyikét a Lovyan_config.h fájlban."
#endif
