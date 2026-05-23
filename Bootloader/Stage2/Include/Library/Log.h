#ifndef LIB_LOG_H
#define LIB_LOG_H

#include <stdarg.h>

typedef enum LogLevel {
	LogLevelDebug,
	LogLevelInfo,
	LogLevelOk,
	LogLevelWarn,
	LogLevelError,
} LogLevel;

#ifndef LogLevelThreshold
#define LogLevelThreshold LogLevelInfo
#endif

void LogWrite(LogLevel Level, const char *Component, const char *Format, ...);
void LogVWrite(LogLevel Level, const char *Component, const char *Format,
			   va_list Arguments);

#define LogShouldWrite(Level) ((Level) >= LogLevelThreshold)

#define LogDebug(Component, Format, ...)                               \
	do {                                                               \
		if (LogShouldWrite(LogLevelDebug)) {                           \
			LogWrite(LogLevelDebug, Component, Format, ##__VA_ARGS__); \
		}                                                              \
	} while (0)
#define LogInfo(Component, Format, ...)                               \
	do {                                                              \
		if (LogShouldWrite(LogLevelInfo)) {                           \
			LogWrite(LogLevelInfo, Component, Format, ##__VA_ARGS__); \
		}                                                             \
	} while (0)
#define LogOk(Component, Format, ...)                               \
	do {                                                            \
		if (LogShouldWrite(LogLevelOk)) {                           \
			LogWrite(LogLevelOk, Component, Format, ##__VA_ARGS__); \
		}                                                           \
	} while (0)
#define LogWarn(Component, Format, ...)                               \
	do {                                                              \
		if (LogShouldWrite(LogLevelWarn)) {                           \
			LogWrite(LogLevelWarn, Component, Format, ##__VA_ARGS__); \
		}                                                             \
	} while (0)
#define LogError(Component, Format, ...)                               \
	do {                                                               \
		if (LogShouldWrite(LogLevelError)) {                           \
			LogWrite(LogLevelError, Component, Format, ##__VA_ARGS__); \
		}                                                              \
	} while (0)

#endif
