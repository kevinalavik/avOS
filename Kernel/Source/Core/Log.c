#include <Core/Log.h>
#include <Library/Printf.h>

#include <stdarg.h>

static LogLevel MinLevel = LogLevelInfo;

static const char *const LevelLabels[] = {
	"TRCE", "DBUG", "INFO", " OK ", "WARN", "ERR!", "FALT",
};

static const char *const LevelColors[] = {
	"\033[90m",      // Trace
	"\033[36m",      // Debug
	"\033[94m",      // Info
	"\033[92m",      // Ok
	"\033[93m",      // Warn
	"\033[91m",      // Error
	"\033[1;91m",    // Fatal
};

void LogInit(void)
{
	MinLevel = LogLevelInfo;
}

void LogSetLevel(LogLevel Level)
{
	MinLevel = Level;
}

void Log(LogLevel Level, const char *Format, ...)
{
	if (Level < MinLevel)
		return;

	Printf("%s[%s]\033[0m ", LevelColors[Level], LevelLabels[Level]);

	{
		va_list Arguments;
		va_start(Arguments, Format);
		VPrintf(Format, Arguments);
		va_end(Arguments);
	}

	PrintLine("");
}
