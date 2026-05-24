#include <Core/Ksh.h>

#include <Core/Log.h>
#include <Drivers/Device.h>
#include <Filesystem/Vfs.h>
#include <Library/Printf.h>

#include <stddef.h>
#include <stdint.h>

#define KshFileReadMax 255u
#define KshLineMax 127u
#define KshArgMax 8u

static char KshCurrentPath[VfsPathMax];

typedef void (*KshCommandCallback)(char **ArgPointer, size_t ArgCount);

typedef struct KshCommand {
	const char *Name;
	const char *Description;
	KshCommandCallback Callback;
} KshCommand;

#define KSH_COMMAND(Name, Description, Callback) \
	{                                            \
		(Name), (Description), (Callback)        \
	}

static void KshCommandGuide(char **ArgPointer, size_t ArgCount);
static void KshCommandVolumes(char **ArgPointer, size_t ArgCount);
static void KshCommandWhere(char **ArgPointer, size_t ArgCount);
static void KshCommandGoto(char **ArgPointer, size_t ArgCount);
static void KshCommandList(char **ArgPointer, size_t ArgCount);
static void KshCommandTree(char **ArgPointer, size_t ArgCount);
static void KshCommandInfo(char **ArgPointer, size_t ArgCount);
static void KshCommandRead(char **ArgPointer, size_t ArgCount);

static const KshCommand KshCommands[] = {
	KSH_COMMAND("guide", "show this help", KshCommandGuide),
	KSH_COMMAND("volumes", "show the VFS volume registry", KshCommandVolumes),
	KSH_COMMAND("where", "show current location", KshCommandWhere),
	KSH_COMMAND("goto", "move to a folder", KshCommandGoto),
	KSH_COMMAND("list", "list a folder", KshCommandList),
	KSH_COMMAND("tree", "show a folder tree", KshCommandTree),
	KSH_COMMAND("info", "inspect a file or folder", KshCommandInfo),
	KSH_COMMAND("read", "read a text file", KshCommandRead),
};

#define KshCommandCount (sizeof(KshCommands) / sizeof(KshCommands[0]))

static size_t KshStringLength(const char *String)
{
	size_t Length = 0;

	if (String == 0) {
		return 0;
	}

	while (String[Length] != '\0') {
		++Length;
	}

	return Length;
}

static bool KshStringsEqual(const char *Left, const char *Right)
{
	size_t Index = 0;

	if (Left == 0 || Right == 0) {
		return Left == Right;
	}

	while (Left[Index] != '\0' || Right[Index] != '\0') {
		if (Left[Index] != Right[Index]) {
			return false;
		}
		++Index;
	}

	return true;
}

static bool KshCopyString(char *Destination, size_t DestinationSize,
						  const char *Source)
{
	size_t Index = 0;

	if (Destination == 0 || DestinationSize == 0 || Source == 0) {
		return false;
	}

	while (Source[Index] != '\0') {
		if (Index + 1 >= DestinationSize) {
			Destination[0] = '\0';
			return false;
		}

		Destination[Index] = Source[Index];
		++Index;
	}

	Destination[Index] = '\0';
	return true;
}

static void KshTrimSpaces(char *Text)
{
	size_t Start = 0;
	size_t End;
	size_t Write = 0;

	if (Text == 0) {
		return;
	}

	while (Text[Start] == ' ' || Text[Start] == '\t') {
		++Start;
	}

	End = KshStringLength(Text);
	while (End > Start && (Text[End - 1] == ' ' || Text[End - 1] == '\t')) {
		--End;
	}

	while (Start < End) {
		Text[Write++] = Text[Start++];
	}

	Text[Write] = '\0';
}

static size_t KshParseArguments(char *Text, char **ArgPointer,
								size_t MaxArgCount)
{
	size_t Index = 0;
	size_t ArgCount = 0;

	if (Text == 0 || ArgPointer == 0 || MaxArgCount == 0) {
		return 0;
	}

	while (Text[Index] != '\0' && ArgCount < MaxArgCount) {
		while (Text[Index] == ' ' || Text[Index] == '\t') {
			Text[Index++] = '\0';
		}

		if (Text[Index] == '\0') {
			break;
		}

		ArgPointer[ArgCount++] = &Text[Index];
		while (Text[Index] != '\0' && Text[Index] != ' ' &&
			   Text[Index] != '\t') {
			++Index;
		}
	}

	return ArgCount;
}

