#include <Aether/Render.h>

#define AETHER_CURSOR_FALLBACK_SIZE 5
#define WIREFRAME_COLOR 0xFFFFFFFFu
#define WIREFRAME_THICKNESS 2

static void MemCopy(void *Dst, const void *Src, Size N)
{
	U64 *D = (U64 *)Dst;
	const U64 *S = (const U64 *)Src;
	Size NWords = N / 8;
	for (Size i = 0; i < NWords; i++)
		D[i] = S[i];
	for (Size i = NWords * 8; i < N; i++)
		((Byte *)Dst)[i] = ((const Byte *)Src)[i];
}

static AdkRect CursorRect(const AetherRenderer *Rdr, S32 X, S32 Y)
{
	AdkRect Rect;
	U32 Width = AETHER_CURSOR_FALLBACK_SIZE;
	U32 Height = AETHER_CURSOR_FALLBACK_SIZE;
	S32 DrawX = X;
	S32 DrawY = Y;

	if (Rdr->Cursor && Rdr->Cursor->Image) {
		Width = Rdr->Cursor->Image->Width;
		Height = Rdr->Cursor->Image->Height;
		DrawX -= Rdr->Cursor->HotspotX;
		DrawY -= Rdr->Cursor->HotspotY;
	}

	Rect.X = DrawX;
	Rect.Y = DrawY;
	Rect.Width = Width;
	Rect.Height = Height;
	return Rect;
}

static Bool ClipRect(const AetherRenderer *Rdr, const AdkRect *In, AdkRect *Out)
{
	S32 Left = In->X;
	S32 Top = In->Y;
	S32 Right = In->X + (S32)In->Width;
	S32 Bottom = In->Y + (S32)In->Height;

	if (Left < 0)
		Left = 0;
	if (Top < 0)
		Top = 0;
	if (Right > (S32)Rdr->FbWidth)
		Right = (S32)Rdr->FbWidth;
	if (Bottom > (S32)Rdr->FbHeight)
		Bottom = (S32)Rdr->FbHeight;
	if (Left >= Right || Top >= Bottom)
		return False;

	Out->X = Left;
	Out->Y = Top;
	Out->Width = (U32)(Right - Left);
	Out->Height = (U32)(Bottom - Top);
	return True;
}

static Bool RectIntersects(const AdkRect *A, const AdkRect *B)
{
	S32 ARight = A->X + (S32)A->Width;
	S32 ABottom = A->Y + (S32)A->Height;
	S32 BRight = B->X + (S32)B->Width;
	S32 BBottom = B->Y + (S32)B->Height;

	return A->X < BRight && ARight > B->X && A->Y < BBottom && ABottom > B->Y;
}

static Bool OverlayRect(const AetherRenderer *Rdr, AdkRect *Rect)
{
	if (Rdr->OverlayText[0] == '\0')
		return False;

	Rect->X = 8;
	Rect->Y = 8;
	Rect->Width = 88;
	Rect->Height = 18;
	return True;
}

static void CopyRectToFramebuffer(const AetherRenderer *Rdr,
								  const AdkRect *Rect)
{
	AdkRect Clipped;
	U32 Pitch = Rdr->FbPitch / 4;

	if (!ClipRect(Rdr, Rect, &Clipped))
		return;

	for (U32 Y = 0; Y < Clipped.Height; Y++) {
		U32 Row = (U32)(Clipped.Y + (S32)Y) * Pitch + (U32)Clipped.X;
		MemCopy((void *)((U32 *)Rdr->FbBase + Row), Rdr->SceneBuf + Row,
				(Size)Clipped.Width * sizeof(U32));
	}
}

static void CopyRectFromCleanToScene(AetherRenderer *Rdr, const AdkRect *Rect)
{
	AdkRect Clipped;
	U32 Pitch = Rdr->FbPitch / 4;

	if (!ClipRect(Rdr, Rect, &Clipped))
		return;

	for (U32 Y = 0; Y < Clipped.Height; Y++) {
		U32 Row = (U32)(Clipped.Y + (S32)Y) * Pitch + (U32)Clipped.X;
		MemCopy(Rdr->SceneBuf + Row, Rdr->CleanBuf + Row,
				(Size)Clipped.Width * sizeof(U32));
	}
}

