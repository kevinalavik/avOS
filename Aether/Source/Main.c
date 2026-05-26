#include <Lib/String.h>
#include <Lib/Stdio.h>
#include <System/Device.h>
#include <System/Fileio.h>
#include <System/Memory.h>
#include <System/Process.h>
#include <System/Pulse.h>
#include <System/Memory.h>
#include <System/Time.h>
#include <System/Types.h>
#include <Aether/Fb.h>
#include <Aether/Config.h>
#include <Aether/Mouse.h>
#include <Aether/Background.h>
#include <Aether/Cursor.h>
#include <Aether/Event.h>
#include <Aether/WinMgr.h>
#include <Aether/Render.h>
#include <Adk/AdkAether.h>
#include <Adk/AdkBackground.h>
#include <Adk/AdkFont.h>
#include <Adk/AdkImage.h>
#include <Adk/AdkObjectManager.h>
#include <Adk/AdkWindow.h>

#define CONFIG_DIR "a:/Config/Aether/"
#define APPLICATIONS_DIR "a:/Applications/"
#define TERMINAL_APP_PATH "a:/Applications/Terminal.app"
#define KBD_CTRL_GET_COUNT 3
#define KBD_CTRL_IS_KEY_DOWN_BASE 0x10000u
#define MOUSE_CTRL_GET_COUNT 2

#define KBD_KEY_ESCAPE 0x001Bu
#define KBD_KEY_BACKSPACE 0x0008u
#define KBD_KEY_TAB 0x0009u
#define KBD_KEY_ENTER 0x000Au
#define KBD_KEY_SPACE 0x0020u
#define KBD_KEY_LEFT_SHIFT 0x0100u
#define KBD_KEY_RIGHT_SHIFT 0x0101u
#define KBD_KEY_LEFT_CTRL 0x0102u
#define KBD_KEY_RIGHT_CTRL 0x0103u
#define KBD_KEY_LEFT_ALT 0x0104u
#define KBD_KEY_RIGHT_ALT 0x0105u
#define KBD_KEY_UP 0x0110u
#define KBD_KEY_DOWN 0x0111u
#define KBD_KEY_LEFT 0x0112u
#define KBD_KEY_RIGHT 0x0113u
#define KBD_MOD_SHIFT 0x01u
#define KBD_MOD_CTRL 0x02u
#define KBD_MOD_ALT 0x04u
#define KBD_MOD_CAPS 0x08u
#define TERMINAL_MARGIN 12
#define TERMINAL_ROW_HEIGHT 11
#define TERMINAL_HISTORY_MAX 96u
#define AETHER_TARGET_FPS 60u
#define AETHER_MAX_CLIENTS 8u
#define AETHER_MAX_DESKTOP_APPS 16u
#define AETHER_MAX_PENDING_SPAWNS 16u
#define AETHER_APP_NAME_MAX 64u
#define AETHER_DESKTOP_ICON_SIZE 40u
#define AETHER_DESKTOP_CELL_WIDTH 92u
#define AETHER_DESKTOP_CELL_HEIGHT 74u
#define AETHER_DESKTOP_MARGIN_X 20
#define AETHER_DESKTOP_MARGIN_Y 24
#define AETHER_DESKTOP_DOUBLE_CLICK_DIVISOR 2u

typedef struct {
	ProcessId ProcessId;
	int AppIndex;
	Bool Used;
} AetherPendingSpawn;

typedef struct {
	U16 Key;
	Bool Down;
} AetherTrackedKey;

typedef struct {
	char Name[AETHER_APP_NAME_MAX];
	char BundlePath[CONFIG_PATH_MAX];
	char ExecPath[CONFIG_PATH_MAX];
	char LogoPath[CONFIG_PATH_MAX];
	AdkImage *Logo;
	AdkRect Bounds;
	Bool Valid;
} AetherDesktopApp;

typedef struct {
	ProcessId ProcessId;
	int WindowIndex;
	U32 WindowKind;
	char Lines[TERMINAL_HISTORY_MAX][ADK_AETHER_LINE_MAX];
	Size LineCount;
	char CurrentPath[ADK_AETHER_CWD_MAX];
	char Input[ADK_AETHER_INPUT_MAX];
	int DesktopAppIndex;
	Bool Active;
	Bool SurfaceDirty;
	Bool RawDirty;
	Bool PresentDirty;
} AetherClient;

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

static Size StrLen(const char *S)
{
	Size N = 0;
	while (S[N])
		N++;
	return N;
}

static void ResolvePath(const char *In, char *Out, Size OutSize)
{
	if (In[0] && In[1] == ':') {
		StringCopy(Out, OutSize, In);
	} else if (In[0] == '/') {
		Out[0] = 'a';
		Out[1] = ':';
		StringCopy(Out + 2, OutSize - 2, In);
	} else {
		Size DirLen = StrLen(CONFIG_DIR);
		StringCopy(Out, OutSize, CONFIG_DIR);
		StringCopy(Out + DirLen, OutSize - DirLen, In);
	}
}

static Bool PathEndsWith(const char *Path, const char *Suffix)
{
	Size PathLen = StrLen(Path);
	Size SuffixLen = StrLen(Suffix);

	if (PathLen < SuffixLen)
		return False;
	return StringCompare(Path + PathLen - SuffixLen, Suffix) == 0;
}

static void TrimText(char *Text)
{
	Size Len;
	Size Start = 0;

	if (!Text)
		return;

	Len = StringLength(Text);
	while (Start < Len && (Text[Start] == ' ' || Text[Start] == '\t'))
		Start++;
	if (Start > 0) {
		for (Size Index = 0; Index + Start <= Len; Index++)
			Text[Index] = Text[Index + Start];
		Len -= Start;
	}

	while (Len > 0 && (Text[Len - 1] == ' ' || Text[Len - 1] == '\t' ||
					   Text[Len - 1] == '\r' || Text[Len - 1] == '\n')) {
		Text[--Len] = '\0';
	}
}

static void JoinPath(const char *BaseDir, const char *Item, char *Out,
					 Size OutSize)
{
	Size BaseLen;

	if (!Item || !Out || OutSize == 0) {
		return;
	}
	if (Item[0] && Item[1] == ':') {
		StringCopy(Out, OutSize, Item);
		return;
	}
	if (Item[0] == '/') {
		Out[0] = 'a';
		Out[1] = ':';
		StringCopy(Out + 2, OutSize - 2, Item);
		return;
	}

	StringCopy(Out, OutSize, BaseDir);
	BaseLen = StringLength(Out);
	if (BaseLen > 0 && Out[BaseLen - 1] != '/')
		StringAppend(Out, OutSize, "/");
	StringAppend(Out, OutSize, Item);
}

static void BundleNameToAppName(const char *BundlePath, char *Out, Size OutSize)
{
	const char *Name = BundlePath;
	Size Start = 0;
	Size End;

	for (Size Index = 0; BundlePath[Index]; Index++) {
		if (BundlePath[Index] == '/')
			Start = Index + 1;
	}

	Name = BundlePath + Start;
	End = StringLength(Name);
	if (End >= 4 && StringCompare(Name + End - 4, ".app") == 0)
		End -= 4;
	if (End >= OutSize)
		End = OutSize - 1;

	for (Size Index = 0; Index < End; Index++)
		Out[Index] = Name[Index];
	Out[End] = '\0';
}

