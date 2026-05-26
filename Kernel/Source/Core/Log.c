#include <Core/Log.h>
#include <Library/Printf.h>
#include <Library/Stdout.h>

#include <stdarg.h>

#define ESC "\033"

static LogLevel MinLevel = LogLevelWarn;

typedef struct LogStyle {
	const char *Label;
	const char *Color;
} LogStyle;

static const LogStyle LogStyles[] = {
	[LogLevelTrace] = { "trace", ESC "[90m" },
	[LogLevelDebug] = { "debug", ESC "[94m" },
	[LogLevelInfo] = { "info", ESC "[97m" },
	[LogLevelOk] = { "ok", ESC "[92m" },
	[LogLevelWarn] = { "warn", ESC "[93m" },
	[LogLevelError] = { "error", ESC "[91m" },
	[LogLevelFatal] = { "fatal", ESC "[1;91m" },
};

#define RESET ESC "[0m"

static LogLevel NormalizeLevel(LogLevel Level)
{
	if (Level < LogLevelTrace || Level > LogLevelFatal)
		return LogLevelInfo;

	return Level;
}

static const char *FindCmdlineValue(const char *Cmdline, const char *Name)
{
	const char *Cursor = Cmdline;
	int NameLength = 0;

	while (Name[NameLength] != '\0')
		++NameLength;

	while (Cursor != 0 && *Cursor != '\0') {
		const char *TokenStart;
		int Index = 0;

		while (*Cursor == ' ' || *Cursor == '\t')
			++Cursor;

		TokenStart = Cursor;
		while (*Cursor != '\0' && *Cursor != ' ' && *Cursor != '\t' &&
			   *Cursor != '=')
			++Cursor;

		while (Index < NameLength && TokenStart[Index] == Name[Index])
			++Index;

		if (Index == NameLength &&
			(TokenStart[Index] == '\0' || TokenStart[Index] == ' ' ||
			 TokenStart[Index] == '\t' || TokenStart[Index] == '=')) {
			while (*Cursor == ' ' || *Cursor == '\t' || *Cursor == '=')
				++Cursor;
			return Cursor;
		}

		while (*Cursor != '\0' && *Cursor != ' ' && *Cursor != '\t')
			++Cursor;
	}

	return 0;
}

static int ParseLoglevel(const char *Cmdline)
{
	const char *Value = FindCmdlineValue(Cmdline, "loglevel");
	int Level = 0;

	if (Value == 0) {
		Value = FindCmdlineValue(Cmdline, "-loglevel");
	}

	if (Value == 0) {
		return -1;
	}

	if (*Value < '0' || *Value > '9') {
		return -1;
	}

	while (*Value >= '0' && *Value <= '9') {
		Level = Level * 10 + (*Value - '0');
		++Value;
	}

	return Level;
}

int LogShouldWrite(LogLevel Level)
{
	return NormalizeLevel(Level) >= MinLevel;
}

static void LogVWrite(LogLevel Level, const char *Component, const char *Format,
					  va_list Arguments)
{
	Level = NormalizeLevel(Level);
	const LogStyle *Style = &LogStyles[Level];

	if (!LogShouldWrite(Level))
		return;

	StdoutLock();
	if (Component != 0 && Component[0] != '\0') {
		Printf("[%s] ", Component);
	}

	Printf("%s%s" RESET ": ", Style->Color, Style->Label);
	VPrintf(Format, Arguments);
	Printf("\n");
	StdoutUnlock();
}

void LogInit(const char *Cmdline)
{
	int level = -1;

	if (Cmdline != 0)
		level = ParseLoglevel(Cmdline);

	if (level >= LogLevelTrace && level <= LogLevelFatal)
		MinLevel = (LogLevel)level;
	else
		MinLevel = LogLevelWarn;
}

LogLevel LogGetLevel(void)
{
	return MinLevel;
}

void LogSetLevel(LogLevel Level)
{
	MinLevel = NormalizeLevel(Level);
}

void LogWrite(LogLevel Level, const char *Component, const char *Format, ...)
{
	va_list Arguments;

	va_start(Arguments, Format);
	LogVWrite(Level, Component, Format, Arguments);
	va_end(Arguments);
}
