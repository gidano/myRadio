#pragma once
#include <Arduino.h>

extern uint8_t g_brightness;

void hw_backlight_init_pwm();
void hw_backlight_set(uint8_t v);
uint8_t hw_backlight_get();