static Bool ReadTextFile(const char *Path, char **DataOut)
{
	Handle File;
	Offset FileLength;
	char *Data;
	SSize Read;

	if (!Path || !DataOut)
		return False;

	*DataOut = 0;
	File = FileOpen(Path);
	if (File == FileInvalid)
		return False;

	FileLength = FileSize(File);
	Data = malloc((Size)FileLength + 1);
	if (!Data) {
		FileClose(File);
		return False;
	}

	Read = FileRead(File, Data, (Size)FileLength);
	FileClose(File);
	if (Read < 0) {
		free(Data);
		return False;
	}

	Data[Read] = '\0';
	*DataOut = Data;
	return True;
}

static Bool ReadBundleInfo(const char *BundlePath, char *NameOut, Size NameSize,
						   char *ExecOut, Size ExecSize, char *LogoOut,
						   Size LogoSize)
{
	char InfoPath[CONFIG_PATH_MAX];
	char *Buffer;
	char *Cursor;

	if (!BundlePath || !ExecOut || ExecSize == 0)
		return False;

	if (NameOut && NameSize > 0)
		BundleNameToAppName(BundlePath, NameOut, NameSize);
	if (LogoOut && LogoSize > 0)
		LogoOut[0] = '\0';
	ExecOut[0] = '\0';

	JoinPath(BundlePath, "Info", InfoPath, sizeof(InfoPath));
	if (!ReadTextFile(InfoPath, &Buffer))
		return False;

	Cursor = Buffer;
	while (*Cursor) {
		char *Line = Cursor;
		char *Eq;

		while (*Cursor && *Cursor != '\n')
			Cursor++;
		if (*Cursor == '\n') {
			*Cursor = '\0';
			Cursor++;
		}

		TrimText(Line);
		if (Line[0] == '\0' || Line[0] == '#')
			continue;

		Eq = Line;
		while (*Eq && *Eq != '=')
			Eq++;
		if (*Eq != '=')
			continue;

		*Eq = '\0';
		Eq++;
		TrimText(Line);
		TrimText(Eq);

		if (StringCompare(Line, "name") == 0 && NameOut && NameSize > 0) {
			StringCopy(NameOut, NameSize, Eq);
		} else if (StringCompare(Line, "exec") == 0) {
			JoinPath(BundlePath, Eq, ExecOut, ExecSize);
		} else if (StringCompare(Line, "logo") == 0 && LogoOut &&
				   LogoSize > 0) {
			JoinPath(BundlePath, Eq, LogoOut, LogoSize);
		}
	}

	free(Buffer);
	return ExecOut[0] != '\0';
}

static ProcessId SpawnBundleApp(const char *BundlePath)
{
	char Name[AETHER_APP_NAME_MAX];
	char ExecPath[CONFIG_PATH_MAX];
	char LogoPath[CONFIG_PATH_MAX];

	if (!BundlePath)
		return ProcessInvalid;
	if (!PathEndsWith(BundlePath, ".app"))
		return ProcessSpawn(BundlePath);
	if (!ReadBundleInfo(BundlePath, Name, sizeof(Name), ExecPath,
						sizeof(ExecPath), LogoPath, sizeof(LogoPath)))
		return ProcessInvalid;
	return ProcessSpawn(ExecPath);
}

static int FindDesktopAppByBundlePath(const AetherDesktopApp *Apps,
									  U32 AppCount, const char *BundlePath)
{
	for (U32 Index = 0; Index < AppCount; Index++) {
		if (!Apps[Index].Valid)
			continue;
		if (StringCompare(Apps[Index].BundlePath, BundlePath) == 0)
			return (int)Index;
	}

	return -1;
}

static void DesktopAppReset(AetherDesktopApp *App)
{
	for (Size Index = 0; Index < sizeof(*App); Index++)
		((Byte *)App)[Index] = 0;
}

static char AppIconLetter(const char *Name)
{
	char Letter = '?';

	if (Name && Name[0] != '\0')
		Letter = Name[0];
	if (Letter >= 'a' && Letter <= 'z')
		Letter = (char)(Letter - ('a' - 'A'));
	return Letter;
}

static U32 AppIconBackgroundColor(const char *Name)
{
	static const U32 Palette[] = { 0xFF2A6F97u, 0xFF8D5A97u, 0xFFB56576u,
								   0xFF4F772Du, 0xFFBC6C25u, 0xFF355070u,
								   0xFF7A8B99u, 0xFF457B9Du };
	U32 Hash = 0;

	if (Name) {
		for (Size Index = 0; Name[Index] != '\0'; Index++)
			Hash = Hash * 33u + (U32)(Byte)Name[Index];
	}
	return Palette[Hash % (sizeof(Palette) / sizeof(Palette[0]))];
}

static void DrawDesktopAppIcon(AdkCanvas *Canvas, const AetherDesktopApp *App,
							   const AdkRect *IconRect)
{
	if (App->Logo) {
		AdkCanvasDrawImageScaled(Canvas, App->Logo, IconRect);
		return;
	}

	AdkCanvasFillRect(Canvas, IconRect, AppIconBackgroundColor(App->Name));
	AdkCanvasFrameRect(Canvas, IconRect, 0x30FFFFFFu, 1);
	{
		char LetterText[2] = { AppIconLetter(App->Name), '\0' };
		S32 LetterX = IconRect->X +
					  ((S32)IconRect->Width - (S32)ADK_FONT_GLYPH_WIDTH) / 2;
		S32 LetterY = IconRect->Y +
					  ((S32)IconRect->Height - (S32)ADK_FONT_GLYPH_HEIGHT) / 2;
		AdkFontDrawText(Canvas, LetterX, LetterY, LetterText, 0xFFFFFFFFu);
	}
}

static void AetherPendingSpawnReset(AetherPendingSpawn *Pending)
{
	if (!Pending)
		return;

	Pending->ProcessId = ProcessInvalid;
	Pending->AppIndex = -1;
	Pending->Used = False;
}

static void AetherInitPendingSpawns(AetherPendingSpawn *PendingSpawns)
{
	for (U32 Index = 0; Index < AETHER_MAX_PENDING_SPAWNS; Index++)
		AetherPendingSpawnReset(&PendingSpawns[Index]);
}

static U8 AetherKeyboardModifiers(void)
{
	U8 Mods = 0;
	Size LeftShift = DeviceControl("kbd", KBD_CTRL_IS_KEY_DOWN_BASE +
								KBD_KEY_LEFT_SHIFT, 0);
	Size RightShift = DeviceControl("kbd", KBD_CTRL_IS_KEY_DOWN_BASE +
								 KBD_KEY_RIGHT_SHIFT, 0);
	Size LeftCtrl = DeviceControl("kbd", KBD_CTRL_IS_KEY_DOWN_BASE +
							  KBD_KEY_LEFT_CTRL, 0);
	Size RightCtrl = DeviceControl("kbd", KBD_CTRL_IS_KEY_DOWN_BASE +
							   KBD_KEY_RIGHT_CTRL, 0);
	Size LeftAlt = DeviceControl("kbd", KBD_CTRL_IS_KEY_DOWN_BASE +
							 KBD_KEY_LEFT_ALT, 0);
	Size RightAlt = DeviceControl("kbd", KBD_CTRL_IS_KEY_DOWN_BASE +
							  KBD_KEY_RIGHT_ALT, 0);

	if ((LeftShift != (Size)-1 && LeftShift > 0) ||
		(RightShift != (Size)-1 && RightShift > 0))
		Mods |= KBD_MOD_SHIFT;
	if ((LeftCtrl != (Size)-1 && LeftCtrl > 0) ||
		(RightCtrl != (Size)-1 && RightCtrl > 0))
		Mods |= KBD_MOD_CTRL;
	if ((LeftAlt != (Size)-1 && LeftAlt > 0) ||
		(RightAlt != (Size)-1 && RightAlt > 0))
		Mods |= KBD_MOD_ALT;

	return Mods;
}

