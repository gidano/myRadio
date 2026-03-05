#include "input_rotary.h"

void input_rotary_init(int pinA, int pinB, int pinBtn, void (*isr)(), volatile uint8_t* encHist) {
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  pinMode(pinBtn, INPUT_PULLUP);

  // Encoder ISR init history
  *encHist = (uint8_t)(((digitalRead(pinA) ? 1 : 0) << 1) | (digitalRead(pinB) ? 1 : 0));

  attachInterrupt(digitalPinToInterrupt(pinA), isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pinB), isr, CHANGE);
}

void input_rotary_apply(InputRotaryCtx& ctx) {
  static int32_t encAcc = 0;

  int32_t d;
  noInterrupts();
  d = *(ctx.encDelta);
  *(ctx.encDelta) = 0;
  interrupts();

  if (d == 0) return;

  encAcc += d;

  int32_t steps = 0;
  while (encAcc >= ctx.pulsesPerStep) { steps++; encAcc -= ctx.pulsesPerStep; }
  while (encAcc <= -ctx.pulsesPerStep) { steps--; encAcc += ctx.pulsesPerStep; }
  if (steps == 0) return;

  if (*(ctx.mode) == ctx.modePlay) {
    int newVol = *(ctx.volume) + (int)steps;
    if (newVol < ctx.volMin) newVol = ctx.volMin;
    if (newVol > ctx.volMax) newVol = ctx.volMax;

    if (newVol != *(ctx.volume)) {
      *(ctx.volume) = newVol;

      if (ctx.sendVolume) ctx.sendVolume((uint8_t)newVol);
      if (ctx.onVolumeChanged) ctx.onVolumeChanged();
    }
  } else {
    if (*(ctx.stationCount) > 0) {
      if (steps > 0) *(ctx.menuIndex) = (*(ctx.menuIndex) + 1) % (*(ctx.stationCount));
      else           *(ctx.menuIndex) = (*(ctx.menuIndex) - 1 + *(ctx.stationCount)) % (*(ctx.stationCount));

      if (ctx.onMenuChanged) ctx.onMenuChanged();
    }
  }
}