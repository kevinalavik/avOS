#ifndef ADK_GRAPHICS_H
#define ADK_GRAPHICS_H

#include <System/Types.h>
#include <Adk/AdkImage.h>

typedef struct {
	S32 X;
	S32 Y;
} AdkPoint;

typedef struct {
	U32 Width;
	U32 Height;
} AdkSize;

typedef struct {
	S32 X;
	S32 Y;
	U32 Width;
	U32 Height;
} AdkRect;

typedef struct {
	U32 *Pixels;
	U32 Width;
	U32 Height;
	U32 Pitch;
} AdkSurface;

typedef struct {
	AdkSurface Surface;
} AdkCanvas;

void AdkSurfaceBind(AdkSurface *Surface, U32 *Pixels, U32 Width, U32 Height,
					U32 Pitch);
void AdkCanvasBind(AdkCanvas *Canvas, U32 *Pixels, U32 Width, U32 Height,
				   U32 Pitch);
void AdkCanvasClear(AdkCanvas *Canvas, U32 Color);
void AdkCanvasFillRect(AdkCanvas *Canvas, const AdkRect *Rect, U32 Color);
void AdkCanvasFrameRect(AdkCanvas *Canvas, const AdkRect *Rect, U32 Color,
						U32 Thickness);
void AdkCanvasBlitSurface(AdkCanvas *Canvas, const AdkSurface *Surface,
						  S32 DstX, S32 DstY);
void AdkCanvasBlitSurfaceOpaque(AdkCanvas *Canvas, const AdkSurface *Surface,
								S32 DstX, S32 DstY);
void AdkCanvasBlitSurfaceOpacity(AdkCanvas *Canvas, const AdkSurface *Surface,
								 S32 DstX, S32 DstY, U8 Opacity);
void AdkCanvasDrawImage(AdkCanvas *Canvas, const AdkImage *Image, S32 DstX,
						S32 DstY);
void AdkCanvasDrawImageRegion(AdkCanvas *Canvas, const AdkImage *Image,
							  const AdkRect *SrcRect, S32 DstX, S32 DstY);
void AdkCanvasDrawImageRegionScaled(AdkCanvas *Canvas, const AdkImage *Image,
									const AdkRect *SrcRect,
									const AdkRect *DstRect);
void AdkCanvasDrawImageScaled(AdkCanvas *Canvas, const AdkImage *Image,
							  const AdkRect *DstRect);

#endif
