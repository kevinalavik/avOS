#include <Adk/AdkGraphics.h>

static void AdkMemCopy(void *Dst, const void *Src, Size N)
{
	U64 *D = (U64 *)Dst;
	const U64 *S = (const U64 *)Src;
	Size NWords = N / 8;

	for (Size i = 0; i < NWords; i++)
		D[i] = S[i];
	for (Size i = NWords * 8; i < N; i++)
		((Byte *)Dst)[i] = ((const Byte *)Src)[i];
}

static Bool AdkRectClipToSurface(const AdkSurface *Surface, const AdkRect *In,
								 AdkRect *Out)
{
	S32 Left = In->X;
	S32 Top = In->Y;
	S32 Right = In->X + (S32)In->Width;
	S32 Bottom = In->Y + (S32)In->Height;

	if (Left < 0)
		Left = 0;
	if (Top < 0)
		Top = 0;
	if (Right > (S32)Surface->Width)
		Right = (S32)Surface->Width;
	if (Bottom > (S32)Surface->Height)
		Bottom = (S32)Surface->Height;

	if (Left >= Right || Top >= Bottom)
		return False;

	Out->X = Left;
	Out->Y = Top;
	Out->Width = (U32)(Right - Left);
	Out->Height = (U32)(Bottom - Top);
	return True;
}

static U32 AdkBlendPixel(U32 Dst, U32 Src)
{
	U8 Alpha = (U8)(Src >> 24);
	if (Alpha == 255)
		return Src;
	if (Alpha == 0)
		return Dst;

	U8 DstR = (U8)(Dst >> 0);
	U8 DstG = (U8)(Dst >> 8);
	U8 DstB = (U8)(Dst >> 16);
	U8 SrcR = (U8)(Src >> 0);
	U8 SrcG = (U8)(Src >> 8);
	U8 SrcB = (U8)(Src >> 16);

	U8 OutR = (U8)(((U16)SrcR * Alpha + (U16)DstR * (255 - Alpha)) / 255);
	U8 OutG = (U8)(((U16)SrcG * Alpha + (U16)DstG * (255 - Alpha)) / 255);
	U8 OutB = (U8)(((U16)SrcB * Alpha + (U16)DstB * (255 - Alpha)) / 255);

	return (U32)OutR | ((U32)OutG << 8) | ((U32)OutB << 16) | 0xFF000000u;
}

static U32 AdkApplyOpacity(U32 Color, U8 Opacity)
{
	U8 Alpha = (U8)(Color >> 24);
	U8 ScaledAlpha = (U8)(((U16)Alpha * (U16)Opacity) / 255);
	return (Color & 0x00FFFFFFu) | ((U32)ScaledAlpha << 24);
}

void AdkSurfaceBind(AdkSurface *Surface, U32 *Pixels, U32 Width, U32 Height,
					U32 Pitch)
{
	Surface->Pixels = Pixels;
	Surface->Width = Width;
	Surface->Height = Height;
	Surface->Pitch = Pitch;
}

void AdkCanvasBind(AdkCanvas *Canvas, U32 *Pixels, U32 Width, U32 Height,
				   U32 Pitch)
{
	AdkSurfaceBind(&Canvas->Surface, Pixels, Width, Height, Pitch);
}

void AdkCanvasClear(AdkCanvas *Canvas, U32 Color)
{
	U32 *Pixels = Canvas->Surface.Pixels;

	for (U32 Y = 0; Y < Canvas->Surface.Height; Y++) {
		U32 RowBase = Y * Canvas->Surface.Pitch;
		for (U32 X = 0; X < Canvas->Surface.Width; X++)
			Pixels[RowBase + X] = Color;
	}
}

void AdkCanvasFillRect(AdkCanvas *Canvas, const AdkRect *Rect, U32 Color)
{
	AdkRect Clipped;
	U8 Alpha;
	if (!AdkRectClipToSurface(&Canvas->Surface, Rect, &Clipped))
		return;

	Alpha = (U8)(Color >> 24);

	for (U32 Y = 0; Y < Clipped.Height; Y++) {
		U32 Row = (U32)(Clipped.Y + (S32)Y) * Canvas->Surface.Pitch;
		U32 Col = (U32)Clipped.X;
		for (U32 X = 0; X < Clipped.Width; X++) {
			U32 Index = Row + Col + X;
			if (Alpha == 255)
				Canvas->Surface.Pixels[Index] = Color;
			else
				Canvas->Surface.Pixels[Index] =
					AdkBlendPixel(Canvas->Surface.Pixels[Index], Color);
		}
	}
}