static void AetherPollKeyboardState(EventQueue *Eq, AetherTrackedKey *Keys,
									U32 KeyCount, Bool *GotData)
{
	U8 Mods = AetherKeyboardModifiers();

	for (U32 Index = 0; Index < KeyCount; ++Index) {
		Size Result = DeviceControl("kbd", KBD_CTRL_IS_KEY_DOWN_BASE +
								 Keys[Index].Key, 0);
		if (Result == (Size)-1)
			continue;

		Bool Down = Result > 0 ? True : False;
		if (Down == Keys[Index].Down)
			continue;

		Keys[Index].Down = Down;

		Event Evt;
		Evt.Type = Down ? EVENT_KEY_DOWN : EVENT_KEY_UP;
		Evt.X = 0;
		Evt.Y = 0;
		Evt.Buttons = 0;
		Evt.Char = 0;
		Evt.Key = Keys[Index].Key;
		Evt.Modifiers = Mods;
		EventQueuePush(Eq, &Evt);
		if (GotData)
			*GotData = True;
	}
}

static void AetherRegisterPendingSpawn(AetherPendingSpawn *PendingSpawns,
									   ProcessId ProcessId, int AppIndex)
{
	if (!PendingSpawns || ProcessId == ProcessInvalid)
		return;

	for (U32 Index = 0; Index < AETHER_MAX_PENDING_SPAWNS; Index++) {
		if (PendingSpawns[Index].Used &&
			PendingSpawns[Index].ProcessId == ProcessId) {
			PendingSpawns[Index].AppIndex = AppIndex;
			return;
		}
	}

	for (U32 Index = 0; Index < AETHER_MAX_PENDING_SPAWNS; Index++) {
		if (!PendingSpawns[Index].Used) {
			PendingSpawns[Index].Used = True;
			PendingSpawns[Index].ProcessId = ProcessId;
			PendingSpawns[Index].AppIndex = AppIndex;
			return;
		}
	}
}

static int AetherConsumePendingSpawn(AetherPendingSpawn *PendingSpawns,
									 ProcessId ProcessId)
{
	if (!PendingSpawns)
		return -1;

	for (U32 Index = 0; Index < AETHER_MAX_PENDING_SPAWNS; Index++) {
		if (PendingSpawns[Index].Used &&
			PendingSpawns[Index].ProcessId == ProcessId) {
			int AppIndex = PendingSpawns[Index].AppIndex;
			AetherPendingSpawnReset(&PendingSpawns[Index]);
			return AppIndex;
		}
	}

	return -1;
}

static U32 LoadDesktopApps(AdkObjectManager *Mgr, AetherDesktopApp *Apps,
						   U32 MaxApps, U32 ScreenWidth, U32 ScreenHeight)
{
	Handle Dir;
	DirEntry Entry;
	U32 Count = 0;
	U32 ColumnCount;
	U32 RowCount;
	U32 MaxVisibleApps;

	for (U32 Index = 0; Index < MaxApps; Index++)
		DesktopAppReset(&Apps[Index]);

	Dir = DirOpen(APPLICATIONS_DIR);
	if (Dir == DirInvalid)
		return 0;

	ColumnCount =
		(ScreenWidth - AETHER_DESKTOP_MARGIN_X) / AETHER_DESKTOP_CELL_WIDTH;
	if (ColumnCount == 0)
		ColumnCount = 1;
	RowCount =
		(ScreenHeight - AETHER_DESKTOP_MARGIN_Y) / AETHER_DESKTOP_CELL_HEIGHT;
	if (RowCount == 0)
		RowCount = 1;
	MaxVisibleApps = ColumnCount * RowCount;
	if (MaxVisibleApps < MaxApps)
		MaxApps = MaxVisibleApps;

	while (Count < MaxApps && DirRead(Dir, &Entry) == 1) {
		char BundlePath[CONFIG_PATH_MAX];
		U32 Row;
		U32 Col;

		if (!(Entry.Flags & DirEntryDirectory))
			continue;
		if (!PathEndsWith(Entry.Name, ".app"))
			continue;

		JoinPath(APPLICATIONS_DIR, Entry.Name, BundlePath, sizeof(BundlePath));
		if (!ReadBundleInfo(BundlePath, Apps[Count].Name,
							sizeof(Apps[Count].Name), Apps[Count].ExecPath,
							sizeof(Apps[Count].ExecPath), Apps[Count].LogoPath,
							sizeof(Apps[Count].LogoPath)))
			continue;

		StringCopy(Apps[Count].BundlePath, sizeof(Apps[Count].BundlePath),
				   BundlePath);
		if (Apps[Count].LogoPath[0] != '\0')
			Apps[Count].Logo = AdkImageLoadTga(Mgr, Apps[Count].LogoPath);
		Apps[Count].Valid = True;

		Row = Count / ColumnCount;
		Col = Count % ColumnCount;
		Apps[Count].Bounds.X =
			AETHER_DESKTOP_MARGIN_X + (S32)(Col * AETHER_DESKTOP_CELL_WIDTH);
		Apps[Count].Bounds.Y =
			AETHER_DESKTOP_MARGIN_Y + (S32)(Row * AETHER_DESKTOP_CELL_HEIGHT);
		Apps[Count].Bounds.Width = AETHER_DESKTOP_CELL_WIDTH - 12;
		Apps[Count].Bounds.Height = AETHER_DESKTOP_CELL_HEIGHT - 4;
		Count++;
	}

	DirClose(Dir);
	return Count;
}

static void DrawDesktopApps(AdkCanvas *Canvas, const AetherDesktopApp *Apps,
							U32 AppCount, int HoveredIndex)
{
	for (U32 Index = 0; Index < AppCount; Index++) {
		AdkRect ItemRect;
		AdkRect IconRect;
		AdkRect LabelBack;
		S32 LabelX;
		S32 LabelY;

		if (!Apps[Index].Valid)
			continue;

		ItemRect = Apps[Index].Bounds;
		if ((int)Index == HoveredIndex)
			AdkCanvasFillRect(Canvas, &ItemRect, 0x35FFFFFFu);

		IconRect.Width = AETHER_DESKTOP_ICON_SIZE;
		IconRect.Height = AETHER_DESKTOP_ICON_SIZE;
		IconRect.X = Apps[Index].Bounds.X +
					 ((S32)Apps[Index].Bounds.Width - (S32)IconRect.Width) / 2;
		IconRect.Y = Apps[Index].Bounds.Y;
		DrawDesktopAppIcon(Canvas, &Apps[Index], &IconRect);

		LabelBack.X = Apps[Index].Bounds.X + 4;
		LabelBack.Y = IconRect.Y + (S32)IconRect.Height + 8;
		LabelBack.Width = Apps[Index].Bounds.Width - 8;
		LabelBack.Height = ADK_FONT_GLYPH_HEIGHT + 4;
		AdkCanvasFillRect(Canvas, &LabelBack, 0x70000000u);

		{
			Size LabelLen = StringLength(Apps[Index].Name);
			S32 TextWidth = (S32)(LabelLen * ADK_FONT_GLYPH_WIDTH);
			LabelX = LabelBack.X + ((S32)LabelBack.Width - TextWidth) / 2;
			if (LabelX < LabelBack.X + 2)
				LabelX = LabelBack.X + 2;
		}
		LabelY = LabelBack.Y + 2;
		AdkFontDrawText(Canvas, LabelX + 1, LabelY + 1, Apps[Index].Name,
						0xFF000000u);
		AdkFontDrawText(Canvas, LabelX, LabelY, Apps[Index].Name, 0xFFFFFFFFu);
	}
}

