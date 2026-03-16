#pragma once

#include <Arduino.h>
#include "Audio.h"
#include "playlist_runtime.h"
#include "audio_control.h"
#include "stream_core.h"

struct StreamLifecyclePlaylistBind {
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

struct StreamLifecycleCoreBind {
  Audio* audio = nullptr;
  StreamCoreLogfFn logf = nullptr;
  StreamCoreLoglnFn logln = nullptr;
  uint32_t* connectRequestedAtMs = nullptr;
  String* lastConnectUrl = nullptr;
  int taskCore = 0;
  int taskStack = 10240;
  int taskPriority = 6;
};

void stream_lifecycle_bindPlaylistRuntime(PlaylistMetaCtx& ctx, const StreamLifecyclePlaylistBind& bind);
void stream_lifecycle_beginCore(const StreamLifecycleCoreBind& bind);
void stream_lifecycle_startCurrent(PlaylistMetaCtx& ctx, int volume, AudioSendVolumeFn sendVolume, AudioSendConnectFn sendConnect);
