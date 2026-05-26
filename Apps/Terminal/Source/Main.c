#include <Adk/AdkMain.h>
#include <Lib/String.h>
#include <System/Console.h>
#include <System/Fileio.h>
#include <System/Process.h>

#define TERMINAL_PATH_MAX 256u
#define TERMINAL_LINE_MAX 128u
#define TERMINAL_INPUT_MAX 96u
#define TERMINAL_HISTORY_MAX 96u
typedef struct {
	char CurrentPath[TERMINAL_PATH_MAX];
	char Input[TERMINAL_INPUT_MAX];
	Size InputLength;
	char Lines[TERMINAL_HISTORY_MAX][TERMINAL_LINE_MAX];
	Size LineCount;
	Bool NeedsRedraw;
	char OutputCarry[TERMINAL_LINE_MAX];
	Size OutputCarryLength;
	Handle RunningProcess;
	Bool WaitingForProcess;
} TerminalState;

static TerminalState Terminal;
static AdkAppContext *TerminalContext;

static void TerminalPushLine(const char *Text)
{
	if (!Text)
		Text = "";

	if (Terminal.LineCount == TERMINAL_HISTORY_MAX) {
		for (Size Index = 1; Index < TERMINAL_HISTORY_MAX; Index++)
			StringCopy(Terminal.Lines[Index - 1], TERMINAL_LINE_MAX,
					   Terminal.Lines[Index]);
		Terminal.LineCount--;
	}

	StringCopy(Terminal.Lines[Terminal.LineCount], TERMINAL_LINE_MAX, Text);
	Terminal.LineCount++;
	Terminal.NeedsRedraw = True;
	if (TerminalContext)
		AdkAppPostLine(TerminalContext, Text);
}

static Bool TerminalPathStartsWith(const char *Path, const char *Prefix)
{
	Size Index = 0;
	while (Prefix[Index]) {
		if (Path[Index] != Prefix[Index])
			return False;
		Index++;
	}
	return True;
}

static Bool TerminalPathEndsWith(const char *Path, const char *Suffix)
{
	Size PathLen = StringLength(Path);
	Size SuffixLen = StringLength(Suffix);

	if (PathLen < SuffixLen)
		return False;

	return StringCompare(Path + PathLen - SuffixLen, Suffix) == 0;
}

static Bool TerminalShouldLaunchViaAether(const char *Path)
{
	return TerminalPathStartsWith(Path, "a:/Applications/") &&
		   TerminalPathEndsWith(Path, ".app");
}

static void TerminalConsumeConsoleText(const char *Text, Size Length)
{
	for (Size Index = 0; Index < Length; Index++) {
		char Ch = Text[Index];
		if (Ch == '\r')
			continue;
		if (Ch == '\b') {
			if (Terminal.OutputCarryLength > 0) {
				Terminal.OutputCarryLength--;
				Terminal.OutputCarry[Terminal.OutputCarryLength] = '\0';
			}
			continue;
		}
		if (Ch == '\n') {
			Terminal.OutputCarry[Terminal.OutputCarryLength] = '\0';
			TerminalPushLine(Terminal.OutputCarry);
			Terminal.OutputCarryLength = 0;
			Terminal.OutputCarry[0] = '\0';
			if (TerminalContext && Terminal.WaitingForProcess)
				AdkAppUpdatePrompt(TerminalContext, "", "");
			continue;
		}
		if (Terminal.OutputCarryLength + 1 >= TERMINAL_LINE_MAX) {
			Terminal.OutputCarry[Terminal.OutputCarryLength] = '\0';
			TerminalPushLine(Terminal.OutputCarry);
			Terminal.OutputCarryLength = 0;
		}
		Terminal.OutputCarry[Terminal.OutputCarryLength++] = Ch;
		Terminal.OutputCarry[Terminal.OutputCarryLength] = '\0';
	}
	if (TerminalContext && Terminal.WaitingForProcess)
		AdkAppUpdatePrompt(TerminalContext, "", Terminal.OutputCarry);
}

