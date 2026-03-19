#pragma once
#include <Arduino.h>

struct InputTouchRuntimeState {
  bool initialized = false;
  bool pressed = false;
  bool longPressFired = false;
  bool dragStarted = false;
  int startX = 0;
  int startY = 0;
  int lastX = 0;
  int lastY = 0;
  uint32_t pressedAtMs = 0;
  uint32_t lastReleaseMs = 0;
};

struct InputTouchCtx {
  bool* enabled = nullptr;
  InputTouchRuntimeState* state = nullptr;
  int screenW = 0;
  int screenH = 0;

  void (*onTap)(int x, int y) = nullptr;
  void (*onLongPress)(int x, int y) = nullptr;
  void (*onDrag)(int startX, int startY, int x, int y) = nullptr;
};

void input_touch_apply(const InputTouchCtx& ctx);
