#include "input_encoder.h"
#include "input_rotary.h"

void input_encoder_init(const InputEncoderAppCtx& ctx) {
  input_rotary_init(ctx.pinA, ctx.pinB, ctx.pinBtn, ctx.isr, ctx.encHist);
}

void input_encoder_apply(const InputEncoderAppCtx& ctx) {
  InputRotaryCtx rctx;
  rctx.encDelta = ctx.encDelta;
  rctx.mode = ctx.mode;
  rctx.volume = ctx.volume;
  rctx.menuIndex = ctx.menuIndex;
  rctx.stationCount = ctx.stationCount;
  rctx.modePlay = ctx.modePlay;
  rctx.volMin = ctx.volMin;
  rctx.volMax = ctx.volMax;
  rctx.pulsesPerStep = ctx.pulsesPerStep;
  rctx.onVolumeChanged = ctx.onVolumeChanged;
  rctx.onMenuChanged = ctx.onMenuChanged;
  rctx.sendVolume = ctx.sendVolume;
  input_rotary_apply(rctx);
}
