#include <Adk/AdkWindow.h>
#include <Adk/AdkFont.h>
#include <System/Memory.h>

typedef struct {
	U32 FrameColor;
	U32 TitleBarColor;
	U32 TitleBarActiveColor;
	U32 ClientColor;
	U32 BorderColor;
	U32 AccentColor;
	U32 AccentActiveColor;
	U32 HighlightColor;
	U32 ShadowColor;
	U32 CloseButtonColor;
	U32 CloseButtonActiveColor;
	U8 Opacity;
} AdkWindowThemeSpec;

typedef struct {
	AdkRect FrameTopLeft;
	AdkRect FrameTop;
	AdkRect FrameTopRight;
	AdkRect FrameLeft;
	AdkRect FrameCenter;
	AdkRect FrameRight;
	AdkRect FrameBottomLeft;
	AdkRect FrameBottom;
	AdkRect FrameBottomRight;
	AdkRect CloseButton;
	AdkRect Icon;
} AdkWindowSkinRects;

static const AdkWindowThemeSpec gAdkWindowThemes[AdkWindowThemeCount] = {
	{ 0xCC0B1017u, 0xD42A3444u, 0xE03E5B7Fu, 0xC81A2330u, 0xF08FA2B8u,
	  0x7AA9D6FFu, 0xB0D8ECFFu, 0xA0FFFFFFu, 0x48000000u, 0xE0A64B47u,
	  0xFFF17872u, 230u },
	{ 0xCC0E1511u, 0xD4263D34u, 0xDE3A684Du, 0xC814251Eu, 0xF08DB39Du,
	  0x7A95E2B0u, 0xB0C6F3D5u, 0x9CFFFFFFu, 0x46000000u, 0xD49C5445u,
	  0xFFED8A6Fu, 226u },
	{ 0xCC17100Cu, 0xD44B3027u, 0xDE8A4A38u, 0xC8271914u, 0xF0D4A78Cu,
	  0x7AF3B387u, 0xB0FFD6B1u, 0x96FFF7EEu, 0x42000000u, 0xD4A24A42u,
	  0xFFFF8D73u, 222u }
};

static const AdkWindowSkinRects gAdkWindowSkinInactive = {
	{ 0, 0, 12, 28 },  { 12, 0, 24, 28 },  { 36, 0, 12, 28 },
	{ 0, 28, 12, 12 }, { 12, 28, 24, 12 }, { 36, 28, 12, 12 },
	{ 0, 40, 12, 8 },  { 12, 40, 24, 8 },  { 36, 40, 12, 8 },
	{ 52, 6, 16, 16 }, { 52, 28, 16, 16 }
};

static const AdkWindowSkinRects gAdkWindowSkinActive = {
	{ 0, 48, 12, 28 },	{ 12, 48, 24, 28 }, { 36, 48, 12, 28 },
	{ 0, 76, 12, 12 },	{ 12, 76, 24, 12 }, { 36, 76, 12, 12 },
	{ 0, 88, 12, 8 },	{ 12, 88, 24, 8 },	{ 36, 88, 12, 8 },
	{ 52, 54, 16, 16 }, { 52, 76, 16, 16 }
};

static void AdkStringCopy(char *Dst, Size DstSize, const char *Src)
{
	Size Index = 0;

	if (!Dst || DstSize == 0)
		return;

	if (!Src)
		Src = "";

	while (Index + 1 < DstSize && Src[Index]) {
		Dst[Index] = Src[Index];
		Index++;
	}

	Dst[Index] = '\0';
}

static U32 AdkWindowClientHeight(const AdkWindow *Wnd)
{
	U32 ChromeHeight = ADK_WINDOW_TITLE_BAR_HEIGHT + ADK_WINDOW_BORDER_SIZE;
	if (Wnd->Height <= ChromeHeight)
		return 1;
	return Wnd->Height - ChromeHeight;
}

