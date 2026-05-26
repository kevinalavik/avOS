#include <System/Pulse.h>
#include <System/Syscall.h>

S64 PulseSend(ProcessId TargetId, U32 Type, const void *Payload, U32 Size)
{
	return (S64)Syscall4(SyscallPulseSend, TargetId, Type, (U64)Payload, Size);
}

S64 PulseReceive(PulseMessage *Message)
{
	return (S64)Syscall1(SyscallPulseReceive, (U64)Message);
}
