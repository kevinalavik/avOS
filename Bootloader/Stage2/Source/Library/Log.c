#include <Library/Log.h>

#include <Device/Console.h>
#include <Library/Printf.h>

typedef struct LogStyle {
	const char *Label;
	uint8_t Foreground;
} LogStyle;

static const LogStyle LogStyles[] = {
	[LogLevelDebug] = { "DEBUG", ConsoleColorDarkGray },
	[LogLevelInfo] = { "INFO", ConsoleColorLightCyan },
	[LogLevelOk] = { " OK ", ConsoleColorLightGreen },
	[LogLevelWarn] = { "WARN", ConsoleColorYellow },
	[LogLevelError] = { "ERROR", ConsoleColorLightRed },
};

static const LogStyle *StyleFor(LogLevel Level)
{
	if (Level < LogLevelDebug || Level > LogLevelError) {
		Level = LogLevelInfo;
	}

	return &LogStyles[Level];
}

void LogVWrite(LogLevel Level, const char *Component, const char *Format,
			   va_list Arguments)
{
	const LogStyle *Style = StyleFor(Level);
	uint8_t PreviousColor = ConsoleGetColor();

	ConsoleSetColor(Style->Foreground, VgaDefaultBackground);
	Printf("[%s]", Style->Label);
	ConsoleSetPackedColor(PreviousColor);

	if (Component != 0 && Component[0] != '\0') {
		ConsoleSetColor(ConsoleColorWhite, VgaDefaultBackground);
		Printf(" %s:", Component);
		ConsoleSetPackedColor(PreviousColor);
	}

	Printf(" ");
	VPrintf(Format, Arguments);
	Printf("\n");
}

void LogWrite(LogLevel Level, const char *Component, const char *Format, ...)
{
	va_list Arguments;
	va_start(Arguments, Format);
	LogVWrite(Level, Component, Format, Arguments);
	va_end(Arguments);
}
