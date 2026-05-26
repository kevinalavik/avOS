#ifndef AETHER_CONFIG_H
#define AETHER_CONFIG_H

#include <System/Types.h>
#include <Adk/AdkWindow.h>

#define CONFIG_PATH_MAX 256

typedef struct {
	char WallpaperPath[CONFIG_PATH_MAX];
	char CursorPath[CONFIG_PATH_MAX];
	char SkinPath[CONFIG_PATH_MAX];
	char TaskbarSkinPath[CONFIG_PATH_MAX];
	char TextSkinPath[CONFIG_PATH_MAX];
	char RawSkinPath[CONFIG_PATH_MAX];
	AdkWindowTheme TextTheme;
	AdkWindowTheme RawTheme;
	U8 TextOpacity;
	U8 RawOpacity;
	Bool ShowFps;
	Bool WallpaperValid;
	Bool CursorValid;
	Bool SkinValid;
	Bool TaskbarSkinValid;
	Bool TextSkinValid;
	Bool RawSkinValid;
} WmConfig;

void ConfigInit(WmConfig *Cfg);
int ConfigLoad(const char *Path, WmConfig *Cfg);

#endif
