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

  uint32_t refreshMs = 500;
};

void stream_watchdog_updateBuffer(StreamWatchdogBufferCtx& ctx);
void stream_watchdog_pollReconnect(AudioControlCtx& ctx);
void stream_watchdog_markWifiRestored(bool& needStreamReconnect, Print* log = nullptr);
