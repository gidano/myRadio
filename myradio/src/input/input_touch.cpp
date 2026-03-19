#include "input_touch.h"
#include "../hw/board_pins.h"

#if TOUCH_MODEL == TOUCH_XPT2046
  #include <SPI.h>
  #include <XPT2046_Touchscreen.h>

  #if TOUCH_IRQ >= 0
    static XPT2046_Touchscreen g_ts(TOUCH_CS, TOUCH_IRQ);
  #else
    static XPT2046_Touchscreen g_ts(TOUCH_CS);
  #endif

  static SPIClass g_touchSPI(TOUCH_SPI_HOST);

  static int mapAxis(int value, int inMin, int inMax, int outMax) {
    if (inMax == inMin) return 0;
    long v = ((long)(value - inMin) * (long)(outMax - 1)) / (long)(inMax - inMin);
    if (v < 0) v = 0;
    if (v >= outMax) v = outMax - 1;
    return (int)v;
  }

  static bool rawToScreen(int rawX, int rawY, int& sx, int& sy, int screenW, int screenH) {
    int tx = rawX;
    int ty = rawY;

    if (TOUCH_SWAP_XY) {
      const int t = tx;
      tx = ty;
      ty = t;
    }

    sx = mapAxis(tx, TOUCH_CAL_X_MIN, TOUCH_CAL_X_MAX, screenW);
    sy = mapAxis(ty, TOUCH_CAL_Y_MIN, TOUCH_CAL_Y_MAX, screenH);

    if (TOUCH_INVERT_X) sx = (screenW - 1) - sx;
    if (TOUCH_INVERT_Y) sy = (screenH - 1) - sy;

    if (sx < 0) sx = 0;
    if (sx >= screenW) sx = screenW - 1;
    if (sy < 0) sy = 0;
    if (sy >= screenH) sy = screenH - 1;
    return true;
  }
#endif

void input_touch_apply(const InputTouchCtx& ctx) {
  if (!ctx.enabled || !(*ctx.enabled) || !ctx.state || ctx.screenW <= 0 || ctx.screenH <= 0) return;

#if TOUCH_MODEL == TOUCH_XPT2046
  auto* st = ctx.state;
  if (!st->initialized) {
    g_touchSPI.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    g_ts.begin(g_touchSPI);
    g_ts.setRotation(TOUCH_ROTATION & 3);
    st->initialized = true;
    Serial.printf("[TOUCH] init cs=%d sck=%d miso=%d mosi=%d rot=%d zthr=%d\n",
                  TOUCH_CS, TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, (TOUCH_ROTATION & 3), TOUCH_Z_THRESHOLD);
  }

  const bool isTouched = g_ts.touched();
  if (!isTouched) {
    if (st->pressed) {
      const int dx = st->lastX - st->startX;
      const int dy = st->lastY - st->startY;
      const uint32_t heldMs = millis() - st->pressedAtMs;
      const int move2 = dx * dx + dy * dy;
      const int tol2 = TOUCH_TAP_MOVE_TOLERANCE * TOUCH_TAP_MOVE_TOLERANCE;
      if (!st->longPressFired && heldMs >= 30 && move2 <= tol2 && ctx.onTap) {
        ctx.onTap(st->lastX, st->lastY);
      }
      st->pressed = false;
      st->longPressFired = false;
      st->dragStarted = false;
      st->lastReleaseMs = millis();
    }
    return;
  }

  TS_Point p = g_ts.getPoint();
  static uint32_t lastRawLogMs = 0;
  const uint32_t nowRaw = millis();
  if (nowRaw - lastRawLogMs >= 250) {
    Serial.printf("[TOUCH] raw x=%d y=%d z=%d touched=%d\n", p.x, p.y, p.z, (int)isTouched);
    lastRawLogMs = nowRaw;
  }
  if (p.z < TOUCH_Z_THRESHOLD) return;

  int sx = 0, sy = 0;
  if (!rawToScreen(p.x, p.y, sx, sy, ctx.screenW, ctx.screenH)) return;

  const uint32_t now = millis();
  if (!st->pressed) {
    if (now - st->lastReleaseMs < TOUCH_DEBOUNCE_MS) return;
    st->pressed = true;
    st->longPressFired = false;
    st->dragStarted = false;
    st->pressedAtMs = now;
    st->startX = st->lastX = sx;
    st->startY = st->lastY = sy;
    return;
  }

  st->lastX = sx;
  st->lastY = sy;
  const int dx = st->lastX - st->startX;
  const int dy = st->lastY - st->startY;
  const int move2 = dx * dx + dy * dy;
  const int tol2 = TOUCH_TAP_MOVE_TOLERANCE * TOUCH_TAP_MOVE_TOLERANCE;

  if (!st->longPressFired && move2 <= tol2 && (now - st->pressedAtMs >= TOUCH_LONG_PRESS_MS)) {
    st->longPressFired = true;
    if (ctx.onLongPress) ctx.onLongPress(st->lastX, st->lastY);
  }

  if (move2 > tol2) {
    st->dragStarted = true;
    if (ctx.onDrag) ctx.onDrag(st->startX, st->startY, st->lastX, st->lastY);
  }
#else
  (void)ctx;
#endif
}