static void TerminalAppendUnsigned(char *Buffer, Size BufferSize, U64 Value)
{
	char Digits[24];
	Size Count = 0;

	if (Value == 0) {
		StringAppend(Buffer, BufferSize, "0");
		return;
	}

	while (Value > 0 && Count < sizeof(Digits)) {
		Digits[Count++] = (char)('0' + (Value % 10));
		Value /= 10;
	}

	while (Count > 0) {
		char Ch[2];
		Ch[0] = Digits[--Count];
		Ch[1] = '\0';
		StringAppend(Buffer, BufferSize, Ch);
	}
}

static void TerminalResolvePath(const char *Input, char Out[TERMINAL_PATH_MAX])
{
	if (Input == 0 || Input[0] == '\0') {
		StringCopy(Out, TERMINAL_PATH_MAX, Terminal.CurrentPath);
		return;
	}

	if (Input[1] == ':' && Input[2] == '/') {
		StringCopy(Out, TERMINAL_PATH_MAX, Input);
		return;
	}

	if (Input[0] == '/') {
		StringCopy(Out, TERMINAL_PATH_MAX, "a:");
		StringAppend(Out, TERMINAL_PATH_MAX, Input);
		return;
	}

	StringCopy(Out, TERMINAL_PATH_MAX, Terminal.CurrentPath);
	if (Out[StringLength(Out) - 1u] != '/')
		StringAppend(Out, TERMINAL_PATH_MAX, "/");
	StringAppend(Out, TERMINAL_PATH_MAX, Input);
}

static char *TerminalNextToken(char **Cursor)
{
	char *Start;

	if (!Cursor || !*Cursor)
		return 0;

	while (**Cursor == ' ' || **Cursor == '\t')
		++(*Cursor);
	if (**Cursor == '\0')
		return 0;

	Start = *Cursor;
	while (**Cursor && **Cursor != ' ' && **Cursor != '\t')
		++(*Cursor);
	if (**Cursor) {
		**Cursor = '\0';
		++(*Cursor);
	}
	return Start;
}

static void TerminalCommandHelp(const char *Arg)
{
	(void)Arg;
	TerminalPushLine("help list go read run where clear quit");
}

static void TerminalCommandWhere(const char *Arg)
{
	(void)Arg;
	TerminalPushLine(Terminal.CurrentPath);
}

static void TerminalCommandClear(const char *Arg)
{
	(void)Arg;
	Terminal.LineCount = 0;
	Terminal.OutputCarryLength = 0;
	Terminal.OutputCarry[0] = '\0';
	Terminal.NeedsRedraw = True;
	if (TerminalContext)
		AdkAppClearTerminal(TerminalContext);
}

static void TerminalCommandGo(const char *Arg)
{
	char Path[TERMINAL_PATH_MAX];
	Handle Dir;

	if (!Arg) {
		TerminalPushLine("go: missing path");
		return;
	}

	TerminalResolvePath(Arg, Path);
	Dir = DirOpen(Path);
	if (Dir == DirInvalid) {
		TerminalPushLine("go: not a directory");
		return;
	}

	DirClose(Dir);
	StringCopy(Terminal.CurrentPath, sizeof(Terminal.CurrentPath), Path);
	if (Terminal.CurrentPath[StringLength(Terminal.CurrentPath) - 1u] != '/')
		StringAppend(Terminal.CurrentPath, sizeof(Terminal.CurrentPath), "/");
	TerminalPushLine(Terminal.CurrentPath);
}

static void TerminalCommandList(const char *Arg)
{
	char Path[TERMINAL_PATH_MAX];
	DirEntry Entry;
	Handle Dir;

	TerminalResolvePath(Arg, Path);
	Dir = DirOpen(Path);
	if (Dir == DirInvalid) {
		TerminalPushLine("list: unable to open directory");
		return;
	}

	while (DirRead(Dir, &Entry) == 1) {
		char Buffer[TERMINAL_LINE_MAX];
		StringCopy(Buffer, sizeof(Buffer), "  ");
		StringAppend(Buffer, sizeof(Buffer), Entry.Name);
		if (Entry.Flags & DirEntryDirectory)
			StringAppend(Buffer, sizeof(Buffer), "/");
		StringAppend(Buffer, sizeof(Buffer), " [");
		TerminalAppendUnsigned(Buffer, sizeof(Buffer), Entry.Size);
		StringAppend(Buffer, sizeof(Buffer), " bytes]");
		TerminalPushLine(Buffer);
	}

	DirClose(Dir);
}

