#pragma once
#include <Arduino.h>
#include "../../Lovyan_config.h"
#include "../core/station_store.h"

struct UIStationSelectorCtx {
  #if defined(SSD1322)
  oledgfx::LGFX_Device* tft = nullptr;
  oledgfx::LGFX_Sprite* sprMenu = nullptr;
  #else
  lgfx::LGFX_Device* tft = nullptr;
  lgfx::LGFX_Sprite* sprMenu = nullptr;
  #endif

  Station* stations = nullptr;
  int* stationCount = nullptr;
  int* menuIndex = nullptr;

  int* screenW = nullptr;
  int* screenH = nullptr;
  int* menuListTop = nullptr;
  int* menuListHeight = nullptr;
  int* menuItemH = nullptr;
  int* menuNameY = nullptr;

  const String* labelFontPath = nullptr;
  const String* activeFontPath = nullptr;

  uint32_t menuScrollIntervalMs = 40;
  uint16_t colorBg = TFT_BLACK;
  uint16_t colorActiveBg = TFT_DARKGREY;
  uint16_t colorActiveBorder = TFT_GOLD;
  uint16_t colorActiveText = TFT_GOLD;
  uint16_t colorSideText = TFT_WHITE;
  uint16_t colorSideFarText = TFT_LIGHTGREY;
};

void ui_stationSelectorInit(const UIStationSelectorCtx& ctx);
void ui_stationSelectorBegin(int currentIndex);
void ui_stationSelectorEnd();
bool ui_stationSelectorActive();
int  ui_stationSelectorSelected();
void ui_stationSelectorRotate(int steps);
void ui_stationSelectorDraw();
void ui_stationSelectorTick();
