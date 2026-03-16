#pragma once
#include <Arduino.h>

struct InputButtonState {
  bool last = false;
  uint32_t downAt = 0;
  bool longFired = false;
};

struct InputButtonCtx {
  uint8_t pin = 0;
  bool activeLow = true;
  uint32_t longPressMs = 650;
  InputButtonState* state = nullptr;
  void (*onShortPress)() = nullptr;
  void (*onLongPress)() = nullptr;
};

void input_button_apply(InputButtonCtx& ctx);