static int HitTestDesktopApp(const AetherDesktopApp *Apps, U32 AppCount, S32 X,
							 S32 Y)
{
	for (U32 Index = 0; Index < AppCount; Index++) {
		const AdkRect *Bounds = &Apps[Index].Bounds;

		if (!Apps[Index].Valid)
			continue;
		if (X >= Bounds->X && X < Bounds->X + (S32)Bounds->Width &&
			Y >= Bounds->Y && Y < Bounds->Y + (S32)Bounds->Height)
			return (int)Index;
	}

	return -1;
}

static void RebuildDesktopBase(AetherRenderer *Rdr,
							   const AetherDesktopApp *DesktopApps,
							   U32 DesktopAppCount, int HoveredDesktopApp)
{
	AdkCanvas Canvas;

	if (!Rdr)
		return;

	MemCopy(Rdr->CleanBuf, Rdr->BackBuf, Rdr->BufSize);
	AdkCanvasBind(&Canvas, Rdr->CleanBuf, Rdr->FbWidth, Rdr->FbHeight,
				  Rdr->FbPitch / 4);
	DrawDesktopApps(&Canvas, DesktopApps, DesktopAppCount, HoveredDesktopApp);
}

static void AetherClientPushLine(AetherClient *Client, const char *Text)
{
	if (Client->LineCount == TERMINAL_HISTORY_MAX) {
		for (Size Index = 1; Index < TERMINAL_HISTORY_MAX; Index++) {
			StringCopy(Client->Lines[Index - 1], ADK_AETHER_LINE_MAX,
					   Client->Lines[Index]);
		}
		Client->LineCount--;
	}

	StringCopy(Client->Lines[Client->LineCount], ADK_AETHER_LINE_MAX, Text);
	Client->LineCount++;
}

static void AetherClientReset(AetherClient *Client)
{
	for (Size Index = 0; Index < sizeof(*Client); Index++)
		((Byte *)Client)[Index] = 0;
	Client->WindowIndex = -1;
	Client->DesktopAppIndex = -1;
}

static AetherClient *AetherFindClientByProcessId(AetherClient *Clients,
												 ProcessId ProcessId)
{
	for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++) {
		if (Clients[Index].Active && Clients[Index].ProcessId == ProcessId)
			return &Clients[Index];
	}
	return 0;
}

static AetherClient *AetherFindClientByWindowIndex(AetherClient *Clients,
												   int WindowIndex)
{
	for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++) {
		if (Clients[Index].Active && Clients[Index].WindowIndex == WindowIndex)
			return &Clients[Index];
	}
	return 0;
}


static Bool AetherFocusedWindowAcceptsText(AetherClient *Clients,
                                           int FocusedIndex)
{
	AetherClient *Client =
		AetherFindClientByWindowIndex(Clients, FocusedIndex);

	if (Client == 0 || Client->WindowIndex < 0)
		return False;

	return Client->WindowKind != AdkAetherWindowKindRaw;
}

static void AetherClearPresentDirty(AetherClient *Clients)
{
	for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++) {
		Clients[Index].RawDirty = False;
		Clients[Index].PresentDirty = False;
	}
}

static AetherClient *AetherAllocClientSlot(AetherClient *Clients,
										   ProcessId ProcessId)
{
	AetherClient *Client = AetherFindClientByProcessId(Clients, ProcessId);
	if (Client)
		return Client;

	for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++) {
		if (!Clients[Index].Active) {
			AetherClientReset(&Clients[Index]);
			Clients[Index].Active = True;
			Clients[Index].ProcessId = ProcessId;
			return &Clients[Index];
		}
	}

	return 0;
}

static void AetherAdjustClientWindowsAfterDestroy(AetherClient *Clients,
												  int DestroyedIndex)
{
	for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++) {
		if (!Clients[Index].Active)
			continue;
		if (Clients[Index].WindowIndex == DestroyedIndex)
			Clients[Index].WindowIndex = -1;
		else if (Clients[Index].WindowIndex > DestroyedIndex)
			Clients[Index].WindowIndex--;
	}
}

static void AetherAdjustClientWindowsAfterBringToFront(AetherClient *Clients,
													   int MovedIndex,
													   int WindowCount)
{
	int NewIndex = WindowCount - 1;

	if (MovedIndex < 0 || WindowCount <= 0 || MovedIndex >= WindowCount ||
		MovedIndex == NewIndex)
		return;

	for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++) {
		if (!Clients[Index].Active)
			continue;
		if (Clients[Index].WindowIndex == MovedIndex) {
			Clients[Index].WindowIndex = NewIndex;
		} else if (Clients[Index].WindowIndex > MovedIndex &&
				   Clients[Index].WindowIndex < WindowCount) {
			Clients[Index].WindowIndex--;
		}
	}
}

static void AetherReapExitedClients(AetherClient *Clients, AetherWinMgr *WinMgr,
									Bool *Dirty, Bool *FullRedrawNeeded)
{
	for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++) {
		AetherClient *Client = &Clients[Index];
		S64 ExitCode;

		if (!Client->Active)
			continue;

		ExitCode = ProcessPollExit(Client->ProcessId);
		if (ExitCode == ProcessExitCodeNone)
			continue;

		Printf("[Aether] reaping pid=%u exit=%u window=%d\n",
			   (U64)Client->ProcessId, (U64)ExitCode, (U64)Client->WindowIndex);

		if (Client->WindowIndex >= 0 &&
			Client->WindowIndex < WinMgr->WindowCount) {
			AetherWinMgrDestroyWindow(WinMgr, Client->WindowIndex);
			AetherAdjustClientWindowsAfterDestroy(Clients, Client->WindowIndex);
			if (Dirty)
				*Dirty = True;
			if (FullRedrawNeeded)
				*FullRedrawNeeded = True;
		}

		AetherClientReset(Client);
	}
}

