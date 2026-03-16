#include "input_button.h"

void input_button_apply(InputButtonCtx& ctx) {
  if (!ctx.state) return;

  const bool raw = digitalRead(ctx.pin);
  const bool pressed = ctx.activeLow ? !raw : raw;
  const uint32_t now = millis();

  if (!ctx.state->last && pressed) {
    ctx.state->downAt = now;
    ctx.state->longFired = false;
  }

  if (pressed && !ctx.state->longFired && (now - ctx.state->downAt >= ctx.longPressMs)) {
    ctx.state->longFired = true;
    if (ctx.onLongPress) ctx.onLongPress();
  }

  if (ctx.state->last && !pressed) {
    if (!ctx.state->longFired && ctx.onShortPress) {
      ctx.onShortPress();
    }
  }

  ctx.state->last = pressed;
}
