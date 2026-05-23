#ifndef LIB_FRAMEBUFFER_H
#define LIB_FRAMEBUFFER_H

#include <stdint.h>

struct BootInfo;

extern uint32_t VesaModeNumber;
extern uint32_t VesaFramebufferAddress;
extern uint32_t VesaWidth;
extern uint32_t VesaHeight;
extern uint32_t VesaPitch;
extern uint32_t VesaBpp;

void FramebufferInit(struct BootInfo *Info);

#endif