static AdkRect AdkWindowCloseRectRel(const AdkWindow *Wnd)
{
	AdkRect Rect;
	Rect.Width = ADK_WINDOW_CLOSE_BUTTON_SIZE;
	Rect.Height = ADK_WINDOW_CLOSE_BUTTON_SIZE;
	Rect.X =
		(S32)Wnd->Width - (S32)ADK_WINDOW_CLOSE_BUTTON_MARGIN - (S32)Rect.Width;
	Rect.Y = ((S32)ADK_WINDOW_TITLE_BAR_HEIGHT - (S32)Rect.Height) / 2;
	return Rect;
}

static U32 AdkColorSetAlpha(U32 Color, U8 Alpha)
{
	return (Color & 0x00FFFFFFu) | ((U32)Alpha << 24);
}

static U32 AdkColorScaleAlpha(U32 Color, U8 AlphaScale)
{
	U8 Alpha = (U8)(Color >> 24);
	U8 Scaled = (U8)(((U16)Alpha * (U16)AlphaScale) / 255);
	return AdkColorSetAlpha(Color, Scaled);
}

static void AdkWindowApplyTheme(AdkWindow *Wnd, const AdkWindowThemeSpec *Theme,
								AdkWindowTheme ThemeId)
{
	Wnd->FrameColor = Theme->FrameColor;
	Wnd->TitleBarColor = Theme->TitleBarColor;
	Wnd->TitleBarActiveColor = Theme->TitleBarActiveColor;
	Wnd->ClientColor = Theme->ClientColor;
	Wnd->BorderColor = Theme->BorderColor;
	Wnd->AccentColor = Theme->AccentColor;
	Wnd->AccentActiveColor = Theme->AccentActiveColor;
	Wnd->HighlightColor = Theme->HighlightColor;
	Wnd->ShadowColor = Theme->ShadowColor;
	Wnd->CloseButtonColor = Theme->CloseButtonColor;
	Wnd->CloseButtonActiveColor = Theme->CloseButtonActiveColor;
	Wnd->Theme = ThemeId;
	Wnd->Opacity = Theme->Opacity;
}

static void AdkWindowDrawHorizontalBand(AdkCanvas *Canvas, const AdkRect *Rect,
										U32 TopColor, U32 BottomColor)
{
	if (!Canvas || !Rect || Rect->Width == 0 || Rect->Height == 0)
		return;

	for (U32 Y = 0; Y < Rect->Height; Y++) {
		AdkRect Band = { Rect->X, Rect->Y + (S32)Y, Rect->Width, 1 };
		U32 Alpha;
		U32 Color;

		if (Rect->Height == 1) {
			AdkCanvasFillRect(Canvas, &Band, TopColor);
			continue;
		}

		Alpha = (U32)(((U64)(Rect->Height - 1 - Y) * (TopColor >> 24)) +
					  ((U64)Y * (BottomColor >> 24))) /
				(Rect->Height - 1);
		Color = (((TopColor & 0x00FF00FFu) * (Rect->Height - 1 - Y)) +
				 ((BottomColor & 0x00FF00FFu) * Y)) /
				(Rect->Height - 1);
		Color &= 0x00FF00FFu;
		Color |= ((((TopColor & 0x0000FF00u) * (Rect->Height - 1 - Y)) +
				   ((BottomColor & 0x0000FF00u) * Y)) /
				  (Rect->Height - 1)) &
				 0x0000FF00u;
		Color |= Alpha << 24;
		AdkCanvasFillRect(Canvas, &Band, Color);
	}
}

static void AdkWindowDrawGripDots(AdkCanvas *Canvas, const AdkWindow *Wnd,
								  U32 Color)
{
	for (U32 Row = 0; Row < 2; Row++) {
		for (U32 Col = 0; Col < 4; Col++) {
			AdkRect Dot = { Wnd->X + 12 + (S32)(Col * 6),
							Wnd->Y + 11 + (S32)(Row * 5), 2, 2 };
			AdkCanvasFillRect(Canvas, &Dot, Color);
		}
	}
}

