#pragma once

template <typename T>
inline void ssd1322_draw_debug_boot(T& tft) {
  tft.fillScreen(0x0000);
}

template <typename T>
inline void ssd1322_draw_debug_wifi(T& tft) {
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setCursor(10, 10);
  tft.print("WIFI...");
}

template <typename T>
inline void ssd1322_draw_debug_stream(T& tft) {
  tft.fillScreen(0x0000);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setCursor(10, 10);
  tft.print("STREAM...");
}
