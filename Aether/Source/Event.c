#include <Aether/Event.h>

void EventQueueInit(EventQueue *Eq)
{
	Eq->Head = 0;
	Eq->Tail = 0;
}

Bool EventQueuePush(EventQueue *Eq, const Event *Evt)
{
	U32 Next = (Eq->Tail + 1) % EVENT_QUEUE_SIZE;
	if (Next == Eq->Head)
		return False;
	Eq->Events[Eq->Tail] = *Evt;
	Eq->Tail = Next;
	return True;
}

Bool EventQueuePop(EventQueue *Eq, Event *Evt)
{
	if (Eq->Head == Eq->Tail)
		return False;
	*Evt = Eq->Events[Eq->Head];
	Eq->Head = (Eq->Head + 1) % EVENT_QUEUE_SIZE;
	return True;
}
