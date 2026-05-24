#include <Device/Device.h>

#define DriverMaxCount 16
#define DeviceMaxCount 16

static Driver *DriverTable[DriverMaxCount];
static uint64_t DriverCount;
static Device *DeviceTable[DeviceMaxCount];
static uint64_t DeviceCount;

void DriverRegister(Driver *Drv)
{
	if (Drv && DriverCount < DriverMaxCount)
		DriverTable[DriverCount++] = Drv;
}

bool DeviceRegister(Device *Dev)
{
	if (!Dev || DeviceCount >= DeviceMaxCount)
		return false;

	DeviceTable[DeviceCount++] = Dev;
	return true;
}

void DeviceBind(Device *Dev, Driver *Drv)
{
	if (!Dev || !Drv)
		return;

	Dev->Drv = Drv;
	if (Drv->Bind)
		Drv->Bind(Dev);
}

Device *DeviceGet(const char *Name)
{
	for (uint64_t i = 0; i < DeviceCount; ++i) {
		const char *a = DeviceTable[i]->Name;
		const char *b = Name;
		while (*a && *b && *a == *b) {
			++a;
			++b;
		}
		if (*a == *b)
			return DeviceTable[i];
	}
	return 0;
}

void DeviceEnumerate(void (*Fn)(Device *D, void *Ctx), void *Ctx)
{
	if (!Fn)
		return;

	for (uint64_t i = 0; i < DeviceCount; ++i)
		Fn(DeviceTable[i], Ctx);
}
