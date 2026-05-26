#ifndef DRIVERS_INPUT_MOUSE_H
#define DRIVERS_INPUT_MOUSE_H

#include <Drivers/Device.h>
#include <stdint.h>

#define MOUSE_CTRL_SET_BOUNDS 0
#define MOUSE_CTRL_SET_POS 1
#define MOUSE_CTRL_GET_COUNT 2

typedef struct MousePacket {
	uint32_t X;
	uint32_t Y;
	int32_t Dx;
	int32_t Dy;
	uint8_t Buttons;
} MousePacket;

extern Driver MouseDriver;
extern Device MouseDevice;

#endif
