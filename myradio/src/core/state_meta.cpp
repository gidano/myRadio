#include "state_meta.h"

void state_meta_setup() {
  // Most nincs init teendő.
  // (Később ide jöhet pl. default állapotok, debug, stb.)
}

static bool ctxOkTitle(const StateMetaCtx& c) {
  return c.newTitleFlag && c.pendingTitle && c.splitArtistTitleFn &&
         c.artistOut && c.titleOut &&
         c.trackChangedAtMs && c.forceRedrawText && c.holdPhase &&
         c.xStation && c.xArtist && c.xTitle &&
         c.mode && c.updateMarqueeFn;
}

static bool ctxOkStatus(const StateMetaCtx& c) {
  return c.newStatusFlag &&
         c.codecCur && c.bitrateCurKbps &&
         c.pendingCodec && c.pendingBitrateKbps &&
         c.chCur && c.sampleRateCur && c.bitsPerSampleCur &&
         c.pendingCh && c.pendingSampleRate && c.pendingBitsPerSample &&
         c.drawCodecIconFn && c.drawBottomBarFn && c.drawStreamLabelFn &&
         c.mode;
}

void state_meta_poll(StateMetaCtx& ctx) {
  // ---------------- Title ----------------
  if (ctxOkTitle(ctx) && *(ctx.newTitleFlag)) {
    *(ctx.newTitleFlag) = false;

    // Ezt a logikát 1:1-ben vettük át a te app_loop-odból:
    String fixed = *(ctx.pendingTitle);

    String a, t;
    ctx.splitArtistTitleFn(fixed, a, t);

    if (a.length() == 0) {
      *(ctx.artistOut) = "";
      *(ctx.titleOut)  = t;
    } else {
      *(ctx.artistOut) = a;
      *(ctx.titleOut)  = t;
    }

    *(ctx.trackChangedAtMs) = millis();
    *(ctx.forceRedrawText)  = true;
    *(ctx.holdPhase)        = true;
    *(ctx.xStation)         = 0;
    *(ctx.xArtist)          = 0;
    *(ctx.xTitle)           = 0;

    if (*(ctx.mode) == ctx.modePlay) {
      ctx.updateMarqueeFn();
    }
  }

  // ---------------- Status ----------------
  if (ctxOkStatus(ctx) && *(ctx.newStatusFlag)) {
    *(ctx.newStatusFlag) = false;

    if (ctx.pendingCodec->length())      *(ctx.codecCur) = *(ctx.pendingCodec);
    if (*(ctx.pendingBitrateKbps) > 0)   *(ctx.bitrateCurKbps) = *(ctx.pendingBitrateKbps);
    if (*(ctx.pendingCh) > 0)            *(ctx.chCur) = *(ctx.pendingCh);
    if (*(ctx.pendingSampleRate) > 0)    *(ctx.sampleRateCur) = *(ctx.pendingSampleRate);
    if (*(ctx.pendingBitsPerSample) > 0) *(ctx.bitsPerSampleCur) = *(ctx.pendingBitsPerSample);

    // ugyanaz a UI frissítés, mint eddig:
    ctx.drawCodecIconFn();
    ctx.drawBottomBarFn();
    if (*(ctx.mode) == ctx.modePlay) {
      ctx.drawStreamLabelFn();
    }
  }
}
