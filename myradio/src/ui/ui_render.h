#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>

// App-side setup ctx (az app_impl.cpp ezt használja: UIRenderCtx uctx; ...)
struct UIRenderCtx {
  String* stationName = nullptr;
  String* artist      = nullptr;
  String* title       = nullptr;

  lgfx::LGFX_Sprite* sprStation = nullptr;
  lgfx::LGFX_Sprite* sprArtist  = nullptr;
  lgfx::LGFX_Sprite* sprTitle   = nullptr;

  int* yStationName = nullptr;
  int* yArtist      = nullptr;
  int* yTitle       = nullptr;
};

// Setup
void ui_render_setup(const UIRenderCtx& ctx);

// --- metrics / center helpers (app_impl.cpp hívja) ---
void    ui_render_recalcTextMetrics();
bool    ui_render_scrollStation();
int32_t ui_render_centerXStation();
void    ui_render_markStationDrawn(int32_t xUsed);

// --- marquee engine (state_meta/app_impl hívja) ---
void ui_render_updateMarquee();
bool ui_render_anyScrollActive();

// --- pointer getters (state_meta és app_impl hívja) ---
int32_t*   ui_render_xStationPtr();
int32_t*   ui_render_xArtistPtr();
int32_t*   ui_render_xTitlePtr();
bool*      ui_render_holdPhasePtr();
uint32_t*  ui_render_trackChangedAtPtr();
bool*      ui_render_forceRedrawTextPtr();

// convenience (ha te akarod hívni)
void ui_render_forceRedraw();
void ui_render_notifyTrackChange();