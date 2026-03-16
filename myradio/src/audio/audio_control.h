#pragma once

#include <Arduino.h>

using AudioSendStopFn = void (*)();
using AudioSendVolumeFn = void (*)(uint8_t);
using AudioSendConnectFn = void (*)(const String&);
using AudioDrawStreamLabelFn = void (*)();
using AudioStartPlaybackFn = bool (*)(bool);
using AudioLogfFn = void (*)(const char*, ...);

struct AudioControlCtx {
  bool* paused = nullptr;
  int* volume = nullptr;
  String* playUrl = nullptr;
  String* stationUrl = nullptr;
  bool* needStreamReconnect = nullptr;

  AudioSendStopFn sendStop = nullptr;
  AudioSendVolumeFn sendVolume = nullptr;
  AudioSendConnectFn sendConnect = nullptr;
  AudioDrawStreamLabelFn drawStreamLabelFn = nullptr;
  AudioStartPlaybackFn startPlaybackCurrentFn = nullptr;
  AudioLogfFn logf = nullptr;
};

void audio_control_setPaused(AudioControlCtx& ctx, bool paused);
void audio_control_togglePaused(AudioControlCtx& ctx);
void audio_control_pollReconnect(AudioControlCtx& ctx);
