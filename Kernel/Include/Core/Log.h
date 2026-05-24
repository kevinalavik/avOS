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

void LogInit(void);
void LogSetLevel(LogLevel Level);

void LogWrite(LogLevel Level, const char *Component, const char *Format, ...);

#define LogTrace(Component, Format, ...) \
	LogWrite(LogLevelTrace, Component, Format, ##__VA_ARGS__)
#define LogDebug(Component, Format, ...) \
	LogWrite(LogLevelDebug, Component, Format, ##__VA_ARGS__)
#define LogInfo(Component, Format, ...) \
	LogWrite(LogLevelInfo, Component, Format, ##__VA_ARGS__)
#define LogOk(Component, Format, ...) \
	LogWrite(LogLevelOk, Component, Format, ##__VA_ARGS__)
#define LogWarn(Component, Format, ...) \
	LogWrite(LogLevelWarn, Component, Format, ##__VA_ARGS__)
#define LogError(Component, Format, ...) \
	LogWrite(LogLevelError, Component, Format, ##__VA_ARGS__)
#define LogFatal(Component, Format, ...) \
	LogWrite(LogLevelFatal, Component, Format, ##__VA_ARGS__)

#endif
