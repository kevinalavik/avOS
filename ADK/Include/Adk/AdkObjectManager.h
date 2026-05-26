#ifndef ADK_OBJECT_MANAGER_H
#define ADK_OBJECT_MANAGER_H

#include <System/Types.h>

typedef struct AdkObjectNode {
	void *Ptr;
	struct AdkObjectNode *Next;
} AdkObjectNode;

typedef struct {
	AdkObjectNode *Head;
	Size Count;
} AdkObjectManager;

void AdkObjectManagerInit(AdkObjectManager *Mgr);
void *AdkObjectAlloc(AdkObjectManager *Mgr, Size Bytes);
void AdkObjectFree(AdkObjectManager *Mgr, void *Ptr);
void AdkObjectManagerFreeAll(AdkObjectManager *Mgr);

#endif
