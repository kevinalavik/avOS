#ifndef ADK_WINDOW_H
#define ADK_WINDOW_H

#include <System/Types.h>
#include <Adk/AdkGraphics.h>
#include <Adk/AdkObjectManager.h>

#define ADK_WINDOW_TITLE_BAR_HEIGHT 28u
#define ADK_WINDOW_BORDER_SIZE 2u
#define ADK_WINDOW_CLOSE_BUTTON_SIZE 14u
#define ADK_WINDOW_CLOSE_BUTTON_MARGIN 6u
#define ADK_WINDOW_TITLE_MAX 48u

typedef enum {
	AdkWindowThemeSlate = 0,
	AdkWindowThemeForest,
	AdkWindowThemeSunset,
	AdkWindowThemeCount
} AdkWindowTheme;

typedef enum {
	AdkWindowPartNone,
	AdkWindowPartClient,
	AdkWindowPartTitleBar,
	AdkWindowPartCloseButton,
} AdkWindowPart;

typedef struct AdkWindow AdkWindow;
typedef void (*AdkWindowCloseCallback)(AdkWindow *Wnd);

struct AdkWindow {
	S32 X;
	S32 Y;
	U32 Width, Height;
	AdkSurface Surface;
	U32 *Buffer;
	char Title[ADK_WINDOW_TITLE_MAX];
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
	AdkWindowTheme Theme;
	const AdkImage *Skin;
	U8 Opacity;
	Bool Visible;
	Bool Active;
	AdkWindowCloseCallback OnClose;
	AdkObjectManager *Mgr;
};

AdkWindow *AdkWindowCreate(AdkObjectManager *Mgr, S32 X, S32 Y, U32 Width,
						   U32 Height);
void AdkWindowDestroy(AdkWindow *Wnd);
void AdkWindowDraw(const AdkWindow *Wnd, AdkCanvas *Canvas);
void AdkWindowMoveTo(AdkWindow *Wnd, S32 X, S32 Y);
void AdkWindowSetTitle(AdkWindow *Wnd, const char *Title);
void AdkWindowSetActive(AdkWindow *Wnd, Bool Active);
void AdkWindowSetTheme(AdkWindow *Wnd, AdkWindowTheme Theme);
void AdkWindowSetSkin(AdkWindow *Wnd, const AdkImage *Skin);
void AdkWindowSetOpacity(AdkWindow *Wnd, U8 Opacity);
void AdkWindowFillClient(AdkWindow *Wnd, U32 Color);
AdkRect AdkWindowGetBounds(const AdkWindow *Wnd);
AdkRect AdkWindowGetClientRect(const AdkWindow *Wnd);
AdkWindowPart AdkWindowHitTest(const AdkWindow *Wnd, S32 X, S32 Y);
Bool AdkWindowIsOnBar(const AdkWindow *Wnd, S32 X, S32 Y);
Bool AdkWindowIsOnClose(const AdkWindow *Wnd, S32 X, S32 Y);
Bool AdkWindowIsOnBarRel(const AdkWindow *Wnd, S32 RelX, S32 RelY);
Bool AdkWindowIsOnCloseRel(const AdkWindow *Wnd, S32 RelX, S32 RelY);
Bool AdkWindowIsTranslucent(const AdkWindow *Wnd);
void AdkWindowSetCloseCallback(AdkWindow *Wnd, AdkWindowCloseCallback Cb);

#endif