static void TerminalCommandRead(const char *Arg)
{
	char Path[TERMINAL_PATH_MAX];
	char Buffer[96];
	Handle File;

	if (!Arg) {
		TerminalPushLine("read: missing path");
		return;
	}

	TerminalResolvePath(Arg, Path);
	File = FileOpen(Path);
	if (File == FileInvalid) {
		TerminalPushLine("read: unable to open file");
		return;
	}

	for (;;) {
		SSize Read = FileRead(File, Buffer, sizeof(Buffer) - 1);
		if (Read <= 0)
			break;
		Buffer[Read] = '\0';
		TerminalPushLine(Buffer);
	}

	FileClose(File);
}

static void TerminalCommandRun(const char *Arg)
{
	char Path[TERMINAL_PATH_MAX];
	Handle Process;

	if (!Arg) {
		TerminalPushLine("run: missing path");
		return;
	}
	if (Terminal.WaitingForProcess) {
		TerminalPushLine("run: process already active");
		return;
	}

	TerminalResolvePath(Arg, Path);
	if (TerminalShouldLaunchViaAether(Path)) {
		if (!TerminalContext || !AdkAppSpawnGraphical(TerminalContext, Path)) {
			return;
		}
		return;
	}

	Process = ProcessSpawn(Path);
	if (Process == (Handle)-1) {
		TerminalPushLine("run: spawn failed");
		return;
	}

	Terminal.RunningProcess = Process;
	Terminal.WaitingForProcess = True;
}

static void TerminalExecuteCommand(AdkAppContext *Context)
{
	char Command[TERMINAL_INPUT_MAX];
	char *Cursor;
	char *Name;
	char *Arg;
	char PromptLine[TERMINAL_LINE_MAX];

	StringCopy(Command, sizeof(Command), Terminal.Input);
	StringCopy(PromptLine, sizeof(PromptLine), Terminal.CurrentPath);
	StringAppend(PromptLine, sizeof(PromptLine), "> ");
	StringAppend(PromptLine, sizeof(PromptLine), Command);
	TerminalPushLine(PromptLine);

	Cursor = Command;
	Name = TerminalNextToken(&Cursor);
	Arg = TerminalNextToken(&Cursor);

	if (!Name) {
		Terminal.Input[0] = '\0';
		Terminal.InputLength = 0;
		return;
	}

	if (StringCompare(Name, "help") == 0)
		TerminalCommandHelp(Arg);
	else if (StringCompare(Name, "list") == 0)
		TerminalCommandList(Arg);
	else if (StringCompare(Name, "go") == 0)
		TerminalCommandGo(Arg);
	else if (StringCompare(Name, "read") == 0)
		TerminalCommandRead(Arg);
	else if (StringCompare(Name, "run") == 0)
		TerminalCommandRun(Arg);
	else if (StringCompare(Name, "where") == 0)
		TerminalCommandWhere(Arg);
	else if (StringCompare(Name, "clear") == 0)
		TerminalCommandClear(Arg);
	else if (StringCompare(Name, "quit") == 0)
		AdkAppRequestExit(Context);
	else
		TerminalPushLine("unknown command");

	Terminal.Input[0] = '\0';
	Terminal.InputLength = 0;
	Terminal.NeedsRedraw = True;
	if (TerminalContext)
		AdkAppUpdatePrompt(TerminalContext, Terminal.CurrentPath,
						   Terminal.Input);
}

