#include <System/Device.h>
#include <System/Syscall.h>

Size DeviceRead(const char *Name, void *Buffer, Size Length)
{
	return Syscall3(SyscallDeviceRead, (U64)Name, (U64)Buffer, Length);
}

Size DeviceWrite(const char *Name, const void *Buffer, Size Length)
{
	return Syscall3(SyscallDeviceWrite, (U64)Name, (U64)Buffer, Length);
}

Size DeviceControl(const char *Name, U64 Command, void *Argument)
{
	return Syscall3(SyscallDeviceControl, (U64)Name, Command, (U64)Argument);
}
