#pragma once
#include <Arduino.h>
#include <vector>
#include "Audio.h"

typedef void (*PlaylistConnectFn)(const String& url);

struct PlaylistMetaCtx {
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

void playlist_meta_setup(PlaylistMetaCtx& ctx);

bool playlist_meta_loadM3U(PlaylistMetaCtx& ctx, const String& m3uUrl);
bool playlist_meta_startPlaybackCurrent(PlaylistMetaCtx& ctx, bool allowReloadPlaylist = true);
void playlist_meta_requestAutoNextTrack(PlaylistMetaCtx& ctx);
bool playlist_meta_advancePlaylistAndPlay(PlaylistMetaCtx& ctx);
bool playlist_meta_stepPlaylist(PlaylistMetaCtx& ctx, int delta, bool connectNow = true);
int  playlist_meta_trackCount(const PlaylistMetaCtx& ctx);
int  playlist_meta_trackIndex(const PlaylistMetaCtx& ctx);
void playlist_meta_setNowPlayingFromUrl(PlaylistMetaCtx& ctx, const String& url);
void playlist_meta_handleAudioInfo(PlaylistMetaCtx& ctx, Audio::msg_t m);
