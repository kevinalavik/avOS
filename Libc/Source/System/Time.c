#include <System/Time.h>
#include <System/Syscall.h>

U64 TimeTicks(void)
{
	return Syscall0(SyscallTimeGetTicks);
}

U64 TimeFrequency(void)
{
	return Syscall0(SyscallTimeGetFrequency);
}
