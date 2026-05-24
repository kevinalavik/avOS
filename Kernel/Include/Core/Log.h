#ifndef CORE_LOG_H
#define CORE_LOG_H

typedef enum {
	LogLevelTrace,
	LogLevelDebug,
	LogLevelInfo,
	LogLevelOk,
	LogLevelWarn,
	LogLevelError,
	LogLevelFatal,
} LogLevel;

void LogInit(const char *Cmdline);
LogLevel LogGetLevel(void);
void LogSetLevel(LogLevel Level);
int LogShouldWrite(LogLevel Level);

void LogWrite(LogLevel Level, const char *Component, const char *Format, ...);

#define LogAt(Level, Component, Format, ...)                       \
	do {                                                           \
		if (LogShouldWrite(Level)) {                               \
			LogWrite(Level, Component, Format, ##__VA_ARGS__);     \
		}                                                          \
	} while (0)

#define LogTrace(Component, Format, ...) \
	LogAt(LogLevelTrace, Component, Format, ##__VA_ARGS__)
#define LogDebug(Component, Format, ...) \
	LogAt(LogLevelDebug, Component, Format, ##__VA_ARGS__)
#define LogInfo(Component, Format, ...) \
	LogAt(LogLevelInfo, Component, Format, ##__VA_ARGS__)
#define LogOk(Component, Format, ...) \
	LogAt(LogLevelOk, Component, Format, ##__VA_ARGS__)
#define LogWarn(Component, Format, ...) \
	LogAt(LogLevelWarn, Component, Format, ##__VA_ARGS__)
#define LogError(Component, Format, ...) \
	LogAt(LogLevelError, Component, Format, ##__VA_ARGS__)
#define LogFatal(Component, Format, ...) \
	LogAt(LogLevelFatal, Component, Format, ##__VA_ARGS__)

#endif
