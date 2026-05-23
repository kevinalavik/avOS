#ifndef DEVICE_FRAMEBUFFER_H
#define DEVICE_FRAMEBUFFER_H

#include <Boot/BootInfo.h>
#include <stdbool.h>
#include <stdint.h>

bool FramebufferInit(const BootFramebuffer *Fb);
bool FramebufferReady(void);

uint32_t FramebufferWidth(void);
uint32_t FramebufferHeight(void);

uint32_t FramebufferColor(uint8_t R, uint8_t G, uint8_t B);
void FramebufferPutPixel(uint32_t X, uint32_t Y, uint32_t Color);
void FramebufferClear(uint32_t Color);

#endif