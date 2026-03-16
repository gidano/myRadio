#include "audio_control.h"

#include <WiFi.h>

void audio_control_setPaused(AudioControlCtx& ctx, bool paused) {
  if (!ctx.paused) return;
  if (*ctx.paused == paused) return;

  *ctx.paused = paused;

  if (*ctx.paused) {
    if (ctx.sendStop) ctx.sendStop();
  } else {
    const String url = (ctx.playUrl && ctx.playUrl->length()) ? *ctx.playUrl
                      : ((ctx.stationUrl) ? *ctx.stationUrl : String());
    if (ctx.sendVolume && ctx.volume) ctx.sendVolume(*ctx.volume);
    if (ctx.sendConnect && url.length()) ctx.sendConnect(url);
  }

  if (ctx.drawStreamLabelFn) ctx.drawStreamLabelFn();
}

void audio_control_togglePaused(AudioControlCtx& ctx) {
  if (!ctx.paused) return;
  audio_control_setPaused(ctx, !*ctx.paused);
}

void audio_control_pollReconnect(AudioControlCtx& ctx) {
  static uint32_t lastReconnectAttemptMs = 0;

  if (!ctx.needStreamReconnect || !ctx.paused || !ctx.stationUrl) return;
  if (!*ctx.needStreamReconnect) return;
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t now = millis();
  if (now - lastReconnectAttemptMs < 5000) return;
  lastReconnectAttemptMs = now;

  if (*ctx.paused || ctx.stationUrl->length() == 0) {
    *ctx.needStreamReconnect = false;
    return;
  }

  if (ctx.logf) ctx.logf("[AUDIO] reconnect watchdog: full restart\n");
  if (ctx.sendStop) ctx.sendStop();
  delay(120);

  bool prepared = true;
  if (ctx.startPlaybackCurrentFn) prepared = ctx.startPlaybackCurrentFn(true);

  if (prepared && ctx.playUrl && ctx.playUrl->length() && ctx.sendConnect) {
    if (ctx.sendVolume && ctx.volume) ctx.sendVolume((uint8_t)*ctx.volume);
    if (ctx.logf) ctx.logf("[AUDIO] reconnect -> %s\n", ctx.playUrl->c_str());
    ctx.sendConnect(*ctx.playUrl);
    *ctx.needStreamReconnect = false;
  }
}
