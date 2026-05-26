#ifndef SYSTEM_PULSE_H
#define SYSTEM_PULSE_H

#include <System/Types.h>

#define PULSE_PAYLOAD_MAX 256u

typedef struct {
	ProcessId SenderId;
	U32 Type;
	U32 Size;
	U8 Payload[PULSE_PAYLOAD_MAX];
} PulseMessage;

S64 PulseSend(ProcessId TargetId, U32 Type, const void *Payload, U32 Size);
S64 PulseReceive(PulseMessage *Message);

#endif
