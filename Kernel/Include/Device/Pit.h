#ifndef DEVICE_PIT_H
#define DEVICE_PIT_H

#include <stdint.h>

void PitInit(uint32_t Frequency);
uint64_t PitGetTicks(void);

/* Busy-wait delay using PIT channel 0 counter (does not require IRQs). */
void PitDelayUs(uint64_t Microseconds);
void PitDelayMs(uint64_t Milliseconds);

uint32_t PitGetFrequencyHz(void);
int PitIsInitialized(void);

#endif
