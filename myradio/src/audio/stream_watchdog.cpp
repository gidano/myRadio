#include "stream_watchdog.h"

void stream_watchdog_updateBuffer(StreamWatchdogBufferCtx& ctx) {
  if (!ctx.stationUrl || !ctx.lastBufferCheckMs || !ctx.bufferFilled || !ctx.bufferFree ||
      !ctx.bufferTotal || !ctx.bufferPercent || !ctx.readBufferFilledFn || !ctx.readBufferFreeFn) {
    return;
  }

  if (ctx.stationUrl->length() == 0) return;

  uint32_t now = millis();
  if (now - *ctx.lastBufferCheckMs <= ctx.refreshMs) return;

  *ctx.lastBufferCheckMs = now;
  *ctx.bufferFilled = ctx.readBufferFilledFn();
  *ctx.bufferFree = ctx.readBufferFreeFn();
  *ctx.bufferTotal = *ctx.bufferFilled + *ctx.bufferFree;

  if (*ctx.bufferTotal > 0) {
    *ctx.bufferPercent = (int)((*ctx.bufferFilled * 100) / *ctx.bufferTotal);
  } else {
    *ctx.bufferPercent = 0;
  }
}

void stream_watchdog_pollReconnect(AudioControlCtx& ctx) {
  audio_control_pollReconnect(ctx);
}

void stream_watchdog_markWifiRestored(bool& needStreamReconnect, Print* log) {
  if (log) log->println("[WiFi] Restored.");
  needStreamReconnect = true;
}