void AdkCanvasFrameRect(AdkCanvas *Canvas, const AdkRect *Rect, U32 Color,
						U32 Thickness)
{
	if (Thickness == 0 || Rect->Width == 0 || Rect->Height == 0)
		return;

	AdkRect Top = *Rect;
	AdkRect Bottom = *Rect;
	AdkRect Left = *Rect;
	AdkRect Right = *Rect;

	Top.Height = Thickness;
	Bottom.Y = Rect->Y + (S32)Rect->Height - (S32)Thickness;
	Bottom.Height = Thickness;
	Left.Width = Thickness;
	Right.X = Rect->X + (S32)Rect->Width - (S32)Thickness;
	Right.Width = Thickness;

	AdkCanvasFillRect(Canvas, &Top, Color);
	AdkCanvasFillRect(Canvas, &Bottom, Color);
	AdkCanvasFillRect(Canvas, &Left, Color);
	AdkCanvasFillRect(Canvas, &Right, Color);
}

void AdkCanvasBlitSurface(AdkCanvas *Canvas, const AdkSurface *Surface,
						  S32 DstX, S32 DstY)
{
	if (!Surface || !Surface->Pixels)
		return;

	for (U32 Y = 0; Y < Surface->Height; Y++) {
		S32 CanvasY = DstY + (S32)Y;
		if (CanvasY < 0 || CanvasY >= (S32)Canvas->Surface.Height)
			continue;

		for (U32 X = 0; X < Surface->Width; X++) {
			S32 CanvasX = DstX + (S32)X;
			if (CanvasX < 0 || CanvasX >= (S32)Canvas->Surface.Width)
				continue;

			U32 Src = Surface->Pixels[Y * Surface->Pitch + X];
			U32 Index = (U32)CanvasY * Canvas->Surface.Pitch + (U32)CanvasX;
			Canvas->Surface.Pixels[Index] =
				AdkBlendPixel(Canvas->Surface.Pixels[Index], Src);
		}
	}
}

void AdkCanvasBlitSurfaceOpaque(AdkCanvas *Canvas, const AdkSurface *Surface,
								S32 DstX, S32 DstY)
{
	if (!Canvas || !Surface || !Surface->Pixels)
		return;

	S32 Left = DstX;
	S32 Top = DstY;
	S32 Right = DstX + (S32)Surface->Width;
	S32 Bottom = DstY + (S32)Surface->Height;

	if (Left < 0)
		Left = 0;
	if (Top < 0)
		Top = 0;
	if (Right > (S32)Canvas->Surface.Width)
		Right = (S32)Canvas->Surface.Width;
	if (Bottom > (S32)Canvas->Surface.Height)
		Bottom = (S32)Canvas->Surface.Height;
	if (Left >= Right || Top >= Bottom)
		return;

	U32 CopyWidth = (U32)(Right - Left);
	U32 SrcX = (U32)(Left - DstX);
	U32 SrcY = (U32)(Top - DstY);

	for (U32 Y = 0; Y < (U32)(Bottom - Top); Y++) {
		U32 *DstRow = Canvas->Surface.Pixels +
					  ((U32)Top + Y) * Canvas->Surface.Pitch + (U32)Left;
		const U32 *SrcRow =
			Surface->Pixels + (SrcY + Y) * Surface->Pitch + SrcX;
		AdkMemCopy(DstRow, SrcRow, (Size)CopyWidth * sizeof(U32));
	}
}

void AdkCanvasBlitSurfaceOpacity(AdkCanvas *Canvas, const AdkSurface *Surface,
								 S32 DstX, S32 DstY, U8 Opacity)
{
	if (!Canvas || !Surface || !Surface->Pixels)
		return;

	if (Opacity == 255) {
		AdkCanvasBlitSurface(Canvas, Surface, DstX, DstY);
		return;
	}
	if (Opacity == 0)
		return;

	for (U32 Y = 0; Y < Surface->Height; Y++) {
		S32 CanvasY = DstY + (S32)Y;
		if (CanvasY < 0 || CanvasY >= (S32)Canvas->Surface.Height)
			continue;

		for (U32 X = 0; X < Surface->Width; X++) {
			S32 CanvasX = DstX + (S32)X;
			U32 Src;
			U32 Index;

			if (CanvasX < 0 || CanvasX >= (S32)Canvas->Surface.Width)
				continue;

			Src = AdkApplyOpacity(Surface->Pixels[Y * Surface->Pitch + X],
								  Opacity);
			Index = (U32)CanvasY * Canvas->Surface.Pitch + (U32)CanvasX;
			Canvas->Surface.Pixels[Index] =
				AdkBlendPixel(Canvas->Surface.Pixels[Index], Src);
		}
	}
}

