#include "input_encoder_isr.h"
#include "../../Lovyan_config.h"

volatile int32_t g_encDelta = 0;
volatile uint8_t g_encHist = 0;

static const int8_t ENC_LUT[16] = {
  0, -1,  1,  0,
  1,  0,  0, -1,
 -1,  0,  0,  1,
  0,  1, -1,  0
};

void IRAM_ATTR encoderISR() {
  uint8_t s = (uint8_t)((digitalRead(ENC_A) ? 1 : 0) << 1) | (uint8_t)(digitalRead(ENC_B) ? 1 : 0);
  g_encHist = (uint8_t)((g_encHist << 2) | s);
  g_encDelta += ENC_LUT[g_encHist & 0x0F];
}
