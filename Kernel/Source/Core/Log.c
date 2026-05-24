#include <Core/Log.h>
#include <Library/Printf.h>

#include <stdarg.h>

static LogLevel MinLevel = LogLevelInfo;

typedef struct LogStyle {
	const char *Label;
	const char *Color;
} LogStyle;

static const LogStyle LogStyles[] = {
	[LogLevelTrace] = { "TRACE", "\033[90m" },
	[LogLevelDebug] = { "DEBUG", "\033[90m" },
	[LogLevelInfo] = { "INFO", "\033[96m" },
	[LogLevelOk] = { " OK ", "\033[92m" },
	[LogLevelWarn] = { "WARN", "\033[93m" },
	[LogLevelError] = { "ERROR", "\033[91m" },
	[LogLevelFatal] = { "FATAL", "\033[1;91m" },
};

static LogLevel NormalizeLevel(LogLevel Level)
{
	if (Level < LogLevelTrace || Level > LogLevelFatal)
		return LogLevelInfo;

	return Level;
}

static void LogVWrite(LogLevel Level, const char *Component, const char *Format,
					  va_list Arguments)
{
	Level = NormalizeLevel(Level);
	const LogStyle *Style = &LogStyles[Level];

	if (Level < MinLevel)
		return;

	Printf("%s[%s]\033[0m", Style->Color, Style->Label);
	if (Component != 0 && Component[0] != '\0') {
		Printf(" \033[97m%s:\033[0m", Component);
	}

	Printf(" ");
	VPrintf(Format, Arguments);
	Printf("\n");
}

void LogInit(void)
{
	MinLevel = LogLevelInfo;
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
