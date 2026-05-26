#ifndef AETHER_FB_H
#define AETHER_FB_H

#include <System/Types.h>

#define FB_CTRL_GET_INFO 0
#define FB_CTRL_MAP 1

typedef struct {
	U32 Width;
	U32 Height;
	U32 Pitch;
	U32 Bpp;
} FbInfo;

typedef struct {
	volatile U32 *Base;
	U32 Width;
	U32 Height;
	U32 Pitch;
	U32 Bpp;
	U64 BufSize;
} Framebuffer;

int FbInit(Framebuffer *Fb);

#endif