static void AetherDrawTerminalWindow(AdkWindow *Wnd, const AetherClient *Client)
{
	AdkCanvas Canvas;
	AdkRect FullRect = { 0, 0, Wnd->Surface.Width, Wnd->Surface.Height };
	AdkRect CaretRect;
	AdkRect PromptBack;
	char PromptLine[ADK_AETHER_CWD_MAX + ADK_AETHER_INPUT_MAX + 4];
	S32 TextY = TERMINAL_MARGIN;
	U32 MaxVisibleRows =
		(Wnd->Surface.Height - (TERMINAL_MARGIN * 2)) / TERMINAL_ROW_HEIGHT;
	Size StartIndex = 0;
	Size PromptLen;
	U32 VisibleHistoryRows;

	AdkCanvasBind(&Canvas, Wnd->Buffer, Wnd->Surface.Width, Wnd->Surface.Height,
				  Wnd->Surface.Pitch);

	if (MaxVisibleRows == 0)
		MaxVisibleRows = 1;
	VisibleHistoryRows = MaxVisibleRows > 0 ? MaxVisibleRows - 1 : 0;
	if (Client->LineCount > VisibleHistoryRows)
		StartIndex = Client->LineCount - VisibleHistoryRows;

	AdkCanvasFillRect(&Canvas, &FullRect, 0xFF0B1118u);

	for (Size Index = StartIndex; Index < Client->LineCount; Index++) {
		AdkFontDrawText(&Canvas, TERMINAL_MARGIN, TextY, Client->Lines[Index],
						0xFFD7E4F2u);
		TextY += TERMINAL_ROW_HEIGHT;
	}

	if (Client->CurrentPath[0] != '\0') {
		StringCopy(PromptLine, sizeof(PromptLine), Client->CurrentPath);
		StringAppend(PromptLine, sizeof(PromptLine), "> ");
		StringAppend(PromptLine, sizeof(PromptLine), Client->Input);
	} else {
		StringCopy(PromptLine, sizeof(PromptLine), Client->Input);
	}
	PromptBack.X = TERMINAL_MARGIN - 4;
	PromptBack.Y = TextY - 1;
	PromptBack.Width = Wnd->Surface.Width - (TERMINAL_MARGIN * 2) + 8;
	PromptBack.Height = TERMINAL_ROW_HEIGHT;
	AdkCanvasFillRect(&Canvas, &PromptBack, 0xFF101A24u);
	AdkFontDrawText(&Canvas, TERMINAL_MARGIN, TextY, PromptLine, 0xFFFFFFFFu);

	PromptLen = StringLength(PromptLine);
	CaretRect.X = TERMINAL_MARGIN + (S32)(PromptLen * ADK_FONT_GLYPH_WIDTH);
	CaretRect.Y = TextY - 1;
	CaretRect.Width = 2;
	CaretRect.Height = ADK_FONT_GLYPH_HEIGHT + 1;
	AdkCanvasFillRect(&Canvas, &CaretRect, 0xFF8CB7FFu);
}

static U32 WindowFrameHeightForClient(U32 ClientHeight)
{
	return ClientHeight + ADK_WINDOW_TITLE_BAR_HEIGHT + ADK_WINDOW_BORDER_SIZE;
}

static Bool RectIsEmpty(const AdkRect *Rect)
{
	return !Rect || Rect->Width == 0 || Rect->Height == 0;
}

static AdkRect RectUnion(const AdkRect *A, const AdkRect *B)
{
	AdkRect Result;
	S32 Left;
	S32 Top;
	S32 Right;
	S32 Bottom;

	if (RectIsEmpty(A))
		return *B;
	if (RectIsEmpty(B))
		return *A;

	Left = A->X < B->X ? A->X : B->X;
	Top = A->Y < B->Y ? A->Y : B->Y;
	Right = A->X + (S32)A->Width > B->X + (S32)B->Width ? A->X + (S32)A->Width :
														  B->X + (S32)B->Width;
	Bottom = A->Y + (S32)A->Height > B->Y + (S32)B->Height ?
				 A->Y + (S32)A->Height :
				 B->Y + (S32)B->Height;

	Result.X = Left;
	Result.Y = Top;
	Result.Width = (U32)(Right - Left);
	Result.Height = (U32)(Bottom - Top);
	return Result;
}

static void AccumulateDamageRect(AdkRect *DamageRect, Bool *HasDamageRect,
								 const AdkRect *Rect)
{
	if (!DamageRect || !HasDamageRect || RectIsEmpty(Rect))
		return;
	if (!*HasDamageRect) {
		*DamageRect = *Rect;
		*HasDamageRect = True;
		return;
	}
	*DamageRect = RectUnion(DamageRect, Rect);
}

int main(void)
{
	Framebuffer Fb;
	AdkObjectManager ObjMgr;
	WmConfig Cfg;
	char Path[CONFIG_PATH_MAX];
	EventQueue Eq;
	Event Evt;
	AetherClient *Clients;
	Bool LoggedMouseInput = False;
	Bool LoggedTerminalHandshake = False;
	Bool Dirty = True;
	Bool FullRedrawNeeded = True;
	Bool TerminalSurfaceDirty = False;
	Bool DesktopDirty = True;
	U64 TickFrequency;
	U64 FpsWindowStart;
	U64 PresentInterval;
	U64 NextPresentTick;
	U32 FramesInWindow = 0;
	U32 DisplayFps = 0;
	char OverlayText[32];
	AetherDesktopApp DesktopApps[AETHER_MAX_DESKTOP_APPS];
	AetherPendingSpawn PendingSpawns[AETHER_MAX_PENDING_SPAWNS];
	U32 DesktopAppCount = 0;
	int HoveredDesktopApp = -1;
	int LastClickedDesktopApp = -1;
	U64 LastDesktopClickTick = 0;
	AdkRect DamageRect = { 0, 0, 0, 0 };
	Bool HasDamageRect = False;
	AetherTrackedKey TrackedKeys[] = {
		{ KBD_KEY_ESCAPE, False },
		{ KBD_KEY_TAB, False },
		{ KBD_KEY_ENTER, False },
		{ KBD_KEY_SPACE, False },
		{ KBD_KEY_UP, False },
		{ KBD_KEY_DOWN, False },
		{ KBD_KEY_LEFT, False },
		{ KBD_KEY_RIGHT, False },
		{ KBD_KEY_LEFT_SHIFT, False },
		{ KBD_KEY_RIGHT_SHIFT, False },
		{ KBD_KEY_LEFT_CTRL, False },
		{ KBD_KEY_RIGHT_CTRL, False },
		{ KBD_KEY_LEFT_ALT, False },
		{ KBD_KEY_RIGHT_ALT, False },
		{ 'w', False },
		{ 'a', False },
		{ 's', False },
		{ 'd', False },
		{ 'j', False },
		{ 'k', False },
		{ 'l', False },
	};
	U32 TrackedKeyCount =
		(U32)(sizeof(TrackedKeys) / sizeof(TrackedKeys[0]));

	Clients = zalloc(sizeof(AetherClient) * AETHER_MAX_CLIENTS);
	if (!Clients)
		return 1;
	for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++)
		AetherClientReset(&Clients[Index]);

	AdkObjectManagerInit(&ObjMgr);
	EventQueueInit(&Eq);
	AetherInitPendingSpawns(PendingSpawns);

	AetherWinMgr *WinMgr = zalloc(sizeof(AetherWinMgr));

	if (FbInit(&Fb) != 0)
		return 1;
	Printf("[Aether] fb %ux%u pitch=%u\n", (U64)Fb.Width, (U64)Fb.Height,
		   (U64)Fb.Pitch);

	AetherWinMgrInit(WinMgr, &ObjMgr, Fb.Width, Fb.Height);
	ConfigInit(&Cfg);

	AdkImage *WallpaperImg = 0;
	AdkImage *CursorImg = 0;
	AdkImage *WindowSkinImg = 0;
	AetherCursor Cursor;
	AetherCursorInit(&Cursor, CursorImg);

	AetherBackground Background;
	AdkBackgroundInit(&Background, WallpaperImg, 0xFF10151Cu);

	Size BufSize = Fb.BufSize;
	U32 *BackBuf = malloc(BufSize);
	if (!BackBuf)
		return 1;

	AdkCanvas BackgroundCanvas;
	AdkCanvasBind(&BackgroundCanvas, BackBuf, Fb.Width, Fb.Height,
				  Fb.Pitch / 4);
	AetherBackgroundDraw(&Background, &BackgroundCanvas);

	U32 *CleanBuf = malloc(BufSize);
	if (!CleanBuf) {
		free(BackBuf);
		return 1;
	}
	MemCopy(CleanBuf, BackBuf, BufSize);

	AetherRenderer *Rdr = zalloc(sizeof(AetherRenderer));
	Rdr->FbBase = (volatile U32 *)Fb.Base;
	Rdr->FbPitch = Fb.Pitch;
	Rdr->FbWidth = Fb.Width;
	Rdr->FbHeight = Fb.Height;
	Rdr->SceneBuf = malloc(BufSize);
	Rdr->BackBuf = BackBuf;
	Rdr->CleanBuf = CleanBuf;
	Rdr->BufSize = BufSize;
	Rdr->Background = &Background;
	Rdr->Cursor = &Cursor;
	Rdr->WinMgr = WinMgr;
	Rdr->OverlayText[0] = '\0';
	Rdr->HasPrevCursor = False;
	if (!Rdr->SceneBuf)
		return 1;

	TickFrequency = TimeFrequency();
	if (TickFrequency == 0)
		TickFrequency = 100;
