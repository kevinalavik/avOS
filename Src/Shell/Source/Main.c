#include <Lib/Stdio.h>
#include <Lib/String.h>
#include <System/Memory.h>
#include <System/Console.h>
#include <System/Fileio.h>
#include <System/Process.h>

#define LINE_MAX 160u
#define PATH_MAX 256u

static char CurrentPath[PATH_MAX] = "a:/";

static char *NextToken(char **Cursor)
{
	char *Start;
	if (Cursor == 0 || *Cursor == 0)
		return 0;

	while (**Cursor == ' ' || **Cursor == '\t')
		++(*Cursor);
	if (**Cursor == '\0')
		return 0;

	Start = *Cursor;
	while (**Cursor != '\0' && **Cursor != ' ' && **Cursor != '\t')
		++(*Cursor);

	if (**Cursor != '\0') {
		**Cursor = '\0';
		++(*Cursor);
	}
	return Start;
}

static void ResolvePath(const char *Input, char Out[PATH_MAX])
{
	if (Input == 0 || Input[0] == '\0') {
		StringCopy(Out, PATH_MAX, CurrentPath);
		return;
	}

	if (Input[1] == ':' && Input[2] == '/') {
		StringCopy(Out, PATH_MAX, Input);
		return;
	}

	if (Input[0] == '/') {
		StringCopy(Out, PATH_MAX, "a:");
		StringAppend(Out, PATH_MAX, Input);
		return;
	}

	StringCopy(Out, PATH_MAX, CurrentPath);
	if (Out[StringLength(Out) - 1u] != '/')
		StringAppend(Out, PATH_MAX, "/");
	StringAppend(Out, PATH_MAX, Input);
}

#define BUILTINS       \
	X(help, CmdHelp)   \
	X(list, CmdList)   \
	X(go, CmdGo)       \
	X(read, CmdRead)   \
	X(run, CmdRun)     \
	X(where, CmdWhere) \
	X(quit, CmdQuit)

#define X(name, fn) static void fn(const char *Arg);
BUILTINS
#undef X

/* ------------------------------------------------------------------ */
/*  Input line editing                                                 */
/* ------------------------------------------------------------------ */

