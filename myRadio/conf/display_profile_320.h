#pragma once
// ------------------ UI profile: 320-class displays ------------------ //
// Typical: ST7789 320x240 (rotated) and ILI9341 240x320.

#define UI_FONT_HEADER   24   // used for header/title measurements
#define UI_FONT_LABEL    20   // small labels (e.g. "Állomás", "Stream", etc.)
#define UI_FONT_TEXT     24   // main text measurements (station/artist/title)
#define UI_FONT_TEXT_SB  24   // main text rendering (sprites)
#define UI_FONT_MENU     24   // menu rendering (sprites, semibold)

#define UI_TEXT_LINE_EXTRA       2    // extra pixels added to sprite height
#define UI_HEADER_Y              6
#define UI_GAP_AFTER_HEADER     10
#define UI_GAP_LABEL_TO_TEXT     4
#define UI_LABEL_TEXT_OFFSET    -5    // keep your original baseline tweak
#define UI_GAP_AFTER_STATION_LINE 16
#define UI_GAP_STREAMLABEL_TO_TEXT 6
#define UI_GAP_ARTIST_TO_TITLE   2
#define UI_MARGIN_BOTTOM         6

// Extra vertical shift for Artist/Title block (e.g. on larger displays)
#define UI_ARTIST_TITLE_Y_SHIFT   0

#define UI_WIFI_W               34
#define UI_WIFI_H               18
#define UI_WIFI_MARGIN           4

#define UI_MENU_H               28

// Scrolling behaviour
#define UI_MARQUEE_MS           80
#define UI_SCROLL_STEP           3
#define UI_SCROLL_GAP           40
#define UI_MENU_MS              60