static U32 AdkWindowChooseTextColorFromSkin(const AdkWindow *Wnd,
											const AdkWindowSkinRects *Rects)
{
	if (!Wnd || !Wnd->Skin || !Wnd->Skin->Pixels)
		return 0xFF202632u;

	U64 SumLuma = 0;
	U64 Samples = 0;
	S32 StartX = Rects->FrameTop.X;
	S32 EndX = Rects->FrameTop.X + (S32)Rects->FrameTop.Width;
	S32 StartY = Rects->FrameTop.Y;
	S32 EndY = Rects->FrameTop.Y + (S32)Rects->FrameTop.Height;

	for (S32 Y = StartY + 4; Y < EndY - 4; Y += 2) {
		for (S32 X = StartX + 2; X < EndX - 2; X += 2) {
			U32 Pixel = Wnd->Skin->Pixels[(U32)Y * Wnd->Skin->Width + (U32)X];
			U32 R = (Pixel >> 0) & 0xFFu;
			U32 G = (Pixel >> 8) & 0xFFu;
			U32 B = (Pixel >> 16) & 0xFFu;
			SumLuma += R * 299u + G * 587u + B * 114u;
			Samples++;
		}
	}

	if (Samples == 0)
		return 0xFF202632u;

	SumLuma /= Samples;
	SumLuma /= 1000u;
	return SumLuma >= 170u ? 0xFF202632u : 0xFFF7FAFCu;
}

static void AdkWindowDrawSkinFrame(AdkCanvas *Canvas, const AdkWindow *Wnd,
								   const AdkWindowSkinRects *Rects)
{
	AdkRect Dst;
	U32 LeftWidth = Rects->FrameLeft.Width;
	U32 RightWidth = Rects->FrameRight.Width;
	U32 TopHeight = Rects->FrameTop.Height;
	U32 BottomHeight = Rects->FrameBottom.Height;
	U32 MiddleWidth = Wnd->Width > LeftWidth + RightWidth ?
						  Wnd->Width - LeftWidth - RightWidth :
						  1u;
	U32 MiddleHeight = Wnd->Height > TopHeight + BottomHeight ?
						   Wnd->Height - TopHeight - BottomHeight :
						   1u;

	Dst = (AdkRect){ Wnd->X, Wnd->Y, Rects->FrameTopLeft.Width,
					 Rects->FrameTopLeft.Height };
	AdkCanvasDrawImageRegion(Canvas, Wnd->Skin, &Rects->FrameTopLeft, Dst.X,
							 Dst.Y);
	Dst = (AdkRect){ Wnd->X + (S32)LeftWidth, Wnd->Y, MiddleWidth,
					 Rects->FrameTop.Height };
	AdkCanvasDrawImageRegionScaled(Canvas, Wnd->Skin, &Rects->FrameTop, &Dst);
	Dst = (AdkRect){ Wnd->X + (S32)Wnd->Width - (S32)Rects->FrameTopRight.Width,
					 Wnd->Y, Rects->FrameTopRight.Width,
					 Rects->FrameTopRight.Height };
	AdkCanvasDrawImageRegion(Canvas, Wnd->Skin, &Rects->FrameTopRight, Dst.X,
							 Dst.Y);

	Dst = (AdkRect){ Wnd->X, Wnd->Y + (S32)TopHeight, Rects->FrameLeft.Width,
					 MiddleHeight };
	AdkCanvasDrawImageRegionScaled(Canvas, Wnd->Skin, &Rects->FrameLeft, &Dst);
	Dst = (AdkRect){ Wnd->X + (S32)LeftWidth, Wnd->Y + (S32)TopHeight,
					 MiddleWidth, MiddleHeight };
	AdkCanvasDrawImageRegionScaled(Canvas, Wnd->Skin, &Rects->FrameCenter,
								   &Dst);
	Dst = (AdkRect){ Wnd->X + (S32)Wnd->Width - (S32)Rects->FrameRight.Width,
					 Wnd->Y + (S32)TopHeight, Rects->FrameRight.Width,
					 MiddleHeight };
	AdkCanvasDrawImageRegionScaled(Canvas, Wnd->Skin, &Rects->FrameRight, &Dst);

	Dst = (AdkRect){
		Wnd->X, Wnd->Y + (S32)Wnd->Height - (S32)Rects->FrameBottomLeft.Height,
		Rects->FrameBottomLeft.Width, Rects->FrameBottomLeft.Height
	};
	AdkCanvasDrawImageRegion(Canvas, Wnd->Skin, &Rects->FrameBottomLeft, Dst.X,
							 Dst.Y);
	Dst = (AdkRect){ Wnd->X + (S32)LeftWidth,
					 Wnd->Y + (S32)Wnd->Height - (S32)Rects->FrameBottom.Height,
					 MiddleWidth, Rects->FrameBottom.Height };
	AdkCanvasDrawImageRegionScaled(Canvas, Wnd->Skin, &Rects->FrameBottom,
								   &Dst);
	Dst = (AdkRect){
		Wnd->X + (S32)Wnd->Width - (S32)Rects->FrameBottomRight.Width,
		Wnd->Y + (S32)Wnd->Height - (S32)Rects->FrameBottomRight.Height,
		Rects->FrameBottomRight.Width, Rects->FrameBottomRight.Height
	};
	AdkCanvasDrawImageRegion(Canvas, Wnd->Skin, &Rects->FrameBottomRight, Dst.X,
							 Dst.Y);
}