static void BlitSurfaceOpaqueToScene(AetherRenderer *Rdr,
									 const AdkSurface *Surface, S32 DstX,
									 S32 DstY, const AdkRect *Clip)
{
	S32 Left;
	S32 Top;
	S32 Right;
	S32 Bottom;
	U32 CopyWidth;
	U32 SrcX;
	U32 SrcY;
	U32 Pitch;

	if (!Surface || !Surface->Pixels)
		return;

	Left = DstX;
	Top = DstY;
	Right = DstX + (S32)Surface->Width;
	Bottom = DstY + (S32)Surface->Height;

	if (Left < Clip->X)
		Left = Clip->X;
	if (Top < Clip->Y)
		Top = Clip->Y;
	if (Right > Clip->X + (S32)Clip->Width)
		Right = Clip->X + (S32)Clip->Width;
	if (Bottom > Clip->Y + (S32)Clip->Height)
		Bottom = Clip->Y + (S32)Clip->Height;
	if (Left < 0)
		Left = 0;
	if (Top < 0)
		Top = 0;
	if (Right > (S32)Rdr->FbWidth)
		Right = (S32)Rdr->FbWidth;
	if (Bottom > (S32)Rdr->FbHeight)
		Bottom = (S32)Rdr->FbHeight;
	if (Left >= Right || Top >= Bottom)
		return;

	CopyWidth = (U32)(Right - Left);
	SrcX = (U32)(Left - DstX);
	SrcY = (U32)(Top - DstY);
	Pitch = Rdr->FbPitch / 4;

	for (U32 Y = 0; Y < (U32)(Bottom - Top); Y++) {
		U32 *DstRow = Rdr->SceneBuf + ((U32)Top + Y) * Pitch + (U32)Left;
		const U32 *SrcRow =
			Surface->Pixels + (SrcY + Y) * Surface->Pitch + SrcX;
		MemCopy(DstRow, SrcRow, (Size)CopyWidth * sizeof(U32));
	}
}

static Bool WindowHasHigherIntersectingWindow(const AetherRenderer *Rdr,
											  const AdkWindow *Wnd,
											  const AdkRect *Rect)
{
	int WindowIndex = -1;

	for (int i = 0; i < Rdr->WinMgr->WindowCount; i++) {
		if (Rdr->WinMgr->Windows[i] == Wnd) {
			WindowIndex = i;
			break;
		}
	}

	if (WindowIndex < 0)
		return True;

	for (int i = WindowIndex + 1; i < Rdr->WinMgr->WindowCount; i++) {
		AdkWindow *Other = Rdr->WinMgr->Windows[i];
		AdkRect Bounds;

		if (!Other || !Other->Visible)
			continue;

		Bounds = AdkWindowGetBounds(Other);
		if (RectIntersects(&Bounds, Rect))
			return True;
	}

	return False;
}

static void DrawOverlay(AetherRenderer *Rdr)
{
	AdkCanvas Canvas;
	AdkRect Box;

	if (!OverlayRect(Rdr, &Box))
		return;

	AdkCanvasBind(&Canvas, Rdr->SceneBuf, Rdr->FbWidth, Rdr->FbHeight,
				  Rdr->FbPitch / 4);
	AdkCanvasFillRect(&Canvas, &Box, 0xCC091018u);
	AdkFontDrawText(&Canvas, 12, 13, Rdr->OverlayText, 0xFFFFFFFFu);
}

static void DrawCursorToFramebuffer(AetherRenderer *Rdr)
{
	AdkCanvas Canvas;
	AdkCanvasBind(&Canvas, (U32 *)Rdr->FbBase, Rdr->FbWidth, Rdr->FbHeight,
				  Rdr->FbPitch / 4);

	if (Rdr->Cursor)
		AetherCursorDraw(Rdr->Cursor, &Canvas);
}

/*
 * Draw a solid-color rectangle border directly into the scene buffer, without
 * touching the clean buffer.  Thickness is clamped so it never exceeds half
 * the smaller dimension.
 */
static void DrawWireframeRect(AetherRenderer *Rdr, const AdkRect *Rect,
							  U32 Color, U32 Thickness)
{
	AdkCanvas Canvas;
	AdkRect Clipped;

	if (!ClipRect(Rdr, Rect, &Clipped))
		return;

	AdkCanvasBind(&Canvas, Rdr->SceneBuf, Rdr->FbWidth, Rdr->FbHeight,
				  Rdr->FbPitch / 4);
	AdkCanvasFrameRect(&Canvas, &Clipped, Color, (S32)Thickness);
}

void AetherRenderFull(AetherRenderer *Rdr)
{
	AdkCanvas Canvas;
	AdkCanvasBind(&Canvas, Rdr->SceneBuf, Rdr->FbWidth, Rdr->FbHeight,
				  Rdr->FbPitch / 4);

	MemCopy(Rdr->SceneBuf, Rdr->CleanBuf, Rdr->BufSize);

	for (int i = 0; i < Rdr->WinMgr->WindowCount; i++) {
		/* Skip the window being dragged in wireframe mode — it will be
		 * represented by an outline only. */
		if (Rdr->WinMgr->WireframeDrag && i == Rdr->WinMgr->DragIndex)
			continue;
		AdkWindowDraw(Rdr->WinMgr->Windows[i], &Canvas);
	}

	/* Draw wireframe outline for the dragged window */
	if (Rdr->WinMgr->WireframeDrag && Rdr->WinMgr->DragIndex >= 0 &&
		Rdr->WinMgr->DragIndex < Rdr->WinMgr->WindowCount &&
		Rdr->WinMgr->Windows[Rdr->WinMgr->DragIndex]) {
		AdkWindow *Wnd = Rdr->WinMgr->Windows[Rdr->WinMgr->DragIndex];
		AdkRect WndRect = AdkWindowGetBounds(Wnd);
		DrawWireframeRect(Rdr, &WndRect, WIREFRAME_COLOR, WIREFRAME_THICKNESS);
	}

	DrawOverlay(Rdr);
	MemCopy((U32 *)Rdr->FbBase, Rdr->SceneBuf, Rdr->BufSize);
	DrawCursorToFramebuffer(Rdr);

	if (Rdr->Cursor) {
		Rdr->PrevCursorX = Rdr->Cursor->X;
		Rdr->PrevCursorY = Rdr->Cursor->Y;
		Rdr->HasPrevCursor = True;
	}
}

