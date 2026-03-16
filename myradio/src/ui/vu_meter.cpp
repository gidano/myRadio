#include "vu_meter.h"
#include <Arduino.h>

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ISR-ből is íródik -> volatile
static volatile uint16_t envL = 0, envR = 0;
static volatile uint8_t  lvlL = 0, lvlR = 0;
static volatile uint8_t  peakL = 0, peakR = 0;

// AGC gain (100 = 1.0x) – UI oldalon állítjuk, ISR csak olvassa.
static volatile uint16_t agcGainX100 = 100;

// Peak decay idő: NEM ISR-ben kezeljük
static uint32_t lastPeakDecay = 0;

static inline uint16_t absv16(int16_t v) { return (v < 0) ? (uint16_t)(-v) : (uint16_t)v; }

static inline void feed_core(const int16_t* s, size_t n, bool isr)
{
  if (!s || n < 2) return;

  // Peak keresés (n nálunk többnyire 2 lesz, de működik nagyobbra is)
  uint16_t pL = 0, pR = 0;
  n &= ~1U; // páros (L/R)
  for (size_t i = 0; i < n; i += 2) {
    uint16_t l = absv16(s[i]);
    uint16_t r = absv16(s[i + 1]);
    if (l > pL) pL = l;
    if (r > pR) pR = r;
  }

  if (isr) portENTER_CRITICAL_ISR(&mux);
  else     portENTER_CRITICAL(&mux);

  // olcsó envelope (integer)
  auto follow = [](uint16_t env, uint16_t p) -> uint16_t {
    if (p > env) {
      env = env + (uint16_t)(((uint32_t)(p - env) * 3U) / 4U);
    } else {
      env = env - (uint16_t)(((uint32_t)(env - p)) / 10U);
    }
    return env;
  };

  envL = follow((uint16_t)envL, pL);
  envR = follow((uint16_t)envR, pR);

  // Érzékenyebb (nemlineáris) skálázás: sqrt, hogy kis jelszinteknél is szépen mozogjon.
  // env: 0..32767 -> t: 0..10000 -> sqrt(t): 0..100
  auto isqrt_u32 = [](uint32_t x) -> uint16_t {
    uint32_t op = x;
    uint32_t res = 0;
    uint32_t one = 1uL << 30; // The second-to-top bit
    while (one > op) one >>= 2;
    while (one != 0) {
      if (op >= res + one) { op -= res + one; res = res + 2 * one; }
      res >>= 1;
      one >>= 2;
    }
    return (uint16_t)res;
  };

  uint32_t tL = ((uint32_t)envL * 10000UL) / 32767UL;
  uint32_t tR = ((uint32_t)envR * 10000UL) / 32767UL;

  // Extra "gain" a sqrt előtt (fixpontos):
  // - VU_GAIN_X100: manuális alap érzékenység
  // - agcGainX100: automatikus korrekció (ha engedélyezve)
  uint16_t agc = 100;
#if VU_AGC_ENABLE
  agc = (uint16_t)agcGainX100;
#endif
  uint32_t effGainX100 = ((uint32_t)VU_GAIN_X100 * (uint32_t)agc) / 100UL; // 100 = 1.0x
  tL = (tL * effGainX100) / 100UL;
  tR = (tR * effGainX100) / 100UL;

  if (tL > 10000) tL = 10000;
  if (tR > 10000) tR = 10000;

  uint16_t l = isqrt_u32(tL); // 0..100
  uint16_t r = isqrt_u32(tR); // 0..100

  lvlL = (uint8_t)l;
  lvlR = (uint8_t)r;

  if (lvlL > peakL) peakL = lvlL;
  if (lvlR > peakR) peakR = lvlR;

  if (isr) portEXIT_CRITICAL_ISR(&mux);
  else     portEXIT_CRITICAL(&mux);
}

void vu_init()
{
  portENTER_CRITICAL(&mux);
  envL = envR = 0;
  lvlL = lvlR = 0;
  peakL = peakR = 0;
  agcGainX100 = 100;
  portEXIT_CRITICAL(&mux);

  lastPeakDecay = millis();
}

// ISR-biztos: nincs millis(), nincs Serial, nincs heap
void IRAM_ATTR vu_feedStereoISR(const int16_t* s, size_t n)
{
  feed_core(s, n, true);
}

// Nem-ISR változat (ha bárhol máshol kell)
void vu_feedStereo(const int16_t* s, size_t n)
{
  feed_core(s, n, false);
}

// Peak lecsengés UI oldalon
static inline void decay_peaks_if_needed()
{
  uint32_t now = millis();
  if (now - lastPeakDecay >= 90) {
    portENTER_CRITICAL(&mux);
    if (peakL > 0) peakL--;
    if (peakR > 0) peakR--;
    portEXIT_CRITICAL(&mux);
    lastPeakDecay = now;
  }

#if VU_AGC_ENABLE
  // AGC frissítés ritkábban, hogy ne "pumpáljon"
  static uint32_t lastAgc = 0;
  static uint16_t avg = 0; // 0..100 (EMA)
  if (now - lastAgc >= 160) {
    uint8_t l, r;
    uint16_t g;
    portENTER_CRITICAL(&mux);
    l = lvlL;
    r = lvlR;
    g = agcGainX100;
    portEXIT_CRITICAL(&mux);

    uint16_t cur = (l > r) ? l : r;
    // Exponenciális átlagolás: avg = 0.75*avg + 0.25*cur
    avg = (uint16_t)((avg * 3U + cur) / 4U);

    int16_t err = (int16_t)VU_AGC_TARGET - (int16_t)avg;
    // kis holtsáv
    if (err > 2) {
      uint16_t step = (uint16_t)min((int)VU_AGC_STEP_UP, (int)err);
      g = (uint16_t)min<uint32_t>((uint32_t)VU_AGC_MAX_X100, (uint32_t)g + step);
    } else if (err < -2) {
      uint16_t step = (uint16_t)min((int)VU_AGC_STEP_DN, (int)(-err));
      if (g > step) g = (uint16_t)(g - step);
      else g = VU_AGC_MIN_X100;
      if (g < VU_AGC_MIN_X100) g = VU_AGC_MIN_X100;
    }

    portENTER_CRITICAL(&mux);
    agcGainX100 = g;
    portEXIT_CRITICAL(&mux);

    lastAgc = now;
  }
#endif
}

uint8_t vu_getL()
{
  decay_peaks_if_needed();
  portENTER_CRITICAL(&mux);
  uint8_t v = (uint8_t)lvlL;
  portEXIT_CRITICAL(&mux);
  return v;
}

uint8_t vu_getR()
{
  decay_peaks_if_needed();
  portENTER_CRITICAL(&mux);
  uint8_t v = (uint8_t)lvlR;
  portEXIT_CRITICAL(&mux);
  return v;
}

uint8_t vu_getPeakL()
{
  decay_peaks_if_needed();
  portENTER_CRITICAL(&mux);
  uint8_t v = (uint8_t)peakL;
  portEXIT_CRITICAL(&mux);
  return v;
}

uint8_t vu_getPeakR()
{
  decay_peaks_if_needed();
  portENTER_CRITICAL(&mux);
  uint8_t v = (uint8_t)peakR;
  portEXIT_CRITICAL(&mux);
  return v;
}