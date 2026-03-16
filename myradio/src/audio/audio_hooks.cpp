#include <Arduino.h>
#include "../ui/vu_meter.h"

// ESP32-audioI2S (a te builded szerint) ezt a szignatúrát hívja:
// audio_process_i2s(short*, long, bool*)
void IRAM_ATTR audio_process_i2s(int16_t* outBuff, int32_t validSamples, bool* continueI2S)
{
  if (continueI2S) *continueI2S = true;
  if (!outBuff || validSamples < 2) return;

  // ISR-közeli: csak 1 stereo frame -> gyors, heap/millis/Serial nélkül
  int16_t s[2];
  s[0] = outBuff[0];
  s[1] = outBuff[1];
  vu_feedStereoISR(s, 2);
}