static void ReadLine(char Line[LINE_MAX])
{
	U64 Len = 0;

	for (;;) {
		char Ch;
		U64 Read = ConsoleRead(&Ch, 1);
		if (Read == 0 || Read == (U64)-1)
			continue;
		if (Ch == '\r')
			continue;

		if (Ch == '\n') {
			Line[Len] = '\0';
			Print("\n");
			return;
		}

		if (Ch == '\b') {
			if (Len > 0) {
				--Len;
				Print("\b \b");
			}
			continue;
		}

		if (Len + 1 < LINE_MAX) {
			Line[Len++] = Ch;
			ConsoleWrite(&Ch, 1);
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Command implementations                                            */
/* ------------------------------------------------------------------ */

static void CmdHelp(const char *Arg)
{
	if (Arg == 0) {
		Print("available commands:\n");
		Print("  help   Show this help, or detailed help for a command\n");
		Print("  list   List directory contents\n");
		Print("  go     Change the current directory\n");
		Print("  read   Display file contents\n");
		Print("  run    Execute a program\n");
		Print("  where  Show the current directory path\n");
		Print("  quit   Exit the shell\n");
		return;
	}

	if (StringCompare(Arg, "help") == 0) {
		Print("usage: help [command]\n");
		Print("\n");
		Print("With no arguments, lists every command and a brief\n");
		Print("description.  With a command name, shows usage syntax\n");
		Print("and detailed information.\n");
	} else if (StringCompare(Arg, "list") == 0) {
		Print("usage: list [path]\n");
		Print("\n");
		Print("Lists the entries in a directory.  If path is omitted\n");
		Print("the current directory is used.  Directories are marked\n");
		Print("with a trailing '/' and each entry shows its name and\n");
		Print("size in bytes.\n");
	} else if (StringCompare(Arg, "go") == 0) {
		Print("usage: go <path>\n");
		Print("\n");
		Print("Sets the working directory to path.  The path must be\n");
		Print("an existing directory.  Both absolute (a:/...) and\n");
		Print("relative forms are accepted.\n");
	} else if (StringCompare(Arg, "read") == 0) {
		Print("usage: read <path>\n");
		Print("\n");
		Print("Opens the file at path and writes its entire contents\n");
		Print("to the console.  Useful for viewing short text files.\n");
	} else if (StringCompare(Arg, "run") == 0) {
		Print("usage: run <path>\n");
		Print("\n");
		Print("Loads and executes the program at path.  The shell\n");
		Print("waits for the program to finish and then prints its\n");
		Print("exit code.\n");
	} else if (StringCompare(Arg, "where") == 0) {
		Print("usage: where\n");
		Print("\n");
		Print("Prints the absolute path of the current working\n");
		Print("directory.\n");
	} else if (StringCompare(Arg, "quit") == 0) {
		Print("usage: quit\n");
		Print("\n");
		Print("Terminates the shell and returns to the caller.\n");
	} else {
		Print("no help for \"");
		Print(Arg);
		Print("\"\n");
	}
}

static void CmdList(const char *Arg)
{
	char Resolved[PATH_MAX];
	DirEntry Entry;
	U64 Dir;

	if (Arg == 0)
		StringCopy(Resolved, PATH_MAX, CurrentPath);
	else
		ResolvePath(Arg, Resolved);

	Dir = DirOpen(Resolved);
	if (Dir == DirInvalid) {
		Print("list: unable to open \"");
		Print(Resolved);
		Print("\"\n");
		return;
	}

	while (DirRead(Dir, &Entry) == 1) {
		Printf("  %s%s  [%u bytes]\n", Entry.Name,
			   (Entry.Flags & DirEntryDirectory) ? "/" : "", Entry.Size);
	}

	DirClose(Dir);
}

static void CmdGo(const char *Arg)
{
	char Resolved[PATH_MAX];
	U64 Dir;

	if (Arg == 0) {
		Print("go: missing path\n");
		Print("usage: go <path>\n");
		return;
	}

	ResolvePath(Arg, Resolved);
	Dir = DirOpen(Resolved);
	if (Dir == DirInvalid) {
		Print("go: \"");
		Print(Resolved);
		Print("\" is not a directory\n");
		return;
	}

	DirClose(Dir);
	StringCopy(CurrentPath, sizeof(CurrentPath), Resolved);
	if (CurrentPath[StringLength(CurrentPath) - 1u] != '/')
		StringAppend(CurrentPath, sizeof(CurrentPath), "/");
}

static void CmdRead(const char *Arg)
{
	char Resolved[PATH_MAX];
	char Buf[128];
	U64 File;

	if (Arg == 0) {
		Print("read: missing path\n");
		Print("usage: read <path>\n");
		return;
	}

	ResolvePath(Arg, Resolved);
	File = FileOpen(Resolved);
	if (File == FileInvalid) {
		Print("read: unable to open \"");
		Print(Resolved);
		Print("\"\n");
		return;
	}

	for (;;) {
		SSize Read = FileRead(File, Buf, sizeof(Buf));
		if (Read == 0 || Read == (SSize)-1)
			break;
		ConsoleWrite(Buf, Read);
	}

	FileClose(File);
	Print("\n");
}

static void CmdRun(const char *Arg)
{
	char Resolved[PATH_MAX];
	Handle Process;
	U64 ExitCode;

	if (Arg == 0) {
		Print("run: missing path\n");
		Print("usage: run <path>\n");
		return;
	}

	ResolvePath(Arg, Resolved);
	Process = ProcessSpawn(Resolved);
	if (Process == ProcessInvalid) {
		Print("run: failed to launch \"");
		Print(Resolved);
		Print("\"\n");
		return;
	}

	ExitCode = ProcessWait(Process);
	Printf("exit %u\n", ExitCode);
}

static void CmdWhere(const char *Arg)
{
	(void)Arg;
	Print(CurrentPath);
	Print("\n");
}

static void CmdQuit(const char *Arg)
{
	(void)Arg;
	Exit(0);
}

int main(void)
{
	char Line[LINE_MAX];

	Print("avOS shell\n");
	Print("type \"help\" for available commands.\n\n");

	for (;;) {
		char *Cursor;

		Print(CurrentPath);
		Print("> ");
		ReadLine(Line);

		Cursor = Line;
		char *Name = NextToken(&Cursor);
		if (Name == 0)
			continue;

		char *Arg = NextToken(&Cursor);

#define X(n, fn)                        \
	if (StringCompare(Name, #n) == 0) { \
		fn(Arg);                        \
		goto done;                      \
	}
		BUILTINS
#undef X

		Print("unknown command \"");
		Print(Name);
		Print("\".  type \"help\" for available commands.\n");

done:;
	}
}