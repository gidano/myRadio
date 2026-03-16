#pragma once
// ------------------ UI profil: 480-os osztályú kijelzők ------------------ //
// Tipikus: ST7796 / ILI9488 (általában 320x480 natív; forgatás révén tájképes => 480x320).
// Itt szinte biztosan nagyobb betűméretet és nagyobb betűközöket szeretne.

#define UI_FONT_HEADER   28
#define UI_FONT_LABEL    20
#define UI_FONT_STREAM   20
#define UI_FONT_STATION  28
#define UI_FONT_ARTIST   24
#define UI_FONT_TITLE    20
#define UI_FONT_VOLUME   20
#define UI_FONT_MENU     28

#define UI_TEXT_LINE_EXTRA        4
#define UI_HEADER_Y              10
#define UI_GAP_AFTER_HEADER      14
#define UI_GAP_LABEL_TO_TEXT      6
#define UI_LABEL_TEXT_OFFSET     -7
#define UI_GAP_AFTER_STATION_LINE 22
#define UI_GAP_STREAMLABEL_TO_TEXT 8
#define UI_GAP_ARTIST_TO_TITLE    6
#define UI_MARGIN_BOTTOM          8

// Extra vertical shift for Artist/Title block (480x320-on kérésre lejjebb)
#define UI_ARTIST_TITLE_Y_SHIFT   15

#define UI_WIFI_W                44
#define UI_WIFI_H                24
#define UI_WIFI_MARGIN            6

#define UI_MENU_H                42

// Görgetési viselkedés (nagy képernyőkön kissé gyorsabb)
#define UI_MARQUEE_MS            60
#define UI_SCROLL_STEP            4
#define UI_SCROLL_GAP            60
#define UI_MENU_MS               50