void AetherRenderRegion(AetherRenderer *Rdr, const AdkRect *Rect)
{
	AdkCanvas Canvas;
	AdkRect Clipped;
	AdkRect OvlRect;

	if (!Rdr || !Rect) {
		AetherRenderFull(Rdr);
		return;
	}
	if (!ClipRect(Rdr, Rect, &Clipped))
		return;

	CopyRectFromCleanToScene(Rdr, &Clipped);
	AdkCanvasBind(&Canvas, Rdr->SceneBuf, Rdr->FbWidth, Rdr->FbHeight,
				  Rdr->FbPitch / 4);

	for (int i = 0; i < Rdr->WinMgr->WindowCount; i++) {
		AdkWindow *Wnd = Rdr->WinMgr->Windows[i];
		AdkRect Bounds;

		if (!Wnd || !Wnd->Visible)
			continue;

		/* Skip the dragged window — draw wireframe instead */
		if (Rdr->WinMgr->WireframeDrag && i == Rdr->WinMgr->DragIndex)
			continue;

		Bounds = AdkWindowGetBounds(Wnd);
		if (!RectIntersects(&Bounds, &Clipped))
			continue;
		AdkWindowDraw(Wnd, &Canvas);
	}

	/* Wireframe for dragged window, clipped to the dirty region */
	if (Rdr->WinMgr->WireframeDrag && Rdr->WinMgr->DragIndex >= 0 &&
		Rdr->WinMgr->DragIndex < Rdr->WinMgr->WindowCount &&
		Rdr->WinMgr->Windows[Rdr->WinMgr->DragIndex]) {
		AdkWindow *Wnd = Rdr->WinMgr->Windows[Rdr->WinMgr->DragIndex];
		AdkRect WndRect = AdkWindowGetBounds(Wnd);
		if (RectIntersects(&WndRect, &Clipped))
			DrawWireframeRect(Rdr, &WndRect, WIREFRAME_COLOR,
							  WIREFRAME_THICKNESS);
	}

	if (OverlayRect(Rdr, &OvlRect) && RectIntersects(&OvlRect, &Clipped))
		DrawOverlay(Rdr);

	CopyRectToFramebuffer(Rdr, &Clipped);
	AetherRenderCursor(Rdr);
}

void AetherRenderWindowClient(AetherRenderer *Rdr, const AdkWindow *Wnd)
{
	AdkRect ClientRect;
	AdkRect DirtyRect;
	AdkRect OvlRect;

	if (!Rdr)
		return;
	if (!Wnd || !Wnd->Visible) {
		AetherRenderFull(Rdr);
		return;
	}
	if (AdkWindowIsTranslucent(Wnd)) {
		AetherRenderFull(Rdr);
		return;
	}

	ClientRect = AdkWindowGetClientRect(Wnd);
	if (!ClipRect(Rdr, &ClientRect, &DirtyRect))
		return;

	/* If something above this window covers the updated client area, fall back
	 * to the full compositor. The fast path is for the common raw-surface case:
	 * the client rewrites the whole opaque client buffer, and no higher window
	 * needs to be recomposited over it. */
	if (WindowHasHigherIntersectingWindow(Rdr, Wnd, &DirtyRect)) {
		AetherRenderFull(Rdr);
		return;
	}

	BlitSurfaceOpaqueToScene(Rdr, &Wnd->Surface, ClientRect.X, ClientRect.Y,
							 &DirtyRect);

	if (OverlayRect(Rdr, &OvlRect) && RectIntersects(&OvlRect, &DirtyRect))
		DrawOverlay(Rdr);

	CopyRectToFramebuffer(Rdr, &DirtyRect);
}

void AetherRenderCursor(AetherRenderer *Rdr)
{
	AdkRect OldRect;
	AdkRect NewRect;

	if (!Rdr->Cursor || !Rdr->HasPrevCursor) {
		AetherRenderFull(Rdr);
		return;
	}

	OldRect = CursorRect(Rdr, Rdr->PrevCursorX, Rdr->PrevCursorY);
	NewRect = CursorRect(Rdr, Rdr->Cursor->X, Rdr->Cursor->Y);

	CopyRectToFramebuffer(Rdr, &OldRect);
	CopyRectToFramebuffer(Rdr, &NewRect);
	DrawCursorToFramebuffer(Rdr);

	Rdr->PrevCursorX = Rdr->Cursor->X;
	Rdr->PrevCursorY = Rdr->Cursor->Y;
}
