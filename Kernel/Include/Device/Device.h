#ifndef DEVICE_DEVICE_H
#define DEVICE_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Device Device;
typedef struct Driver Driver;

struct Driver {
	const char *Name;
	void (*Bind)(Device *Dev);
	void (*Remove)(Device *Dev);
	int64_t (*Read)(Device *Dev, void *Buf, uint64_t Size);
	int64_t (*Write)(Device *Dev, const void *Buf, uint64_t Size);
	int64_t (*Control)(Device *Dev, uint64_t Cmd, void *Arg);
};

struct Device {
	const char *Name;
	Driver *Drv;
	void *Data;
};

void DriverRegister(Driver *Drv);
bool DeviceRegister(Device *Dev);
void DeviceBind(Device *Dev, Driver *Drv);
Device *DeviceGet(const char *Name);
void DeviceEnumerate(void (*Fn)(Device *D, void *Ctx), void *Ctx);

static inline int64_t DeviceRead(Device *D, void *B, uint64_t S)
{
	return D && D->Drv && D->Drv->Read
		? D->Drv->Read(D, B, S) : -1;
}

static inline int64_t DeviceWrite(Device *D, const void *B, uint64_t S)
{
	return D && D->Drv && D->Drv->Write
		? D->Drv->Write(D, B, S) : -1;
}

static inline int64_t DeviceControl(Device *D, uint64_t C, void *A)
{
	return D && D->Drv && D->Drv->Control
		? D->Drv->Control(D, C, A) : -1;
}

#endif