static void AdkWindowDrawSkinned(const AdkWindow *Wnd, AdkCanvas *Canvas)
{
	AdkRect ClientRect = AdkWindowGetClientRect(Wnd);
	AdkRect CloseRect;
	AdkRect IconRect;
	U32 TitleColor;
	const AdkWindowSkinRects *Rects =
		Wnd->Active ? &gAdkWindowSkinActive : &gAdkWindowSkinInactive;

	AdkWindowDrawSkinFrame(Canvas, Wnd, Rects);
	AdkCanvasBlitSurfaceOpacity(Canvas, &Wnd->Surface, ClientRect.X,
								ClientRect.Y, Wnd->Opacity);

	CloseRect.X = Wnd->X + (S32)Wnd->Width -
				  (S32)ADK_WINDOW_CLOSE_BUTTON_MARGIN -
				  (S32)Rects->CloseButton.Width;
	CloseRect.Y =
		Wnd->Y +
		((S32)ADK_WINDOW_TITLE_BAR_HEIGHT - (S32)Rects->CloseButton.Height) / 2;
	CloseRect.Width = Rects->CloseButton.Width;
	CloseRect.Height = Rects->CloseButton.Height;
	AdkCanvasDrawImageRegion(Canvas, Wnd->Skin, &Rects->CloseButton,
							 CloseRect.X, CloseRect.Y);

	IconRect.X = Wnd->X + 8;
	IconRect.Y = Wnd->Y + 6;
	IconRect.Width = Rects->Icon.Width;
	IconRect.Height = Rects->Icon.Height;
	AdkCanvasDrawImageRegion(Canvas, Wnd->Skin, &Rects->Icon, IconRect.X,
							 IconRect.Y);

	TitleColor = AdkWindowChooseTextColorFromSkin(Wnd, Rects);
	AdkFontDrawText(Canvas, Wnd->X + 30, Wnd->Y + 9, Wnd->Title, TitleColor);
}

