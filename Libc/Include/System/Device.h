#ifndef SYSTEM_DEVICE_H
#define SYSTEM_DEVICE_H

#include <System/Types.h>

Size DeviceRead(const char *Name, void *Buffer, Size Length);
Size DeviceWrite(const char *Name, const void *Buffer, Size Length);
Size DeviceControl(const char *Name, U64 Command, void *Argument);

#endif
