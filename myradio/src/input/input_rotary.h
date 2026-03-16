#pragma once
#include <Arduino.h>

struct InputRotaryCtx {
  volatile int32_t* encDelta;

  uint8_t* mode;          // (uint8_t*)&g_mode
  int*     volume;        // &g_Volume (nálad int)
  int*     menuIndex;
  int*     stationCount;

  uint8_t modePlay;
  int     volMin;
  int     volMax;
  int     pulsesPerStep;

  void (*onVolumeChanged)();   // updateVolumeOnly()
  void (*onMenuChanged)();     // drawMenuScreen()
  void (*sendVolume)(uint8_t); // audioSendVolume(uint8_t)
};

void input_rotary_init(int pinA, int pinB, int pinBtn, void (*isr)(), volatile uint8_t* encHist);
void input_rotary_apply(InputRotaryCtx& ctx);