AdkWindow *AdkWindowCreate(AdkObjectManager *Mgr, S32 X, S32 Y, U32 Width,
						   U32 Height)
{
	if (!Mgr || Width <= ADK_WINDOW_BORDER_SIZE * 2 ||
		Height <= ADK_WINDOW_TITLE_BAR_HEIGHT + ADK_WINDOW_BORDER_SIZE)
		return 0;

	AdkWindow *Wnd = (AdkWindow *)AdkObjectAlloc(Mgr, sizeof(AdkWindow));
	if (!Wnd)
		return 0;

	U32 ClientH = Height - ADK_WINDOW_TITLE_BAR_HEIGHT - ADK_WINDOW_BORDER_SIZE;
	U32 *Buf = (U32 *)MemoryAlloc((Size)Width * ClientH * sizeof(U32));
	if (!Buf) {
		AdkObjectFree(Mgr, Wnd);
		return 0;
	}

	for (U32 i = 0; i < Width * ClientH; i++)
		Buf[i] = 0xFF1B1B22u;

	Wnd->X = X;
	Wnd->Y = Y;
	Wnd->Width = Width;
	Wnd->Height = Height;
	Wnd->Buffer = Buf;
	AdkSurfaceBind(&Wnd->Surface, Buf, Width, ClientH, Width);
	AdkStringCopy(Wnd->Title, sizeof(Wnd->Title), "Window");
	Wnd->Visible = True;
	Wnd->Active = False;
	Wnd->OnClose = 0;
	Wnd->Mgr = Mgr;
	Wnd->Skin = 0;
	AdkWindowApplyTheme(Wnd, &gAdkWindowThemes[AdkWindowThemeSlate],
						AdkWindowThemeSlate);
	return Wnd;
}

void AdkWindowDestroy(AdkWindow *Wnd)
{
	if (!Wnd)
		return;

	if (Wnd->OnClose)
		Wnd->OnClose(Wnd);

	MemoryFree(Wnd->Buffer);
	AdkObjectFree(Wnd->Mgr, Wnd);
}