void AdkCanvasDrawImage(AdkCanvas *Canvas, const AdkImage *Image, S32 DstX,
						S32 DstY)
{
	if (!Image || !Image->Pixels)
		return;

	AdkSurface Surface;
	AdkSurfaceBind(&Surface, Image->Pixels, Image->Width, Image->Height,
				   Image->Width);
	AdkCanvasBlitSurface(Canvas, &Surface, DstX, DstY);
}

void AdkCanvasDrawImageRegion(AdkCanvas *Canvas, const AdkImage *Image,
							  const AdkRect *SrcRect, S32 DstX, S32 DstY)
{
	AdkRect DstRect;

	if (!SrcRect)
		return;

	DstRect.X = DstX;
	DstRect.Y = DstY;
	DstRect.Width = SrcRect->Width;
	DstRect.Height = SrcRect->Height;
	AdkCanvasDrawImageRegionScaled(Canvas, Image, SrcRect, &DstRect);
}

void AdkCanvasDrawImageRegionScaled(AdkCanvas *Canvas, const AdkImage *Image,
									const AdkRect *SrcRect,
									const AdkRect *DstRect)
{
	AdkRect Clipped;

	if (!Canvas || !Image || !Image->Pixels || !SrcRect || !DstRect ||
		SrcRect->Width == 0 || SrcRect->Height == 0 || DstRect->Width == 0 ||
		DstRect->Height == 0)
		return;
	if (SrcRect->X < 0 || SrcRect->Y < 0 ||
		SrcRect->X + (S32)SrcRect->Width > (S32)Image->Width ||
		SrcRect->Y + (S32)SrcRect->Height > (S32)Image->Height)
		return;
	if (!AdkRectClipToSurface(&Canvas->Surface, DstRect, &Clipped))
		return;

	for (U32 Y = 0; Y < Clipped.Height; Y++) {
		S32 AbsY = Clipped.Y + (S32)Y;
		U32 SrcY = (U32)SrcRect->Y +
				   (U32)(((U64)(AbsY - DstRect->Y) * SrcRect->Height) /
						 DstRect->Height);
		if (SrcY >= (U32)SrcRect->Y + SrcRect->Height)
			SrcY = (U32)SrcRect->Y + SrcRect->Height - 1;

		for (U32 X = 0; X < Clipped.Width; X++) {
			S32 AbsX = Clipped.X + (S32)X;
			U32 SrcX = (U32)SrcRect->X +
					   (U32)(((U64)(AbsX - DstRect->X) * SrcRect->Width) /
							 DstRect->Width);
			U32 Src;
			U32 Index;

			if (SrcX >= (U32)SrcRect->X + SrcRect->Width)
				SrcX = (U32)SrcRect->X + SrcRect->Width - 1;

			Src = Image->Pixels[SrcY * Image->Width + SrcX];
			Index = (U32)AbsY * Canvas->Surface.Pitch + (U32)AbsX;
			Canvas->Surface.Pixels[Index] =
				AdkBlendPixel(Canvas->Surface.Pixels[Index], Src);
		}
	}
}

void AdkCanvasDrawImageScaled(AdkCanvas *Canvas, const AdkImage *Image,
							  const AdkRect *DstRect)
{
	if (!Image || !Image->Pixels || !DstRect || DstRect->Width == 0 ||
		DstRect->Height == 0)
		return;

	AdkRect Clipped;
	if (!AdkRectClipToSurface(&Canvas->Surface, DstRect, &Clipped))
		return;

	for (U32 Y = 0; Y < Clipped.Height; Y++) {
		S32 AbsY = Clipped.Y + (S32)Y;
		U32 SrcY =
			(U32)(((U64)(AbsY - DstRect->Y) * Image->Height) / DstRect->Height);
		if (SrcY >= Image->Height)
			SrcY = Image->Height - 1;

		for (U32 X = 0; X < Clipped.Width; X++) {
			S32 AbsX = Clipped.X + (S32)X;
			U32 SrcX = (U32)(((U64)(AbsX - DstRect->X) * Image->Width) /
							 DstRect->Width);
			if (SrcX >= Image->Width)
				SrcX = Image->Width - 1;

			U32 Src = Image->Pixels[SrcY * Image->Width + SrcX];
			U32 Index = (U32)AbsY * Canvas->Surface.Pitch + (U32)AbsX;
			Canvas->Surface.Pixels[Index] =
				AdkBlendPixel(Canvas->Surface.Pixels[Index], Src);
		}
	}
}