static void KshAppendPath(char *Destination, size_t DestinationSize,
						  const char *BasePath, const char *Name)
{
	size_t Length = 0;

	while (BasePath[Length] != '\0' && Length + 1 < DestinationSize) {
		Destination[Length] = BasePath[Length];
		++Length;
	}

	if (Length == 0) {
		Destination[Length++] = '/';
	}

	if (Length > 0 && Destination[Length - 1] != '/' &&
		Length + 1 < DestinationSize) {
		Destination[Length++] = '/';
	}

	for (size_t Index = 0; Name[Index] != '\0' && Length + 1 < DestinationSize;
		 ++Index) {
		Destination[Length++] = Name[Index];
	}

	Destination[Length] = '\0';
}

static bool KshIsVolumePath(const char *Path)
{
	return Path != 0 && (((Path[0] >= 'a' && Path[0] <= 'z') ||
						  (Path[0] >= 'A' && Path[0] <= 'Z')) &&
						 Path[1] == ':' && Path[2] == '/');
}

static char KshNormalizeVolumeId(char VolumeId)
{
	if (VolumeId >= 'A' && VolumeId <= 'Z') {
		return (char)(VolumeId - 'A' + 'a');
	}

	return VolumeId;
}

static bool KshBuildRootPath(char *Destination, size_t DestinationSize,
							 char VolumeId)
{
	if (Destination == 0 || DestinationSize < 4) {
		return false;
	}

	Destination[0] = KshNormalizeVolumeId(VolumeId);
	Destination[1] = ':';
	Destination[2] = '/';
	Destination[3] = '\0';
	return true;
}

static char KshCurrentVolumeId(void)
{
	return KshIsVolumePath(KshCurrentPath) ?
			   KshNormalizeVolumeId(KshCurrentPath[0]) :
			   '\0';
}

static bool KshBuildParentPath(char *Path)
{
	size_t Length;

	if (!KshIsVolumePath(Path)) {
		return false;
	}

	Length = KshStringLength(Path);
	if (Length <= 3) {
		return true;
	}

	while (Length > 3 && Path[Length - 1] == '/') {
		Path[--Length] = '\0';
	}

	while (Length > 3 && Path[Length - 1] != '/') {
		Path[--Length] = '\0';
	}

	if (Length > 3) {
		Path[Length] = '\0';
	}

	return true;
}

static bool KshResolvePath(char *Destination, size_t DestinationSize,
						   const char *CurrentPath, const char *InputPath)
{
	char Working[VfsPathMax];
	size_t Offset = 0;

	if (Destination == 0 || DestinationSize == 0 || CurrentPath == 0 ||
		InputPath == 0) {
		return false;
	}

	if (InputPath[0] == '\0') {
		return KshCopyString(Destination, DestinationSize, CurrentPath);
	}

	if (KshIsVolumePath(InputPath)) {
		if (!KshBuildRootPath(Working, sizeof(Working), InputPath[0])) {
			return false;
		}
	} else {
		if (!KshCopyString(Working, sizeof(Working), CurrentPath)) {
			return false;
		}

		if (InputPath[0] == '/') {
			Working[3] = '\0';
		}
	}

	if (KshIsVolumePath(InputPath)) {
		Offset = 3;
	} else if (InputPath[0] == '/') {
		Offset = 1;
	}

	while (InputPath[Offset] != '\0') {
		char Segment[128];
		size_t SegmentLength = 0;

		while (InputPath[Offset] == '/') {
			++Offset;
		}

		if (InputPath[Offset] == '\0') {
			break;
		}

		while (InputPath[Offset] != '\0' && InputPath[Offset] != '/') {
			if (SegmentLength + 1 >= sizeof(Segment)) {
				return false;
			}

			Segment[SegmentLength++] = InputPath[Offset++];
		}
		Segment[SegmentLength] = '\0';

		if (KshStringsEqual(Segment, ".")) {
			continue;
		}

		if (KshStringsEqual(Segment, "..")) {
			if (!KshBuildParentPath(Working)) {
				return false;
			}
			continue;
		}

		KshAppendPath(Working, sizeof(Working), Working, Segment);
	}

	return KshCopyString(Destination, DestinationSize, Working);
}

static void KshPrintPrompt(void)
{
	Printf("%s> ", KshCurrentPath);
}