void AdkWindowDraw(const AdkWindow *Wnd, AdkCanvas *Canvas)
{
	if (!Wnd || !Canvas || !Wnd->Visible)
		return;
	if (Wnd->Skin) {
		AdkWindowDrawSkinned(Wnd, Canvas);
		return;
	}

	AdkRect Bounds = AdkWindowGetBounds(Wnd);
	AdkRect TitleBar = Bounds;
	AdkRect Shadow = { Wnd->X + 3, Wnd->Y + 4, Wnd->Width, Wnd->Height };
	AdkRect InnerFrame = { Wnd->X + 1, Wnd->Y + 1, Wnd->Width - 2,
						   Wnd->Height - 2 };
	AdkRect AccentBar;
	AdkRect TitleShine;
	AdkRect TitleStripe;
	TitleBar.Height = ADK_WINDOW_TITLE_BAR_HEIGHT;

	AdkRect ClientRect = AdkWindowGetClientRect(Wnd);
	AdkRect BottomBorder = Bounds;
	BottomBorder.Y = ClientRect.Y + (S32)ClientRect.Height;
	BottomBorder.Height = ADK_WINDOW_BORDER_SIZE;
	AccentBar.X = Wnd->X + (S32)ADK_WINDOW_BORDER_SIZE;
	AccentBar.Y = Wnd->Y + (S32)ADK_WINDOW_TITLE_BAR_HEIGHT - 4;
	AccentBar.Width = Wnd->Width - (ADK_WINDOW_BORDER_SIZE * 2);
	AccentBar.Height = 3;
	TitleShine.X = Wnd->X + (S32)ADK_WINDOW_BORDER_SIZE;
	TitleShine.Y = Wnd->Y + 1;
	TitleShine.Width = Wnd->Width - (ADK_WINDOW_BORDER_SIZE * 2);
	TitleShine.Height = ADK_WINDOW_TITLE_BAR_HEIGHT / 2;
	TitleStripe.X = Wnd->X + (S32)ADK_WINDOW_BORDER_SIZE;
	TitleStripe.Y = Wnd->Y + (S32)ADK_WINDOW_TITLE_BAR_HEIGHT / 2;
	TitleStripe.Width = Wnd->Width - (ADK_WINDOW_BORDER_SIZE * 2);
	TitleStripe.Height = 1;

	AdkCanvasFillRect(Canvas, &Shadow, Wnd->ShadowColor);
	AdkCanvasFillRect(Canvas, &Bounds, Wnd->FrameColor);
	AdkCanvasFillRect(Canvas, &InnerFrame, AdkColorSetAlpha(0x0010141Au, 128));
	AdkCanvasFrameRect(Canvas, &Bounds, Wnd->BorderColor,
					   ADK_WINDOW_BORDER_SIZE);
	AdkWindowDrawHorizontalBand(
		Canvas, &TitleBar,
		Wnd->Active ? Wnd->TitleBarActiveColor : Wnd->TitleBarColor,
		Wnd->Active ? AdkColorSetAlpha(Wnd->AccentActiveColor, 208) :
					  AdkColorSetAlpha(Wnd->AccentColor, 188));
	AdkCanvasFillRect(Canvas, &TitleShine,
					  AdkColorScaleAlpha(Wnd->HighlightColor, 96));
	AdkCanvasFillRect(Canvas, &TitleStripe,
					  AdkColorScaleAlpha(Wnd->HighlightColor, 72));
	AdkCanvasFillRect(Canvas, &AccentBar,
					  Wnd->Active ? Wnd->AccentActiveColor : Wnd->AccentColor);
	AdkCanvasFillRect(Canvas, &ClientRect, Wnd->ClientColor);
	AdkCanvasBlitSurfaceOpacity(Canvas, &Wnd->Surface, ClientRect.X,
								ClientRect.Y, Wnd->Opacity);
	AdkCanvasFillRect(Canvas, &BottomBorder, Wnd->FrameColor);
	AdkWindowDrawGripDots(Canvas, Wnd,
						  Wnd->Active ?
							  AdkColorScaleAlpha(Wnd->HighlightColor, 160) :
							  AdkColorScaleAlpha(Wnd->HighlightColor, 112));

	AdkRect CloseRect = AdkWindowCloseRectRel(Wnd);
	CloseRect.X += Wnd->X;
	CloseRect.Y += Wnd->Y;
	AdkCanvasFillRect(Canvas, &CloseRect,
					  Wnd->Active ? Wnd->CloseButtonActiveColor :
									Wnd->CloseButtonColor);
	AdkCanvasFrameRect(Canvas, &CloseRect, AdkColorSetAlpha(0x00FFFFFFu, 90),
					   1);
	AdkFontDrawText(Canvas, Wnd->X + 10, Wnd->Y + 9, Wnd->Title, 0xFFF1F5F9u);

	for (U32 I = 0; I < ADK_WINDOW_CLOSE_BUTTON_SIZE; I++) {
		S32 Px = CloseRect.X + (S32)I;
		S32 PyA = CloseRect.Y + (S32)I;
		S32 PyB = CloseRect.Y + (S32)ADK_WINDOW_CLOSE_BUTTON_SIZE - 1 - (S32)I;
		AdkRect DotA = { Px, PyA, 1, 1 };
		AdkRect DotB = { Px, PyB, 1, 1 };
		AdkCanvasFillRect(Canvas, &DotA, 0xFFFFFFFFu);
		AdkCanvasFillRect(Canvas, &DotB, 0xFFFFFFFFu);
	}
	if (Wnd->Active)
		AdkCanvasFrameRect(Canvas, &Bounds, AdkColorSetAlpha(0x00FFFFFFu, 176),
						   2);
}

Bool AdkWindowIsOnBar(const AdkWindow *Wnd, S32 X, S32 Y)
{
	return AdkWindowHitTest(Wnd, X, Y) == AdkWindowPartTitleBar;
}

Bool AdkWindowIsOnClose(const AdkWindow *Wnd, S32 X, S32 Y)
{
	return AdkWindowHitTest(Wnd, X, Y) == AdkWindowPartCloseButton;
}

Bool AdkWindowIsOnBarRel(const AdkWindow *Wnd, S32 RelX, S32 RelY)
{
	return RelX >= 0 && (U32)RelX < Wnd->Width && RelY >= 0 &&
		   (U32)RelY < ADK_WINDOW_TITLE_BAR_HEIGHT &&
		   !AdkWindowIsOnCloseRel(Wnd, RelX, RelY);
}

