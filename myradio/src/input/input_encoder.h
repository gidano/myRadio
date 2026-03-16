#pragma once

#include <Arduino.h>

struct InputEncoderAppCtx {
  int pinA;
  int pinB;
  int pinBtn;
  void (*isr)();
  volatile uint8_t* encHist;

  volatile int32_t* encDelta;

  uint8_t* mode;
  int* volume;
  int* menuIndex;
  int* stationCount;

  uint8_t modePlay;
  int volMin;
  int volMax;
  int pulsesPerStep;

  void (*onVolumeChanged)();
  void (*onMenuChanged)();
  void (*sendVolume)(uint8_t);
};

void input_encoder_init(const InputEncoderAppCtx& ctx);
void input_encoder_apply(const InputEncoderAppCtx& ctx);
