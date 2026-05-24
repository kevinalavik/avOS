#ifndef DEVICE_SERIAL_H
#define DEVICE_SERIAL_H

#include <stddef.h>
#include <stdint.h>

#define SerialCom1 0x3F8
#define SerialCom2 0x2F8
#define SerialCom3 0x3E8
#define SerialCom4 0x2E8

void SerialInit(uint16_t Port, uint32_t Baud);
void SerialPutc(char Character);
void SerialWrite(const char *Buffer, size_t Length);
void SerialPrint(const char *String);

#endif
