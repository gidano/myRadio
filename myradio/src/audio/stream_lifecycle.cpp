#include "stream_lifecycle.h"

void stream_lifecycle_bindPlaylistRuntime(PlaylistMetaCtx& ctx, const StreamLifecyclePlaylistBind& bind) {
  PlaylistRuntimeBind runtime{};
  runtime.playlistUrls = bind.playlistUrls;
  runtime.playlistIndex = bind.playlistIndex;
  runtime.playlistSourceUrl = bind.playlistSourceUrl;
  runtime.stationUrl = bind.stationUrl;
  runtime.playUrl = bind.playUrl;
  runtime.paused = bind.paused;
  runtime.autoNextRequested = bind.autoNextRequested;
  runtime.autoNextRequestedAt = bind.autoNextRequestedAt;
  runtime.pendingTitle = bind.pendingTitle;
  runtime.newTitleFlag = bind.newTitleFlag;
  runtime.id3Artist = bind.id3Artist;
  runtime.id3Title = bind.id3Title;
  runtime.id3SeenAt = bind.id3SeenAt;
  runtime.pendingCodec = bind.pendingCodec;
  runtime.pendingBitrateK = bind.pendingBitrateK;
  runtime.pendingCh = bind.pendingCh;
  runtime.pendingSampleRate = bind.pendingSampleRate;
  runtime.pendingBitsPerSample = bind.pendingBitsPerSample;
  runtime.newStatusFlag = bind.newStatusFlag;
  runtime.connectFn = bind.connectFn;
  playlist_runtime_bind(ctx, runtime);
}

void stream_lifecycle_beginCore(const StreamLifecycleCoreBind& bind) {
  StreamCoreConfig scfg{};
  scfg.audio = bind.audio;
  scfg.logf = bind.logf;
  scfg.logln = bind.logln;
  scfg.connectRequestedAtMs = bind.connectRequestedAtMs;
  scfg.lastConnectUrl = bind.lastConnectUrl;
  scfg.taskCore = bind.taskCore;
  scfg.taskStack = bind.taskStack;
  scfg.taskPriority = bind.taskPriority;
  stream_core_begin(scfg);
}

void stream_lifecycle_startCurrent(PlaylistMetaCtx& ctx, int volume, AudioSendVolumeFn sendVolume, AudioSendConnectFn sendConnect) {
  if (sendVolume) sendVolume((uint8_t)volume);
  if (playlist_runtime_startPlaybackCurrent(ctx, true) && ctx.playUrl && ctx.playUrl->length() && sendConnect) {
    sendConnect(*ctx.playUrl);
  }
}
