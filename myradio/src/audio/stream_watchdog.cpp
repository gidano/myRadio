#include "stream_watchdog.h"

#include <WiFi.h>

void stream_watchdog_updateBuffer(StreamWatchdogBufferCtx& ctx) {
  if (!ctx.stationUrl || !ctx.lastBufferCheckMs || !ctx.bufferFilled || !ctx.bufferFree ||
      !ctx.bufferTotal || !ctx.bufferPercent || !ctx.readBufferFilledFn || !ctx.readBufferFreeFn) {
    return;
  }

  static uint32_t lowBufferSinceMs = 0;

  if (ctx.stationUrl->length() == 0) {
    lowBufferSinceMs = 0;
    return;
  }

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

  const bool paused = ctx.paused && *ctx.paused;
  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  const bool afterStartupGrace =
    (!ctx.connectRequestedAtMs || *ctx.connectRequestedAtMs == 0 ||
     (now - *ctx.connectRequestedAtMs) >= ctx.startupGraceMs);
  const bool lowBuffer = (*ctx.bufferPercent <= ctx.lowBufferPercent);

  if (!paused && wifiOk && afterStartupGrace && lowBuffer) {
    if (lowBufferSinceMs == 0) lowBufferSinceMs = now;

    if (ctx.needStreamReconnect && !*ctx.needStreamReconnect &&
        (now - lowBufferSinceMs) >= ctx.lowBufferHoldMs) {
      *ctx.needStreamReconnect = true;
    }
  } else {
    lowBufferSinceMs = 0;
  }
}

void stream_watchdog_pollReconnect(AudioControlCtx& ctx) {
  audio_control_pollReconnect(ctx);
}

void stream_watchdog_markWifiRestored(bool& needStreamReconnect, Print* log) {
  if (log) log->println("[WiFi] Restored.");
  needStreamReconnect = true;
}