static Bool TerminalInitialize(AdkAppContext *Context)
{
	TerminalContext = Context;
	ConsoleSetTarget(ProcessCurrentId());
	StringCopy(Terminal.CurrentPath, sizeof(Terminal.CurrentPath), "a:/");
	Terminal.Input[0] = '\0';
	Terminal.InputLength = 0;
	Terminal.LineCount = 0;
	Terminal.NeedsRedraw = True;
	Terminal.OutputCarryLength = 0;
	Terminal.OutputCarry[0] = '\0';
	Terminal.RunningProcess = 0;
	Terminal.WaitingForProcess = False;
	TerminalPushLine("avOS Terminal");
	TerminalPushLine("Type help for commands");
	AdkAppUpdatePrompt(Context, Terminal.CurrentPath, Terminal.Input);
	return True;
}

static void TerminalUpdate(AdkAppContext *Context)
{
	char Buffer[TERMINAL_LINE_MAX];
	S64 ExitCode;

	(void)Context;
	if (!Terminal.WaitingForProcess)
		return;

	ExitCode = ProcessPollExit(Terminal.RunningProcess);
	if (ExitCode == ProcessExitCodeNone)
		return;

	if (Terminal.OutputCarryLength > 0) {
		Terminal.OutputCarry[Terminal.OutputCarryLength] = '\0';
		TerminalPushLine(Terminal.OutputCarry);
		Terminal.OutputCarryLength = 0;
		Terminal.OutputCarry[0] = '\0';
	}

	Terminal.WaitingForProcess = False;
	Terminal.RunningProcess = 0;
	StringCopy(Buffer, sizeof(Buffer), "exit ");
	TerminalAppendUnsigned(Buffer, sizeof(Buffer), (U64)ExitCode);
	TerminalPushLine(Buffer);
	if (TerminalContext)
		AdkAppUpdatePrompt(TerminalContext, Terminal.CurrentPath,
						   Terminal.Input);
}

static void TerminalPulse(AdkAppContext *Context, const PulseMessage *Message)
{
	(void)Context;
	if (!Message)
		return;
	if (Message->Type == CONSOLE_PULSE_WRITE &&
		Message->Size >= sizeof(ConsolePulseWrite)) {
		const ConsolePulseWrite *Write =
			(const ConsolePulseWrite *)Message->Payload;
		if (Write->Length > CONSOLE_PULSE_TEXT_MAX)
			return;
		TerminalConsumeConsoleText(Write->Text, Write->Length);
	}
}

static void TerminalCharInput(AdkAppContext *Context, char Ch)
{
	if (Terminal.WaitingForProcess) {
		if (Terminal.RunningProcess != 0) {
			ProcessWriteConsoleInput((ProcessId)Terminal.RunningProcess, &Ch,
									 1);
		}
		return;
	}

	if (Ch == '\r' || Ch == '\n') {
		TerminalExecuteCommand(Context);
		return;
	}

	if (Ch == '\b') {
		if (Terminal.InputLength > 0) {
			Terminal.InputLength--;
			Terminal.Input[Terminal.InputLength] = '\0';
			Terminal.NeedsRedraw = True;
			AdkAppUpdatePrompt(Context, Terminal.CurrentPath, Terminal.Input);
		}
		return;
	}

	if ((U8)Ch < 32 || Terminal.InputLength + 1 >= TERMINAL_INPUT_MAX)
		return;

	Terminal.Input[Terminal.InputLength++] = Ch;
	Terminal.Input[Terminal.InputLength] = '\0';
	Terminal.NeedsRedraw = True;
	AdkAppUpdatePrompt(Context, Terminal.CurrentPath, Terminal.Input);
}

int main(void)
{
	AdkApplication Application;

	Application.Title = "avOS Terminal";
	Application.WindowWidth = 780;
	Application.WindowHeight = 520;
	Application.WindowKind = AdkAetherWindowKindText;
	Application.Initialize = TerminalInitialize;
	Application.Update = TerminalUpdate;
	Application.CharInput = TerminalCharInput;
	Application.Pulse = TerminalPulse;
	Application.Shutdown = 0;

	return AdkRunApplication(&Application);
}
