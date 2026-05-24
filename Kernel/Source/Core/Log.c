#include <Core/Log.h>
#include <Library/Printf.h>

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
	[LogLevelInfo]  = { "info",  ESC "[97m" },
	[LogLevelOk]    = { "ok",    ESC "[92m" },
	[LogLevelWarn]  = { "warn",  ESC "[93m" },
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

static int ParseLoglevel(const char *Cmdline)
{
	const char *p = Cmdline;

	while (*p != '\0') {
		const char *start = p;

		while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '=')
			++p;

		if ((p - start) >= 8) {
			const char *tail = p - 8;
			int match = 1;

			for (int i = 0; i < 8; ++i) {
				if (tail[i] != "loglevel"[i]) {
					match = 0;
					break;
				}
			}

			if (match) {
				while (*p == ' ' || *p == '\t' || *p == '=')
					++p;

				int value = 0;
				while (*p >= '0' && *p <= '9') {
					value = value * 10 + (*p - '0');
					++p;
				}
				return value;
			}
		}

		if (*p != '\0')
			++p;
	}

	return -1;
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

	if (Component != 0 && Component[0] != '\0') {
		Printf("[%s] ", Component);
	}

	Printf("%s%s" RESET ": ", Style->Color, Style->Label);
	VPrintf(Format, Arguments);
	Printf("\n");
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
