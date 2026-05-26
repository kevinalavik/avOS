#include <Aether/WinMgr.h>
#include <Adk/AdkObjectManager.h>
#include <Adk/AdkWindow.h>
#include <System/Memory.h>
#include <Lib/String.h>

static void AetherWinMgrSyncActiveState(AetherWinMgr *Wm)
{
	if (!Wm)
		return;

	for (int i = 0; i < Wm->WindowCount; i++) {
		if (!Wm->Windows[i])
			continue;

		AdkWindowSetActive(Wm->Windows[i], i == Wm->FocusedIndex);
	}
}

void AetherWinMgrInit(AetherWinMgr *Wm, AdkObjectManager *ObjectManager,
					  U32 ScreenWidth, U32 ScreenHeight)
{
	if (!Wm)
		return;

	Wm->ObjectManager = ObjectManager;
	Wm->ScreenWidth = ScreenWidth;
	Wm->ScreenHeight = ScreenHeight;
	Wm->WindowCount = 0;
	Wm->FocusedIndex = -1;
	Wm->DragIndex = -1;
	Wm->DragOffX = 0;
	Wm->DragOffY = 0;
	Wm->WireframeDrag = False;

	for (U32 i = 0; i < MAX_WINDOWS; i++) {
		Wm->Windows[i] = 0;
		Wm->WindowShmIds[i] = 0;
	}
}

int AetherWinMgrCreateWindow(AetherWinMgr *Wm, S32 X, S32 Y, U32 Width,
							 U32 Height, const char *Title, U32 FillColor)
{
	if (!Wm || Wm->WindowCount >= (int)MAX_WINDOWS)
		return -1;

	AdkWindow *Wnd = AdkWindowCreate(Wm->ObjectManager, X, Y, Width, Height);
	if (!Wnd)
		return -1;

	AdkWindowSetTitle(Wnd, Title);
	AdkWindowFillClient(Wnd, FillColor);

	/* Find first empty slot */
	int Index = 0;
	while (Index < Wm->WindowCount && Wm->Windows[Index])
		Index++;

	if (Index >= (int)MAX_WINDOWS) {
		AdkWindowDestroy(Wnd);
		return -1;
	}

	Wm->Windows[Index] = Wnd;
	Wm->WindowShmIds[Index] = 0;
	if (Index >= Wm->WindowCount)
		Wm->WindowCount = Index + 1;

	Wm->FocusedIndex = Index;
	AetherWinMgrSyncActiveState(Wm);

	return Index;
}

void AetherWinMgrSetWindowShmId(AetherWinMgr *Wm, int Index, U64 ShmId)
{
	if (!Wm || Index < 0 || Index >= (int)MAX_WINDOWS)
		return;
	Wm->WindowShmIds[Index] = ShmId;
}

void AetherWinMgrDestroyWindow(AetherWinMgr *Wm, int Index)
{
	if (!Wm || Index < 0 || Index >= Wm->WindowCount || !Wm->Windows[Index])
		return;

	/* For Raw windows whose buffer is a shared-memory mapping, we must unmap
     * and destroy the shm object rather than letting AdkWindowDestroy call
     * free() on the mapping pointer — that would cause a GPF. */
	U64 ShmId = Wm->WindowShmIds[Index];
	if (ShmId != 0) {
		AdkWindow *Wnd = Wm->Windows[Index];
		if (Wnd->Buffer) {
			SharedMemUnmap(Wnd->Buffer);
			Wnd->Buffer = 0; /* prevent double-free in AdkWindowDestroy */
			Wnd->Surface.Pixels = 0; /* keep the surface consistent */
		}
		SharedMemDestroy(ShmId);
		Wm->WindowShmIds[Index] = 0;
	}

	AdkWindowDestroy(Wm->Windows[Index]);
	Wm->Windows[Index] = 0;

	/* If we destroyed the focused window, reset focus */
	if (Wm->FocusedIndex == Index)
		Wm->FocusedIndex = -1;
	else if (Wm->FocusedIndex > Index)
		Wm->FocusedIndex--;

	/* If we destroyed the window being dragged, reset drag */
	if (Wm->DragIndex == Index) {
		Wm->DragIndex = -1;
		Wm->DragOffX = 0;
		Wm->DragOffY = 0;
		Wm->WireframeDrag = False;
	}

	/* Compact the window list */
	for (int i = Index; i < Wm->WindowCount - 1; i++) {
		Wm->Windows[i] = Wm->Windows[i + 1];
		Wm->WindowShmIds[i] = Wm->WindowShmIds[i + 1];
	}
	if (Wm->WindowCount > 0) {
		Wm->WindowCount--;
		Wm->Windows[Wm->WindowCount] = 0;
		Wm->WindowShmIds[Wm->WindowCount] = 0;
	}

	if (Wm->FocusedIndex < 0 && Wm->WindowCount > 0)
		Wm->FocusedIndex = Wm->WindowCount - 1;

	AetherWinMgrSyncActiveState(Wm);
}

