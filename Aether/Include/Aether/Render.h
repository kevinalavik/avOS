#ifndef AETHER_RENDER_H
#define AETHER_RENDER_H

#include <System/Types.h>
#include <Aether/WinMgr.h>
#include <Aether/Cursor.h>
#include <Aether/Background.h>
#include <Adk/AdkFont.h>

typedef struct {
	volatile U32 *FbBase;
	U32 FbPitch;
	U32 FbWidth;
	U32 FbHeight;
	U32 *SceneBuf;
	U32 *BackBuf;
	U32 *CleanBuf;
	Size BufSize;
	AetherBackground *Background;
	AetherCursor *Cursor;
	AetherWinMgr *WinMgr;
	char OverlayText[32];
	S32 PrevCursorX;
	S32 PrevCursorY;
	Bool HasPrevCursor;
} AetherRenderer;

void AetherRenderFull(AetherRenderer *Rdr);
void AetherRenderRegion(AetherRenderer *Rdr, const AdkRect *Rect);
void AetherRenderWindowClient(AetherRenderer *Rdr, const AdkWindow *Wnd);
void AetherRenderCursor(AetherRenderer *Rdr);

#endif