static void KshPrintTree(const char *Path, int Depth)
{
	Directory Dir;

	if (Depth > 16) {
		Printf("depth limit hit at %s\n", Path);
		return;
	}

	if (!DirOpen(Path, &Dir)) {
		Printf("unable to open folder %s\n", Path);
		return;
	}

	VfsDirent Entry;
	while (DirRead(&Dir, &Entry)) {
		for (int Index = 0; Index < Depth; ++Index) {
			Printf("  ");
		}

		Printf("%s%s  [%u bytes]\n", Entry.Name,
			   (Entry.Stat.Flags & VfsNodeFlagDirectory) != 0 ? "/" : "",
			   (unsigned int)Entry.Stat.Size);

		if ((Entry.Stat.Flags & VfsNodeFlagDirectory) != 0) {
			char ChildPath[VfsPathMax * 2];
			KshAppendPath(ChildPath, sizeof(ChildPath), Path, Entry.Name);
			KshPrintTree(ChildPath, Depth + 1);
		}
	}

	DirClose(&Dir);
}

static void KshShowFileText(const char *Path)
{
	File Handle;

	if (!FileOpen(Path, &Handle)) {
		Printf("unable to open %s\n", Path);
		return;
	}

	size_t Size = FileSize(&Handle);
	if (Size > KshFileReadMax) {
		Size = KshFileReadMax;
	}

	char Buffer[KshFileReadMax + 1];
	size_t Read = FileRead(&Handle, Buffer, Size);
	Buffer[Read] = '\0';
	FileClose(&Handle);

	Printf("%s\n", Buffer);
}

static void KshShowDirectory(const char *Path)
{
	Directory Dir;

	if (!DirOpen(Path, &Dir)) {
		Printf("unable to open folder %s\n", Path);
		return;
	}

	Printf("list of %s\n", Path);

	VfsDirent Entry;
	while (DirRead(&Dir, &Entry)) {
		Printf("  %s%s  [%u bytes]\n", Entry.Name,
			   (Entry.Stat.Flags & VfsNodeFlagDirectory) != 0 ? "/" : "",
			   (unsigned int)Entry.Stat.Size);
	}

	DirClose(&Dir);
}

static void KshShowPathInfo(const char *Path)
{
	VfsStat Stat;

	if (!VfsStatPath(Path, &Stat)) {
		Printf("unable to inspect %s\n", Path);
		return;
	}

	Printf("%s\n", Path);
	Printf("  kind : %s\n",
		   (Stat.Flags & VfsNodeFlagDirectory) != 0 ? "folder" : "file");
	Printf("  size : %u bytes\n", (unsigned int)Stat.Size);
}

static const KshCommand *KshFindCommand(const char *Name)
{
	for (size_t Index = 0; Index < KshCommandCount; ++Index) {
		if (KshStringsEqual(KshCommands[Index].Name, Name)) {
			return &KshCommands[Index];
		}
	}

	return 0;
}

static bool KshExpectNoArgs(const char *Name, size_t ArgCount)
{
	if (ArgCount == 0) {
		return true;
	}

	Printf("%s does not take arguments\n", Name);
	return false;
}

static bool KshResolveCommandPath(char **ArgPointer, size_t ArgCount,
								  bool Required, char *Out)
{
	const char *Path = KshCurrentPath;

	if (ArgCount > 1) {
		Printf("too many arguments\n");
		return false;
	}

	if (ArgCount == 0 && Required) {
		Printf("missing path\n");
		return false;
	}

	if (ArgCount == 1) {
		Path = ArgPointer[0];
	}

	if (!KshResolvePath(Out, VfsPathMax, KshCurrentPath, Path)) {
		Printf("path is too long or invalid\n");
		return false;
	}

	return true;
}

static void KshShowVolumes(void)
{
	size_t Count = VfsGetVolumeCount();

	Printf("volume registry\n");
	for (size_t Index = 0; Index < Count; ++Index) {
		VfsVolumeInfo Volume;

		if (!VfsGetVolumeInfo(Index, &Volume)) {
			continue;
		}

		Printf("  %c:/", Volume.Id);
		if (Volume.Id == KshCurrentVolumeId()) {
			Printf("  [current]");
		}
		Printf("\n");
	}
}

static void KshCommandGuide(char **ArgPointer, size_t ArgCount)
{
	(void)ArgPointer;

	if (!KshExpectNoArgs("guide", ArgCount)) {
		return;
	}

	Printf("guide\n");
	for (size_t Index = 0; Index < KshCommandCount; ++Index) {
		Printf("  %-10s %s\n", KshCommands[Index].Name,
			   KshCommands[Index].Description);
	}
}

