#pragma once
#include <Arduino.h>

// A state_meta modul célja: a cím (title) és státusz (codec/bitrate/format) flag-ek
// feldolgozása az app_loop()-ból kiszervezve, nagy átírás nélkül.
//
// MŰKÖDÉS:
// - Az app_impl.cpp-ben meglévő változók címeit/pointereit egy StateMetaCtx-be töltjük.
// - state_meta_poll(ctx) lefuttatja a régi "if (g_newTitleFlag) { ... }" és
//   "if (g_newStatusFlag) { ... }" logikát, változatlan viselkedéssel.

typedef void (*SplitArtistTitleFn)(const String& in, String& artist, String& title);
typedef void (*VoidFn)();

struct StateMetaCtx {
  // ---- Title flag + adatok ----
  volatile bool* newTitleFlag = nullptr;   // g_newTitleFlag
  String*        pendingTitle = nullptr;   // g_pendingTitle (nyers/összerakott cím)
  SplitArtistTitleFn splitArtistTitleFn = nullptr;

  // output (UI használja)
  String* artistOut = nullptr;             // g_artist
  String* titleOut  = nullptr;             // g_title

  // marquee/reset / UI state
  uint32_t* trackChangedAtMs = nullptr;    // trackChangedAt
  bool*     forceRedrawText  = nullptr;    // g_forceRedrawText
  bool*     holdPhase        = nullptr;    // holdPhase
  int32_t*  xStation         = nullptr;    // xStation
  int32_t*  xArtist          = nullptr;    // xArtist
  int32_t*  xTitle           = nullptr;    // xTitle

  uint8_t*  mode             = nullptr;    // &g_mode (uint8_t*)
  uint8_t   modePlay         = 0;          // MODE_PLAY
  VoidFn    updateMarqueeFn  = nullptr;    // updateMarquee()

  // ---- Status flag + adatok ----
  volatile bool* newStatusFlag = nullptr;  // g_newStatusFlag

  String* codecCur       = nullptr;        // g_codec
  int*    bitrateCurKbps = nullptr;        // g_bitrateK

  String* pendingCodec        = nullptr;   // g_pendingCodec
  int*    pendingBitrateKbps  = nullptr;   // g_pendingBitrateK

  int* chCur            = nullptr;         // g_ch
  int* sampleRateCur    = nullptr;         // g_sampleRate
  int* bitsPerSampleCur = nullptr;         // g_bitsPerSample

  int* pendingCh            = nullptr;     // g_pendingCh
  int* pendingSampleRate    = nullptr;     // g_pendingSampleRate
  int* pendingBitsPerSample = nullptr;     // g_pendingBitsPerSample

  // UI frissítő függvények (app_impl.cpp-ből)
  VoidFn drawCodecIconFn   = nullptr;      // drawCodecIconTopLeft()
  VoidFn drawBottomBarFn   = nullptr;      // drawBottomBar()
  VoidFn drawStreamLabelFn = nullptr;      // drawStreamLabelLine()
};

// setup jelenleg üres, de a modul-struktúra miatt meghagyjuk
void state_meta_setup();

// A régi app_loop title/status blokkok modulba szervezett megfelelője
void state_meta_poll(StateMetaCtx& ctx);
