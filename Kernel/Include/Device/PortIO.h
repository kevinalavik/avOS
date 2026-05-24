#ifndef DEVICE_PORT_IO_H
#define DEVICE_PORT_IO_H

#include <stdint.h>

uint8_t PortIORead8(uint16_t Port);
uint16_t PortIORead16(uint16_t Port);
uint32_t PortIORead32(uint16_t Port);
void PortIOWrite8(uint16_t Port, uint8_t Value);
void PortIOWrite16(uint16_t Port, uint16_t Value);
void PortIOWrite32(uint16_t Port, uint32_t Value);

void PortIOWait(void);

#endif
