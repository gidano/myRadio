#pragma once
#include <Arduino.h>
#include "Audio.h"

using StreamCoreLogfFn = void (*)(const char*, ...);
using StreamCoreLoglnFn = void (*)(const char*);

struct StreamCoreConfig {
  Audio* audio = nullptr;
  StreamCoreLogfFn logf = nullptr;
  StreamCoreLoglnFn logln = nullptr;

  // app_impl tulajdonában maradó állapotok
  uint32_t* connectRequestedAtMs = nullptr;
  String*   lastConnectUrl = nullptr;

  int taskCore = 0;
  int taskStack = 10240;
  int taskPriority = 6;
};

void stream_core_begin(const StreamCoreConfig& cfg);

void stream_core_sendVolume(uint8_t v);
void stream_core_sendStop();
void stream_core_sendConnect(const String& url);

void stream_core_readBuffer(size_t& filled, size_t& freeb, size_t& total, int& percent);