static void KshCommandVolumes(char **ArgPointer, size_t ArgCount)
{
	(void)ArgPointer;

	if (!KshExpectNoArgs("volumes", ArgCount)) {
		return;
	}

	KshShowVolumes();
}

static void KshCommandWhere(char **ArgPointer, size_t ArgCount)
{
	(void)ArgPointer;

	if (!KshExpectNoArgs("where", ArgCount)) {
		return;
	}

	Printf("%s\n", KshCurrentPath);
}

static void KshCommandGoto(char **ArgPointer, size_t ArgCount)
{
	char ResolvedPath[VfsPathMax];
	VfsStat Stat;

	if (!KshResolveCommandPath(ArgPointer, ArgCount, true, ResolvedPath)) {
		return;
	}

	if (!VfsStatPath(ResolvedPath, &Stat) ||
		(Stat.Flags & VfsNodeFlagDirectory) == 0) {
		Printf("unable to goto %s\n", ResolvedPath);
		return;
	}

	KshCopyString(KshCurrentPath, sizeof(KshCurrentPath), ResolvedPath);
}

static void KshCommandList(char **ArgPointer, size_t ArgCount)
{
	char ResolvedPath[VfsPathMax];

	if (!KshResolveCommandPath(ArgPointer, ArgCount, false, ResolvedPath)) {
		return;
	}

	KshShowDirectory(ResolvedPath);
}

static void KshCommandTree(char **ArgPointer, size_t ArgCount)
{
	char ResolvedPath[VfsPathMax];

	if (!KshResolveCommandPath(ArgPointer, ArgCount, false, ResolvedPath)) {
		return;
	}

	KshPrintTree(ResolvedPath, 0);
}

static void KshCommandInfo(char **ArgPointer, size_t ArgCount)
{
	char ResolvedPath[VfsPathMax];

	if (!KshResolveCommandPath(ArgPointer, ArgCount, true, ResolvedPath)) {
		return;
	}

	KshShowPathInfo(ResolvedPath);
}

static void KshCommandRead(char **ArgPointer, size_t ArgCount)
{
	char ResolvedPath[VfsPathMax];

	if (!KshResolveCommandPath(ArgPointer, ArgCount, true, ResolvedPath)) {
		return;
	}

	KshShowFileText(ResolvedPath);
}

static bool KshReadLine(Device *Keyboard, char *Buffer, size_t BufferSize)
{
	size_t Length = 0;

	if (Keyboard == 0 || Buffer == 0 || BufferSize < 2) {
		return false;
	}

	for (;;) {
		char Character = '\0';
		int64_t Read = DeviceRead(Keyboard, &Character, 1);

		if (Read <= 0) {
			__asm__ volatile("hlt");
			continue;
		}

		if (Character == '\r') {
			continue;
		}

		if (Character == '\n') {
			Buffer[Length] = '\0';
			Printf("\n");
			return true;
		}

		if (Character == '\b') {
			if (Length > 0) {
				--Length;
				Printf("\b \b");
			}
			continue;
		}

		if (Character < 32 || Character > 126) {
			continue;
		}

		if (Length + 1 >= BufferSize) {
			continue;
		}

		Buffer[Length++] = Character;
		Printf("%c", Character);
	}
}

bool KshInit(char VolumeId)
{
	return KshBuildRootPath(KshCurrentPath, sizeof(KshCurrentPath), VolumeId);
}

void KshRun(void)
{
	Device *Keyboard = DeviceGet("kbd");
	char Line[KshLineMax + 1];

	if (Keyboard == 0) {
		LogWarn("core.ksh", "keyboard unavailable; shell disabled");
		return;
	}

	Printf("\nAvKernel Shell v1.0\n");
	Printf("type 'guide' for commands.\n");

	for (;;) {
		char *ArgPointer[KshArgMax];
		size_t ArgCount;
		const KshCommand *Command;

		KshPrintPrompt();
		if (!KshReadLine(Keyboard, Line, sizeof(Line))) {
			continue;
		}

		KshTrimSpaces(Line);
		ArgCount = KshParseArguments(Line, ArgPointer, KshArgMax);
		if (ArgCount == 0) {
			continue;
		}

		Command = KshFindCommand(ArgPointer[0]);
		if (Command == 0) {
			Printf("unknown command: %s\n", ArgPointer[0]);
			Printf("type 'guide' for commands.\n");
			continue;
		}

		Command->Callback(&ArgPointer[1], ArgCount - 1);
	}
}
