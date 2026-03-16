#pragma once
#include <Arduino.h>
#include "ui_display.h"

void ui_bottom_bar_bind(const UIDisplayCtx& ctx);
void ui_bottom_bar_drawWifiIcon(bool connected);
void ui_bottom_bar_updateWifiIconOnly();
void ui_bottom_bar_drawBufferIndicator(int percent);
void ui_bottom_bar_updateBufferIndicatorOnly(int percent);
void ui_bottom_bar_drawBottomBar(int volume, int bufferPercent, bool wifiConnected);
