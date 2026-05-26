#ifndef AETHER_EVENT_H
#define AETHER_EVENT_H

#include <System/Types.h>

#define EVENT_QUEUE_SIZE 64u

typedef enum {
	EVENT_NONE,
	EVENT_MOUSE_MOVE,
	EVENT_MOUSE_DOWN,
	EVENT_MOUSE_UP,
	EVENT_MOUSE_HOLD,
	EVENT_KEY_DOWN,
	EVENT_KEY_UP,
	EVENT_QUIT,
} EventType;

typedef struct {
	EventType Type;
	S32 X;
	S32 Y;
	U8 Buttons;
	char Char;
	U16 Key;
	U8 Modifiers;
} Event;

typedef struct {
	Event Events[EVENT_QUEUE_SIZE];
	U32 Head;
	U32 Tail;
} EventQueue;

void EventQueueInit(EventQueue *Eq);
Bool EventQueuePush(EventQueue *Eq, const Event *Evt);
Bool EventQueuePop(EventQueue *Eq, Event *Evt);

#endif
