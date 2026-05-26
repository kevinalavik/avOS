#ifndef AETHER_MOUSE_H
#define AETHER_MOUSE_H

#include <System/Types.h>

typedef struct {
	U32 X;
	U32 Y;
	S32 Dx;
	S32 Dy;
	U8 Buttons;
} MousePacket;

#endif