Bool AdkWindowIsOnCloseRel(const AdkWindow *Wnd, S32 RelX, S32 RelY)
{
	if (RelX < 0 || RelY < 0)
		return False;

	AdkRect Rect = AdkWindowCloseRectRel(Wnd);
	U32 Wx = (U32)RelX;
	U32 Wy = (U32)RelY;

	return (Wx >= (U32)Rect.X && Wx < (U32)Rect.X + Rect.Width &&
			Wy >= (U32)Rect.Y && Wy < (U32)Rect.Y + Rect.Height);
}

void AdkWindowSetCloseCallback(AdkWindow *Wnd, AdkWindowCloseCallback Cb)
{
	if (Wnd)
		Wnd->OnClose = Cb;
}

void AdkWindowMoveTo(AdkWindow *Wnd, S32 X, S32 Y)
{
	if (!Wnd)
		return;

	Wnd->X = X;
	Wnd->Y = Y;
}

void AdkWindowSetTitle(AdkWindow *Wnd, const char *Title)
{
	if (!Wnd)
		return;

	AdkStringCopy(Wnd->Title, sizeof(Wnd->Title), Title);
}

void AdkWindowSetActive(AdkWindow *Wnd, Bool Active)
{
	if (Wnd)
		Wnd->Active = Active;
}

void AdkWindowSetTheme(AdkWindow *Wnd, AdkWindowTheme Theme)
{
	if (!Wnd)
		return;
	if (Theme >= AdkWindowThemeCount)
		Theme = AdkWindowThemeSlate;

	AdkWindowApplyTheme(Wnd, &gAdkWindowThemes[Theme], Theme);
}

void AdkWindowSetSkin(AdkWindow *Wnd, const AdkImage *Skin)
{
	if (Wnd)
		Wnd->Skin = Skin;
}

void AdkWindowSetOpacity(AdkWindow *Wnd, U8 Opacity)
{
	if (Wnd)
		Wnd->Opacity = Opacity;
}

void AdkWindowFillClient(AdkWindow *Wnd, U32 Color)
{
	if (!Wnd || !Wnd->Buffer)
		return;

	Wnd->ClientColor = Color;
	for (U32 Y = 0; Y < Wnd->Surface.Height; Y++) {
		U32 Row = Y * Wnd->Surface.Pitch;
		for (U32 X = 0; X < Wnd->Surface.Width; X++)
			Wnd->Buffer[Row + X] = Color;
	}
}

Bool AdkWindowIsTranslucent(const AdkWindow *Wnd)
{
	return Wnd && Wnd->Opacity < 255;
}

AdkRect AdkWindowGetBounds(const AdkWindow *Wnd)
{
	AdkRect Rect;
	Rect.X = Wnd->X;
	Rect.Y = Wnd->Y;
	Rect.Width = Wnd->Width;
	Rect.Height = Wnd->Height;
	return Rect;
}

AdkRect AdkWindowGetClientRect(const AdkWindow *Wnd)
{
	AdkRect Rect;
	Rect.X = Wnd->X;
	Rect.Y = Wnd->Y + (S32)ADK_WINDOW_TITLE_BAR_HEIGHT;
	Rect.Width = Wnd->Width;
	Rect.Height = AdkWindowClientHeight(Wnd);
	return Rect;
}

AdkWindowPart AdkWindowHitTest(const AdkWindow *Wnd, S32 X, S32 Y)
{
	if (!Wnd || !Wnd->Visible)
		return AdkWindowPartNone;

	if (X < Wnd->X || X >= Wnd->X + (S32)Wnd->Width)
		return AdkWindowPartNone;
	if (Y < Wnd->Y || Y >= Wnd->Y + (S32)Wnd->Height)
		return AdkWindowPartNone;

	S32 RelX = X - Wnd->X;
	S32 RelY = Y - Wnd->Y;

	if (AdkWindowIsOnCloseRel(Wnd, RelX, RelY))
		return AdkWindowPartCloseButton;
	if (RelY < (S32)ADK_WINDOW_TITLE_BAR_HEIGHT)
		return AdkWindowPartTitleBar;
	return AdkWindowPartClient;
}