#if AETHER_TARGET_FPS == 0
	PresentInterval = 0;
#else
	{
		PresentInterval = TickFrequency / AETHER_TARGET_FPS;
		if (PresentInterval == 0)
			PresentInterval = 1;
	}
#endif
	FpsWindowStart = TimeTicks();
	NextPresentTick =
		FpsWindowStart + (PresentInterval == 0 ? 0 : PresentInterval);
	OverlayText[0] = '\0';
	Rdr->OverlayText[0] = '\0';

	AetherRenderFull(Rdr);
	Printf("[Aether] wm online\n");

	Printf("[Aether] loading config\n");
	ConfigLoad("a:/Config/Aether/Aether.cfg", &Cfg);
	Printf("[Aether] config loaded\n");
	if (Cfg.ShowFps) {
		StringCopy(OverlayText, sizeof(OverlayText), "FPS 0");
		StringCopy(Rdr->OverlayText, sizeof(Rdr->OverlayText), OverlayText);
	}
	if (Cfg.WallpaperValid) {
		Printf("[Aether] loading wallpaper\n");
		ResolvePath(Cfg.WallpaperPath, Path, sizeof(Path));
		WallpaperImg = AdkImageLoadTga(&ObjMgr, Path);
		Printf("[Aether] wallpaper loaded=%u\n", WallpaperImg != 0);
	}
	if (Cfg.CursorValid) {
		Printf("[Aether] loading cursor\n");
		ResolvePath(Cfg.CursorPath, Path, sizeof(Path));
		CursorImg = AdkImageLoadTga(&ObjMgr, Path);
		Printf("[Aether] cursor loaded=%u\n", CursorImg != 0);
	}
	if (Cfg.SkinValid) {
		Printf("[Aether] loading window skin\n");
		ResolvePath(Cfg.SkinPath, Path, sizeof(Path));
		WindowSkinImg = AdkImageLoadTga(&ObjMgr, Path);
		Printf("[Aether] window skin loaded=%u\n", WindowSkinImg != 0);
	}

	Background.Image = WallpaperImg;
	Cursor.Image = CursorImg;
	DesktopAppCount = LoadDesktopApps(
		&ObjMgr, DesktopApps, AETHER_MAX_DESKTOP_APPS, Fb.Width, Fb.Height);

	AdkCanvasBind(&BackgroundCanvas, BackBuf, Fb.Width, Fb.Height,
				  Fb.Pitch / 4);
	AetherBackgroundDraw(&Background, &BackgroundCanvas);
	RebuildDesktopBase(Rdr, DesktopApps, DesktopAppCount, HoveredDesktopApp);

	MousePacket Mpk;
	U8 PrevButtons = 0;
	AetherRenderFull(Rdr);

	for (;;) {
		Bool GotData = False;
		Bool PointerChanged = False;
		U64 NowTicks = TimeTicks();

		while (DeviceControl("mouse", MOUSE_CTRL_GET_COUNT, 0) > 0) {
			DeviceRead("mouse", &Mpk, sizeof(Mpk));
			GotData = True;
			if (!LoggedMouseInput) {
				Printf("[Aether] mouse input online\n");
				LoggedMouseInput = True;
			}

			if ((S32)Mpk.X != Cursor.X || (S32)Mpk.Y != Cursor.Y) {
				Evt.Type = EVENT_MOUSE_MOVE;
				Evt.X = (S32)Mpk.X;
				Evt.Y = (S32)Mpk.Y;
				Evt.Buttons = Mpk.Buttons;
				EventQueuePush(&Eq, &Evt);
				PointerChanged = True;
			}

			if (Mpk.Buttons && !PrevButtons) {
				Evt.Type = EVENT_MOUSE_DOWN;
				Evt.X = (S32)Mpk.X;
				Evt.Y = (S32)Mpk.Y;
				Evt.Buttons = Mpk.Buttons;
				EventQueuePush(&Eq, &Evt);
			} else if (!Mpk.Buttons && PrevButtons) {
				Evt.Type = EVENT_MOUSE_UP;
				Evt.X = (S32)Mpk.X;
				Evt.Y = (S32)Mpk.Y;
				Evt.Buttons = Mpk.Buttons;
				EventQueuePush(&Eq, &Evt);
			} else if (Mpk.Buttons && Mpk.Buttons == PrevButtons) {
				Evt.Type = EVENT_MOUSE_HOLD;
				Evt.X = (S32)Mpk.X;
				Evt.Y = (S32)Mpk.Y;
				Evt.Buttons = Mpk.Buttons;
				EventQueuePush(&Eq, &Evt);
			}

			PrevButtons = Mpk.Buttons;
			Cursor.X = (S32)Mpk.X;
			Cursor.Y = (S32)Mpk.Y;
		}
		if (PointerChanged)
			Dirty = True;

		AetherPollKeyboardState(&Eq, TrackedKeys, TrackedKeyCount, &GotData);

		{
			Size KbdCount;
			Bool TextTarget =
				AetherFocusedWindowAcceptsText(Clients, WinMgr->FocusedIndex);

			while ((KbdCount = DeviceControl("kbd", KBD_CTRL_GET_COUNT, 0)) >
				   0 && KbdCount != (Size)-1) {
				char KbdBuf[64];
				if (KbdCount > sizeof(KbdBuf))
					KbdCount = sizeof(KbdBuf);
				KbdCount = DeviceRead("kbd", KbdBuf, KbdCount);
				if (KbdCount == 0 || KbdCount == (Size)-1)
					break;

				GotData = True;

				if (!TextTarget)
					continue;

				for (Size ki = 0; ki < KbdCount; ki++) {
					Evt.Type = EVENT_KEY_DOWN;
					Evt.X = 0;
					Evt.Y = 0;
					Evt.Buttons = 0;
					Evt.Char = KbdBuf[ki];
					Evt.Key = (U16)(U8)KbdBuf[ki];
					Evt.Modifiers = AetherKeyboardModifiers();
					EventQueuePush(&Eq, &Evt);
				}
			}
		}

		while (EventQueuePop(&Eq, &Evt)) {
			switch (Evt.Type) {
			case EVENT_MOUSE_DOWN: {
				U64 ClickTicks = TimeTicks();
				AdkWindowPart Part;
				int HitIndex =
					AetherWinMgrHitTest(WinMgr, Evt.X, Evt.Y, &Part, 0, 0);
				if (HitIndex < 0) {
					int AppIndex = HitTestDesktopApp(
						DesktopApps, DesktopAppCount, Evt.X, Evt.Y);
					if (AppIndex >= 0) {
						if (AppIndex == LastClickedDesktopApp &&
							ClickTicks - LastDesktopClickTick <=
								TickFrequency /
									AETHER_DESKTOP_DOUBLE_CLICK_DIVISOR) {
							ProcessId SpawnedPid = SpawnBundleApp(
								DesktopApps[AppIndex].BundlePath);
							if (SpawnedPid != ProcessInvalid) {
								AetherRegisterPendingSpawn(
									PendingSpawns, SpawnedPid, AppIndex);
								Dirty = True;
								FullRedrawNeeded = True;
							}
							LastClickedDesktopApp = -1;
							LastDesktopClickTick = 0;
						} else {
							LastClickedDesktopApp = AppIndex;
							LastDesktopClickTick = ClickTicks;
						}
					} else {
						LastClickedDesktopApp = -1;
					}
					break;
				}

				if (Part == AdkWindowPartCloseButton) {
					AetherClient *Client =
						AetherFindClientByWindowIndex(Clients, HitIndex);
					if (Client) {
						PulseSend(Client->ProcessId, AdkAetherMessageClose, 0,
								  0);
						Client->Active = False;
						Client->WindowIndex = -1;
					}
					AetherWinMgrDestroyWindow(WinMgr, HitIndex);
					AetherAdjustClientWindowsAfterDestroy(Clients, HitIndex);
					Dirty = True;
					FullRedrawNeeded = True;
				} else if (Part == AdkWindowPartTitleBar) {
					AetherAdjustClientWindowsAfterBringToFront(
						Clients, HitIndex, WinMgr->WindowCount);
					AetherWinMgrBringToFront(WinMgr, HitIndex);
					HitIndex = WinMgr->FocusedIndex;
					AetherWinMgrBeginDrag(WinMgr, HitIndex, Evt.X, Evt.Y);
					Dirty = True;
					FullRedrawNeeded = True;
				} else {
					AetherAdjustClientWindowsAfterBringToFront(
						Clients, HitIndex, WinMgr->WindowCount);
					AetherWinMgrBringToFront(WinMgr, HitIndex);
					Dirty = True;
					FullRedrawNeeded = True;
				}
				break;
			}

			case EVENT_MOUSE_UP:
				AetherWinMgrEndDrag(WinMgr);
				Dirty = True;
				FullRedrawNeeded = True;
				break;

			case EVENT_MOUSE_MOVE:
			case EVENT_MOUSE_HOLD: {
				int NewHoveredDesktopApp = -1;
				AdkRect OldBounds;
				AdkRect NewBounds;

				if (WinMgr->DragIndex >= 0 &&
					WinMgr->DragIndex < WinMgr->WindowCount &&
					WinMgr->Windows[WinMgr->DragIndex]) {
					OldBounds =
						AdkWindowGetBounds(WinMgr->Windows[WinMgr->DragIndex]);
				} else {
					OldBounds = (AdkRect){ 0, 0, 0, 0 };
				}
				AetherWinMgrUpdateDrag(WinMgr, Evt.X, Evt.Y);
				if (WinMgr->DragIndex >= 0 &&
					WinMgr->DragIndex < WinMgr->WindowCount &&
					WinMgr->Windows[WinMgr->DragIndex]) {
					NewBounds =
						AdkWindowGetBounds(WinMgr->Windows[WinMgr->DragIndex]);
					AccumulateDamageRect(&DamageRect, &HasDamageRect,
										 &OldBounds);
					AccumulateDamageRect(&DamageRect, &HasDamageRect,
										 &NewBounds);
				}
				if (WinMgr->DragIndex < 0 &&
					AetherWinMgrHitTest(WinMgr, Evt.X, Evt.Y, 0, 0, 0) < 0) {
					NewHoveredDesktopApp = HitTestDesktopApp(
						DesktopApps, DesktopAppCount, Evt.X, Evt.Y);
				}
				if (NewHoveredDesktopApp != HoveredDesktopApp) {
					HoveredDesktopApp = NewHoveredDesktopApp;
					DesktopDirty = True;
				}
				Dirty = True;
				if (DesktopDirty)
					FullRedrawNeeded = True;
				break;
			}

			case EVENT_KEY_DOWN:
			case EVENT_KEY_UP: {
				AetherClient *Client = AetherFindClientByWindowIndex(
					Clients, WinMgr->FocusedIndex);
				if (Client && Client->WindowIndex >= 0) {
					AdkAetherKeyInputMessage Msg;
					Msg.Char = Evt.Char;
					Msg.Key = Evt.Key;
					Msg.Pressed = Evt.Type == EVENT_KEY_DOWN ? 1 : 0;
					Msg.Modifiers = Evt.Modifiers;
					PulseSend(Client->ProcessId, AdkAetherMessageKeyInput, &Msg,
							  sizeof(Msg));
				}
			} break;

			default:
				break;
			}
		}

		for (;;) {
			PulseMessage Message;
			S64 Status = PulseReceive(&Message);
			if (Status <= 0)
				break;

			AetherClient *Client =
				AetherAllocClientSlot(Clients, (ProcessId)Message.SenderId);
			if (Client == 0)
				continue;

			if (Message.Type == AdkAetherMessageCreateWindow &&
				Message.Size >= sizeof(AdkAetherCreateWindowMessage)) {
				AdkAetherCreateWindowMessage *Create =
					(AdkAetherCreateWindowMessage *)Message.Payload;
				if (!LoggedTerminalHandshake) {
					Printf("[Aether] client %u requested window '%s'\n",
						   (U64)Message.SenderId, Create->Title);
					LoggedTerminalHandshake = True;
				}
				int Index = AetherWinMgrCreateWindow(
					WinMgr, 120, 80, Create->Width,
					WindowFrameHeightForClient(Create->Height), Create->Title,
					0xFF0E1621u);
				Client->WindowIndex = Index;
				Client->WindowKind = Create->Kind;
				{
					int PendingAppIndex = AetherConsumePendingSpawn(
						PendingSpawns, (ProcessId)Message.SenderId);
					if (PendingAppIndex >= 0)
						Client->DesktopAppIndex = PendingAppIndex;
				}
				Client->LineCount = 0;
				Client->CurrentPath[0] = '\0';
				Client->Input[0] = '\0';
				Client->SurfaceDirty = True;
				Client->PresentDirty = True;
				TerminalSurfaceDirty = True;
				Dirty = True;
				FullRedrawNeeded = True;

				U64 SharedMemId = 0;
				if (Index >= 0 && Create->Kind == AdkAetherWindowKindRaw) {
					U64 ShmSize = (U64)Create->Width * Create->Height * 4;
					SharedMemId = SharedMemCreate(ShmSize);
					if (SharedMemId != (U64)-1 && SharedMemId != 0) {
						U32 *ShmBuf = (U32 *)SharedMemMap(SharedMemId);
						if (ShmBuf) {
							AdkWindow *Wnd = WinMgr->Windows[Index];
							for (U64 i = 0;
								 i < (U64)Create->Width * Create->Height; i++)
								ShmBuf[i] = 0xFF1B1B22u;
							free(Wnd->Buffer);
							Wnd->Buffer = ShmBuf;
							AdkSurfaceBind(&Wnd->Surface, ShmBuf, Create->Width,
										   Create->Height, Create->Width);
							/* Record shm ID so DestroyWindow unmaps instead of
							 * calling free() on the mapping pointer (GPF fix). */
							AetherWinMgrSetWindowShmId(WinMgr, Index,
													   SharedMemId);
							Printf(
								"[Aether] raw window shm_id=%llu buf=0x%llx\n",
								(U64)SharedMemId, (U64)(U64 *)ShmBuf);
						} else {
							SharedMemId = 0;
						}
					} else {
						SharedMemId = 0;
					}
				}

				if (Index >= 0) {
					AdkWindow *Wnd = WinMgr->Windows[Index];
					if (Wnd) {
						if (Create->Kind == AdkAetherWindowKindRaw) {
							AdkWindowSetTheme(Wnd, Cfg.RawTheme);
							AdkWindowSetOpacity(Wnd, Cfg.RawOpacity);
						} else {
							AdkWindowSetTheme(Wnd, Cfg.TextTheme);
							AdkWindowSetOpacity(Wnd, Cfg.TextOpacity);
						}
						AdkWindowSetSkin(Wnd, WindowSkinImg);
					}
				}

				if (Index >= 0) {
					AdkAetherWindowReadyMessage Ready;
					Ready.WindowId = (U32)Index;
					Ready.SharedMemId = SharedMemId;
					Printf("[Aether] window ready idx=%u\n",
						   (U64)Ready.WindowId);
					PulseSend(Client->ProcessId, AdkAetherMessageWindowReady,
							  &Ready, sizeof(Ready));
				}
			} else if (Message.Type == AdkAetherMessageTerminalLine &&
					   Message.Size >= sizeof(AdkAetherTerminalLineMessage)) {
				AdkAetherTerminalLineMessage *Line =
					(AdkAetherTerminalLineMessage *)Message.Payload;
				AetherClientPushLine(Client, Line->Text);
				Client->SurfaceDirty = True;
				Client->PresentDirty = True;
				TerminalSurfaceDirty = True;
				Dirty = True;
			} else if (Message.Type == AdkAetherMessageTerminalPrompt &&
					   Message.Size >= sizeof(AdkAetherTerminalPromptMessage)) {
				AdkAetherTerminalPromptMessage *Prompt =
					(AdkAetherTerminalPromptMessage *)Message.Payload;
				StringCopy(Client->CurrentPath, sizeof(Client->CurrentPath),
						   Prompt->CurrentPath);
				StringCopy(Client->Input, sizeof(Client->Input), Prompt->Input);
				Client->SurfaceDirty = True;
				Client->PresentDirty = True;
				TerminalSurfaceDirty = True;
				Dirty = True;
			} else if (Message.Type == AdkAetherMessageTerminalClear) {
				Client->LineCount = 0;
				Client->SurfaceDirty = True;
				Client->PresentDirty = True;
				TerminalSurfaceDirty = True;
				Dirty = True;
			} else if (Message.Type == AdkAetherMessageSpawnRequest &&
					   Message.Size >= sizeof(AdkAetherSpawnRequestMessage)) {
				AdkAetherSpawnRequestMessage *Spawn =
					(AdkAetherSpawnRequestMessage *)Message.Payload;
				ProcessId SpawnedPid = SpawnBundleApp(Spawn->Path);
				if (SpawnedPid != ProcessInvalid) {
					int AppIndex = FindDesktopAppByBundlePath(
						DesktopApps, DesktopAppCount, Spawn->Path);
					AetherRegisterPendingSpawn(PendingSpawns, SpawnedPid,
											   AppIndex);
					Printf("[Aether] spawned app pid=%u path=%s\n",
						   (U64)SpawnedPid, Spawn->Path);
				}
			} else if (Message.Type == AdkAetherMessageRawFrameReady) {
				Client->RawDirty = True;
				Client->PresentDirty = True;
				Dirty = True;
			}
		}

		AetherReapExitedClients(Clients, WinMgr, &Dirty, &FullRedrawNeeded);

		while (NowTicks - FpsWindowStart >= TickFrequency) {
			DisplayFps = FramesInWindow;
			FramesInWindow = 0;
			FpsWindowStart += TickFrequency;
			if (Cfg.ShowFps) {
				StringCopy(OverlayText, sizeof(OverlayText), "FPS ");
				if (DisplayFps >= 100) {
					char Hundreds[2] = {
						(char)('0' + ((DisplayFps / 100) % 10)), 0
					};
					StringAppend(OverlayText, sizeof(OverlayText), Hundreds);
				}
				if (DisplayFps >= 10) {
					char Tens[2] = { (char)('0' + ((DisplayFps / 10) % 10)),
									 0 };
					StringAppend(OverlayText, sizeof(OverlayText), Tens);
				}
				{
					char Ones[2] = { (char)('0' + (DisplayFps % 10)), 0 };
					StringAppend(OverlayText, sizeof(OverlayText), Ones);
				}
				StringCopy(Rdr->OverlayText, sizeof(Rdr->OverlayText),
						   OverlayText);
			}
			if (Cfg.ShowFps) {
				Dirty = True;
				FullRedrawNeeded = True;
			}
		}

		if (TerminalSurfaceDirty) {
			for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++) {
				AetherClient *Client = &Clients[Index];
				if (!Client->Active || !Client->SurfaceDirty)
					continue;
				if (Client->WindowIndex < 0 ||
					Client->WindowIndex >= WinMgr->WindowCount)
					continue;
				if (Client->WindowKind == AdkAetherWindowKindText) {
					AetherDrawTerminalWindow(
						WinMgr->Windows[Client->WindowIndex], Client);
				}
				Client->SurfaceDirty = False;
			}
			TerminalSurfaceDirty = False;
		}

		NowTicks = TimeTicks();
		if (PresentInterval == 0 || NowTicks >= NextPresentTick) {
			if (DesktopDirty && FullRedrawNeeded) {
				RebuildDesktopBase(Rdr, DesktopApps, DesktopAppCount,
								   HoveredDesktopApp);
				DesktopDirty = False;
				AetherRenderFull(Rdr);
				AetherClearPresentDirty(Clients);
				Dirty = False;
				FullRedrawNeeded = False;
				HasDamageRect = False;
			} else if (WinMgr->DragIndex >= 0 && HasDamageRect) {
				AetherRenderRegion(Rdr, &DamageRect);
				AetherClearPresentDirty(Clients);
				Dirty = False;
				FullRedrawNeeded = False;
				HasDamageRect = False;
			} else if (FullRedrawNeeded) {
				AetherRenderFull(Rdr);
				AetherClearPresentDirty(Clients);
				Dirty = False;
				FullRedrawNeeded = False;
				HasDamageRect = False;
			} else {
				for (U32 Index = 0; Index < AETHER_MAX_CLIENTS; Index++) {
					AetherClient *Client = &Clients[Index];

					if (!Client->Active || !Client->PresentDirty)
						continue;
					if (Client->WindowIndex >= 0 &&
						Client->WindowIndex < WinMgr->WindowCount) {
						AetherRenderWindowClient(
							Rdr, WinMgr->Windows[Client->WindowIndex]);
					}
					Client->RawDirty = False;
					Client->PresentDirty = False;
				}
				AetherRenderCursor(Rdr);
				Dirty = False;
			}
			FramesInWindow++;

			if (PresentInterval != 0) {
				do {
					NextPresentTick += PresentInterval;
				} while (NowTicks >= NextPresentTick);
			}
		}

		if (!GotData)
			__asm__ volatile("pause" ::: "memory");
	}

	(void)Dirty;

	return 0;
}
