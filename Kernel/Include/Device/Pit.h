#ifndef DEVICE_PIT_H
#define DEVICE_PIT_H

#include <stdint.h>

void PitInit(uint32_t Frequency);
uint64_t PitGetTicks(void);

#endif