void AetherWinMgrBringToFront(AetherWinMgr *Wm, int Index)
{
	if (!Wm || Index < 0 || Index >= Wm->WindowCount || !Wm->Windows[Index])
		return;

	if (Index == Wm->WindowCount - 1) {
		Wm->FocusedIndex = Index;
		AetherWinMgrSyncActiveState(Wm);
		return;
	}

	/* Remove the window from its current position */
	AdkWindow *Wnd = Wm->Windows[Index];
	U64 ShmId = Wm->WindowShmIds[Index];
	for (int i = Index; i < Wm->WindowCount - 1; i++) {
		Wm->Windows[i] = Wm->Windows[i + 1];
		Wm->WindowShmIds[i] = Wm->WindowShmIds[i + 1];
	}
	Wm->Windows[Wm->WindowCount - 1] = Wnd;
	Wm->WindowShmIds[Wm->WindowCount - 1] = ShmId;

	/* Update focused index */
	if (Wm->FocusedIndex == Index)
		Wm->FocusedIndex = Wm->WindowCount - 1;
	else if (Wm->FocusedIndex > Index)
		Wm->FocusedIndex--;

	/* Update drag index */
	if (Wm->DragIndex == Index)
		Wm->DragIndex = Wm->WindowCount - 1;
	else if (Wm->DragIndex > Index)
		Wm->DragIndex--;

	Wm->FocusedIndex = Wm->WindowCount - 1;
	AetherWinMgrSyncActiveState(Wm);
}

void AetherWinMgrBeginDrag(AetherWinMgr *Wm, int Index, S32 MouseX, S32 MouseY)
{
	if (!Wm || Index < 0 || Index >= Wm->WindowCount || !Wm->Windows[Index])
		return;

	Wm->DragIndex = Index;
	Wm->WireframeDrag = True;
	AdkWindow *Wnd = Wm->Windows[Index];
	Wm->DragOffX = MouseX - Wnd->X;
	Wm->DragOffY = MouseY - Wnd->Y;
}

void AetherWinMgrUpdateDrag(AetherWinMgr *Wm, S32 MouseX, S32 MouseY)
{
	if (!Wm || Wm->DragIndex < 0)
		return;

	AdkWindow *Wnd = Wm->Windows[Wm->DragIndex];
	if (!Wnd)
		return;

	Wnd->X = MouseX - Wm->DragOffX;
	Wnd->Y = MouseY - Wm->DragOffY;
}

void AetherWinMgrSetWireframeDrag(AetherWinMgr *Wm, Bool Enable)
{
	if (!Wm)
		return;
	Wm->WireframeDrag = Enable;
}

void AetherWinMgrEndDrag(AetherWinMgr *Wm)
{
	if (!Wm)
		return;

	Wm->DragIndex = -1;
	Wm->WireframeDrag = False;
	Wm->DragOffX = 0;
	Wm->DragOffY = 0;
}

int AetherWinMgrHitTest(const AetherWinMgr *Wm, S32 AbsX, S32 AbsY,
						AdkWindowPart *Part, S32 *RelX, S32 *RelY)
{
	if (!Wm)
		return -1;

	/* Check windows from front to back (highest index to lowest) */
	for (int i = Wm->WindowCount - 1; i >= 0; i--) {
		AdkWindow *Wnd = Wm->Windows[i];
		if (!Wnd || !Wnd->Visible)
			continue;

		S32 wndRelX = AbsX - Wnd->X;
		S32 wndRelY = AbsY - Wnd->Y;

		if (wndRelX < 0 || wndRelX >= (S32)Wnd->Width || wndRelY < 0 ||
			wndRelY >= (S32)Wnd->Height)
			continue;

		AdkWindowPart part = AdkWindowHitTest(Wnd, AbsX, AbsY);
		if (part != AdkWindowPartNone) {
			if (Part)
				*Part = part;
			if (RelX)
				*RelX = wndRelX;
			if (RelY)
				*RelY = wndRelY;
			return i;
		}
	}

	return -1;
}

int AetherWinMgrFindWindow(const AetherWinMgr *Wm, S32 AbsX, S32 AbsY,
						   S32 *RelX, S32 *RelY)
{
	if (!Wm)
		return -1;

	/* Check windows from front to back (highest index to lowest) */
	for (int i = Wm->WindowCount - 1; i >= 0; i--) {
		AdkWindow *Wnd = Wm->Windows[i];
		if (!Wnd || !Wnd->Visible)
			continue;

		S32 wndRelX = AbsX - Wnd->X;
		S32 wndRelY = AbsY - Wnd->Y;

		if (wndRelX < 0 || wndRelX >= (S32)Wnd->Width || wndRelY < 0 ||
			wndRelY >= (S32)Wnd->Height)
			continue;

		if (RelX)
			*RelX = wndRelX;
		if (RelY)
			*RelY = wndRelY;
		return i;
	}

	return -1;
}
