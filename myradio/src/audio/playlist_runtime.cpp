#include "playlist_runtime.h"

void playlist_runtime_bind(PlaylistMetaCtx& ctx, const PlaylistRuntimeBind& bind) {
  ctx.playlistUrls = bind.playlistUrls;
  ctx.playlistIndex = bind.playlistIndex;
  ctx.playlistSourceUrl = bind.playlistSourceUrl;
  ctx.stationUrl = bind.stationUrl;
  ctx.playUrl = bind.playUrl;
  ctx.paused = bind.paused;
  ctx.autoNextRequested = bind.autoNextRequested;
  ctx.autoNextRequestedAt = bind.autoNextRequestedAt;
  ctx.pendingTitle = bind.pendingTitle;
  ctx.newTitleFlag = bind.newTitleFlag;
  ctx.id3Artist = bind.id3Artist;
  ctx.id3Title = bind.id3Title;
  ctx.id3SeenAt = bind.id3SeenAt;
  ctx.pendingCodec = bind.pendingCodec;
  ctx.pendingBitrateK = bind.pendingBitrateK;
  ctx.pendingCh = bind.pendingCh;
  ctx.pendingSampleRate = bind.pendingSampleRate;
  ctx.pendingBitsPerSample = bind.pendingBitsPerSample;
  ctx.newStatusFlag = bind.newStatusFlag;
  ctx.connectFn = bind.connectFn;
  playlist_meta_setup(ctx);
}

bool playlist_runtime_startPlaybackCurrent(PlaylistMetaCtx& ctx, bool allowReloadPlaylist) {
  return playlist_meta_startPlaybackCurrent(ctx, allowReloadPlaylist);
}

bool playlist_runtime_advancePlaylistAndPlay(PlaylistMetaCtx& ctx) {
  return playlist_meta_advancePlaylistAndPlay(ctx);
}
