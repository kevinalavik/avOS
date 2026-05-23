#ifndef E_PORT_IO_H
#define E_PORT_IO_H

#include <stdint.h>

typedef enum PortIOWidth {
	PortIOWidth8 = 1,
	PortIOWidth16 = 2,
	PortIOWidth32 = 4,
} PortIOWidth;

uint32_t PortIORead(PortIOWidth eWidth, uint16_t Port);
void PortIOWrite(PortIOWidth eWidth, uint16_t Port, uint32_t Data);

#endif
