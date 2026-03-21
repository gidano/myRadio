#include "backlight.h"
#include "../../Lovyan_config.h"

uint8_t g_brightness = 128;

void hw_backlight_init_pwm() {
  pinMode(TFT_BL, OUTPUT);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // v3.x: csatorna kezelést a core intézi
  ledcAttach(TFT_BL, BL_PWM_FREQ, BL_PWM_RES);
  ledcWrite(TFT_BL, g_brightness);
#else
  ledcSetup(BL_PWM_CH, BL_PWM_FREQ, BL_PWM_RES);
  ledcAttachPin(TFT_BL, BL_PWM_CH);
  ledcWrite(BL_PWM_CH, g_brightness);
#endif
}

void hw_backlight_set(uint8_t v) {
  g_brightness = v;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(TFT_BL, g_brightness);
#else
  ledcWrite(BL_PWM_CH, g_brightness);
#endif
}

uint8_t hw_backlight_get() {
  return g_brightness;
}
