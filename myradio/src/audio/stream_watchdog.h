#pragma once

#include <Arduino.h>
#include "audio_control.h"

struct StreamWatchdogBufferCtx {
  const String* stationUrl = nullptr;
  uint32_t* lastBufferCheckMs = nullptr;

  size_t* bufferFilled = nullptr;
  size_t* bufferFree = nullptr;
  size_t* bufferTotal = nullptr;
  int* bufferPercent = nullptr;

  size_t (*readBufferFilledFn)() = nullptr;
  size_t (*readBufferFreeFn)() = nullptr;

  bool* needStreamReconnect = nullptr;
  bool* paused = nullptr;
  const uint32_t* connectRequestedAtMs = nullptr;

  uint32_t refreshMs = 500;
  uint32_t startupGraceMs = 6000;
  uint32_t lowBufferHoldMs = 7000;
  int lowBufferPercent = 8;
};

void stream_watchdog_updateBuffer(StreamWatchdogBufferCtx& ctx);
void stream_watchdog_pollReconnect(AudioControlCtx& ctx);
void stream_watchdog_markWifiRestored(bool& needStreamReconnect, Print* log = nullptr);
