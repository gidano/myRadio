#pragma once

#include "playlist_meta.h"

struct PlaylistRuntimeBind {
  std::vector<String>* playlistUrls = nullptr;
  int*                 playlistIndex = nullptr;
  String*              playlistSourceUrl = nullptr;

  String* stationUrl = nullptr;
  String* playUrl    = nullptr;
  bool*   paused     = nullptr;

  volatile bool*     autoNextRequested = nullptr;
  volatile uint32_t* autoNextRequestedAt = nullptr;

  String*   pendingTitle = nullptr;
  volatile bool* newTitleFlag = nullptr;

  String*   id3Artist = nullptr;
  String*   id3Title  = nullptr;
  uint32_t* id3SeenAt = nullptr;

  String* pendingCodec = nullptr;
  int*    pendingBitrateK = nullptr;
  int*    pendingCh = nullptr;
  int*    pendingSampleRate = nullptr;
  int*    pendingBitsPerSample = nullptr;
  volatile bool* newStatusFlag = nullptr;

  PlaylistConnectFn connectFn = nullptr;
};

void playlist_runtime_bind(PlaylistMetaCtx& ctx, const PlaylistRuntimeBind& bind);
bool playlist_runtime_startPlaybackCurrent(PlaylistMetaCtx& ctx, bool allowReloadPlaylist = true);
bool playlist_runtime_advancePlaylistAndPlay(PlaylistMetaCtx& ctx);
