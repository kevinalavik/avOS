#include <Adk/AdkObjectManager.h>
#include <System/Memory.h>

void AdkObjectManagerInit(AdkObjectManager *Mgr)
{
	Mgr->Head = 0;
	Mgr->Count = 0;
}

void *AdkObjectAlloc(AdkObjectManager *Mgr, Size Bytes)
{
	AdkObjectNode *Node = (AdkObjectNode *)MemoryAlloc(sizeof(AdkObjectNode));
	if (!Node)
		return 0;

	void *Ptr = MemoryAlloc(Bytes);
	if (!Ptr) {
		MemoryFree(Node);
		return 0;
	}

	Node->Ptr = Ptr;
	Node->Next = Mgr->Head;
	Mgr->Head = Node;
	Mgr->Count++;
	return Ptr;
}

void AdkObjectFree(AdkObjectManager *Mgr, void *Ptr)
{
	if (!Mgr || !Ptr)
		return;

	AdkObjectNode **Prev = &Mgr->Head;
	AdkObjectNode *Cur = Mgr->Head;

	while (Cur) {
		if (Cur->Ptr == Ptr) {
			*Prev = Cur->Next;
			MemoryFree(Cur->Ptr);
			MemoryFree(Cur);
			Mgr->Count--;
			return;
		}
		Prev = &Cur->Next;
		Cur = Cur->Next;
	}
}

void AdkObjectManagerFreeAll(AdkObjectManager *Mgr)
{
	if (!Mgr)
		return;

	AdkObjectNode *Cur = Mgr->Head;
	while (Cur) {
		AdkObjectNode *Next = Cur->Next;
		MemoryFree(Cur->Ptr);
		MemoryFree(Cur);
		Cur = Next;
	}

	Mgr->Head = 0;
	Mgr->Count = 0;
}
