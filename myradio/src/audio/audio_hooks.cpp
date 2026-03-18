// audio_hooks.cpp
// ============================================================
// Audio-I2S 3.4.4 → 3.4.5 TÉNYLEGES változás (GitHub alapján):
//
// 3.4.4 (régi kód):
//   audio_process_i2s(int16_t* outBuff, int32_t validSamples, bool* continueI2S)
//   – puffer: int16_t*, L/R váltakozva, 1 stereo pár = 2 int16_t elem
//
// 3.4.5 (jelenlegi):
//   audio_process_i2s(int32_t* outBuff, int16_t validSamples, bool* continueI2S)
//   – puffer: int32_t*, 1 stereo pár = 2 int32_t elem
//     outBuff[0] = bal csatorna (32 bit, IDF5 DMA formátum)
//     outBuff[1] = jobb csatorna (32 bit)
//   – bool* continueI2S MEGMARADT (NEM tűnt el!)
//   – validSamples típusa int32_t → int16_t lett
//
// A 32-bites pufferből a 16-bites amplitúdó kinyerése:
//   int16_t L = (int16_t)(outBuff[0] >> 16);
//   int16_t R = (int16_t)(outBuff[1] >> 16);
// (A IDF5 I2S DMA a hangadatot a felső 16 bitbe igazítja.)
// ============================================================

#include <Arduino.h>
#include "../ui/vu_meter.h"

// *** 3.4.5-ös szignatúra ***
void IRAM_ATTR audio_process_i2s(int32_t* outBuff, int16_t validSamples, bool* continueI2S)
{
  if (continueI2S) *continueI2S = true;
  if (!outBuff || validSamples < 1) return;

  // 32-bites DMA pufferből a VU-hoz szükséges 16-bites értékek kinyerése.
  // Az IDF5 I2S driver a hangmintát a 32 bites word felső 16 bitjébe helyezi.
  int16_t s[2];
  s[0] = (int16_t)(outBuff[0] >> 16);   // bal csatorna
  s[1] = (int16_t)(outBuff[1] >> 16);   // jobb csatorna
  vu_feedStereoISR(s, 2);
}
