#pragma once
#include <Arduino.h>

extern volatile int32_t g_encDelta;
extern volatile uint8_t g_encHist;

void IRAM_ATTR encoderISR();
