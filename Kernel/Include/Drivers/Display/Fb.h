#ifndef DRIVERS_DISPLAY_FB_H
#define DRIVERS_DISPLAY_FB_H

#include <Drivers/Device.h>
#include <stdint.h>

#define FB_CTRL_GET_INFO 0
#define FB_CTRL_MAP 1

typedef struct FbInfo {
	uint32_t Width;
	uint32_t Height;
	uint32_t Pitch;
	uint32_t Bpp;
} FbInfo;

extern Driver FbDriver;
extern Device FbDevice;

#endif
