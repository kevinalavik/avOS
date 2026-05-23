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

void Log(LogLevel Level, const char *Format, ...);

#define LogTrace(...)  Log(LogLevelTrace, __VA_ARGS__)
#define LogDebug(...)  Log(LogLevelDebug, __VA_ARGS__)
#define LogInfo(...)   Log(LogLevelInfo, __VA_ARGS__)
#define LogOk(...)     Log(LogLevelOk, __VA_ARGS__)
#define LogWarn(...)   Log(LogLevelWarn, __VA_ARGS__)
#define LogError(...)  Log(LogLevelError, __VA_ARGS__)
#define LogFatal(...)  Log(LogLevelFatal, __VA_ARGS__)

#endif
