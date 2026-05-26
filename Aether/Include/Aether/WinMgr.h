#ifndef AETHER_WINMGR_H
#define AETHER_WINMGR_H

#include <System/Types.h>
#include <Adk/AdkObjectManager.h>
#include <Adk/AdkWindow.h>

#define MAX_WINDOWS 16u

typedef struct {
	AdkObjectManager *ObjectManager;
	AdkWindow *Windows[MAX_WINDOWS];
	/* For Raw windows whose Buffer is a shared-memory mapping, store the
	 * shm ID so we can unmap rather than free on destroy.  0 means the
	 * window is not a Raw/shm window. */
	U64 WindowShmIds[MAX_WINDOWS];
	U32 ScreenWidth;
	U32 ScreenHeight;
	int WindowCount;
	int FocusedIndex;
	int DragIndex;
	S32 DragOffX;
	S32 DragOffY;
	/* When True, the window at DragIndex is rendered as a wireframe outline
	 * instead of its full contents, keeping the compositor fast during drag. */
	Bool WireframeDrag;
} AetherWinMgr;

void AetherWinMgrInit(AetherWinMgr *Wm, AdkObjectManager *ObjectManager,
					  U32 ScreenWidth, U32 ScreenHeight);
int AetherWinMgrCreateWindow(AetherWinMgr *Wm, S32 X, S32 Y, U32 Width,
							 U32 Height, const char *Title, U32 FillColor);
void AetherWinMgrDestroyWindow(AetherWinMgr *Wm, int Index);
void AetherWinMgrBringToFront(AetherWinMgr *Wm, int Index);
void AetherWinMgrBeginDrag(AetherWinMgr *Wm, int Index, S32 MouseX, S32 MouseY);
void AetherWinMgrUpdateDrag(AetherWinMgr *Wm, S32 MouseX, S32 MouseY);
void AetherWinMgrEndDrag(AetherWinMgr *Wm);
void AetherWinMgrSetWireframeDrag(AetherWinMgr *Wm, Bool Enable);
/* Record the shared-memory ID for a Raw window so destroy can unmap it. */
void AetherWinMgrSetWindowShmId(AetherWinMgr *Wm, int Index, U64 ShmId);
int AetherWinMgrHitTest(const AetherWinMgr *Wm, S32 AbsX, S32 AbsY,
						AdkWindowPart *Part, S32 *RelX, S32 *RelY);
int AetherWinMgrFindWindow(const AetherWinMgr *Wm, S32 AbsX, S32 AbsY,
						   S32 *RelX, S32 *RelY);

#endif
