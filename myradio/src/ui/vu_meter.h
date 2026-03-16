#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// VU érzékenység: 100 = 1.0x, 200 = 2.0x, 300 = 3.0x.
// Ha túl "nyugtalan", vedd le 160-180 környékére.
#ifndef VU_GAIN_X100
#define VU_GAIN_X100 200
#endif

// --- Automatikus gain (csak a kijelző VU-hoz, NEM a hangerőhöz) ---
// Cél: halk adóknál feljebb húzza a VU érzékenységet, hangosnál visszaveszi,
// hogy a kijelzés használható tartományban mozogjon.
#ifndef VU_AGC_ENABLE
#define VU_AGC_ENABLE 1
#endif

// 0..100 cél szint (kb. ennyit célozzon a sáv)
#ifndef VU_AGC_TARGET
#define VU_AGC_TARGET 60
#endif

// AGC gain határok (100 = 1.0x). Ez a VU_GAIN_X100 fölé szorzódik.
#ifndef VU_AGC_MIN_X100
#define VU_AGC_MIN_X100 80
#endif
#ifndef VU_AGC_MAX_X100
#define VU_AGC_MAX_X100 450
#endif

// Mennyire gyorsan reagáljon (lépés / frissítés). Hangosnál gyorsabb visszavétel.
#ifndef VU_AGC_STEP_UP
#define VU_AGC_STEP_UP 2
#endif
#ifndef VU_AGC_STEP_DN
#define VU_AGC_STEP_DN 6
#endif


void vu_init();

// Ezt hívd ISR-ből (audio callback)
void IRAM_ATTR vu_feedStereoISR(const int16_t* s, size_t n);

// Ha valahol nem-ISR környezetből etetnéd (opcionális)
void vu_feedStereo(const int16_t* s, size_t n);

// UI oldali olvasás (nem ISR)
uint8_t vu_getL();
uint8_t vu_getR();
uint8_t vu_getPeakL();
uint8_t vu_getPeakR();