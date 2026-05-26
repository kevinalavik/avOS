#include <Aether/Config.h>
#include <System/Fileio.h>
#include <System/Types.h>
#include <Lib/String.h>

static AdkWindowTheme ParseThemeName(const char *Value)
{
	if (StringCompare(Value, "forest") == 0)
		return AdkWindowThemeForest;
	if (StringCompare(Value, "sunset") == 0)
		return AdkWindowThemeSunset;
	return AdkWindowThemeSlate;
}

static U8 ParseOpacityValue(const char *Value, U8 Fallback)
{
	U32 Result = 0;
	Size Index = 0;

	if (!Value || Value[0] == '\0')
		return Fallback;

	while (Value[Index] >= '0' && Value[Index] <= '9') {
		Result = Result * 10u + (U32)(Value[Index] - '0');
		if (Result > 255u)
			return 255u;
		Index++;
	}

	if (Index == 0)
		return Fallback;
	return (U8)Result;
}

static Bool ParseBoolValue(const char *Value, Bool Fallback)
{
	if (!Value || Value[0] == '\0')
		return Fallback;
	if (StringCompare(Value, "1") == 0 || StringCompare(Value, "true") == 0 ||
		StringCompare(Value, "yes") == 0 || StringCompare(Value, "on") == 0)
		return True;
	if (StringCompare(Value, "0") == 0 || StringCompare(Value, "false") == 0 ||
		StringCompare(Value, "no") == 0 || StringCompare(Value, "off") == 0)
		return False;
	return Fallback;
}

static void Trim(char *Text)
{
	Size Len = StringLength(Text);

	Size Start = 0;
	while (Start < Len && (Text[Start] == ' ' || Text[Start] == '\t'))
		Start++;

	if (Start > 0) {
		Len -= Start;
		for (Size i = 0; i <= Len; i++)
			Text[i] = Text[i + Start];
	}

	while (Len > 0 && (Text[Len - 1] == ' ' || Text[Len - 1] == '\t' ||
					   Text[Len - 1] == '\r' || Text[Len - 1] == '\n')) {
		Text[--Len] = '\0';
	}
}

static void ParseLine(char *Line, WmConfig *Cfg)
{
	Size Len = StringLength(Line);

	Size Eq = 0;
	while (Eq < Len && Line[Eq] != '=')
		Eq++;

	if (Eq == 0 || Eq >= Len)
		return;

	Line[Eq] = '\0';
	char *Key = Line;
	char *Val = Line + Eq + 1;

	Trim(Key);
	Trim(Val);

	if (StringCompare(Key, "wallpaper") == 0) {
		StringCopy(Cfg->WallpaperPath, CONFIG_PATH_MAX, Val);
		Cfg->WallpaperValid = True;
	} else if (StringCompare(Key, "cursor") == 0) {
		StringCopy(Cfg->CursorPath, CONFIG_PATH_MAX, Val);
		Cfg->CursorValid = True;
	} else if (StringCompare(Key, "skin") == 0) {
		StringCopy(Cfg->SkinPath, CONFIG_PATH_MAX, Val);
		Cfg->SkinValid = True;
	} else if (StringCompare(Key, "taskbar_skin") == 0) {
		StringCopy(Cfg->TaskbarSkinPath, CONFIG_PATH_MAX, Val);
		Cfg->TaskbarSkinValid = True;
	} else if (StringCompare(Key, "text_theme") == 0) {
		Cfg->TextTheme = ParseThemeName(Val);
	} else if (StringCompare(Key, "raw_theme") == 0) {
		Cfg->RawTheme = ParseThemeName(Val);
	} else if (StringCompare(Key, "text_skin") == 0) {
		StringCopy(Cfg->TextSkinPath, CONFIG_PATH_MAX, Val);
		Cfg->TextSkinValid = True;
	} else if (StringCompare(Key, "raw_skin") == 0) {
		StringCopy(Cfg->RawSkinPath, CONFIG_PATH_MAX, Val);
		Cfg->RawSkinValid = True;
	} else if (StringCompare(Key, "text_opacity") == 0) {
		Cfg->TextOpacity = ParseOpacityValue(Val, Cfg->TextOpacity);
	} else if (StringCompare(Key, "raw_opacity") == 0) {
		Cfg->RawOpacity = ParseOpacityValue(Val, Cfg->RawOpacity);
	} else if (StringCompare(Key, "show_fps") == 0) {
		Cfg->ShowFps = ParseBoolValue(Val, Cfg->ShowFps);
	}
}

void ConfigInit(WmConfig *Cfg)
{
	if (!Cfg)
		return;

	Cfg->WallpaperValid = False;
	Cfg->CursorValid = False;
	Cfg->SkinValid = False;
	Cfg->TaskbarSkinValid = False;
	Cfg->TextSkinValid = False;
	Cfg->RawSkinValid = False;
	Cfg->WallpaperPath[0] = '\0';
	Cfg->CursorPath[0] = '\0';
	Cfg->SkinPath[0] = '\0';
	Cfg->TaskbarSkinPath[0] = '\0';
	Cfg->TextSkinPath[0] = '\0';
	Cfg->RawSkinPath[0] = '\0';
	Cfg->TextTheme = AdkWindowThemeSlate;
	Cfg->RawTheme = AdkWindowThemeForest;
	Cfg->TextOpacity = 255u;
	Cfg->RawOpacity = 255u;
	Cfg->ShowFps = False;
}

int ConfigLoad(const char *Path, WmConfig *Cfg)
{
	if (!Cfg)
		return -1;

	Handle Fd = FileOpen(Path);
	if (Fd == FileInvalid)
		return -1;

	for (;;) {
		char Line[256];
		SSize Read = FileRead(Fd, Line, sizeof(Line) - 1);

		if (Read <= 0)
			break;

		Line[Read] = '\0';

		Size Pos = 0;
		while (Pos < (Size)Read) {
			Size Start = Pos;

			while (Pos < (Size)Read && Line[Pos] != '\n')
				Pos++;

			Line[Pos] = '\0';

			Trim(Line + Start);
			if (Line[Start] != '\0' && Line[Start] != '#')
				ParseLine(Line + Start, Cfg);

			if (Pos < (Size)Read)
				Pos++;
		}
	}

	FileClose(Fd);
	return 0;
}
