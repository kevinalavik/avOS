#ifndef LIB_DEBUGLOG_H
#define LIB_DEBUGLOG_H

#ifndef BOOTLOADER_DEBUG
#define BOOTLOADER_DEBUG 0
#endif

void BootError(const char *Component, const char *Format, ...);

#if BOOTLOADER_DEBUG
void DebugLog(const char *Component, const char *Format, ...);
#else
#define DebugLog(...)               \
	do {                            \
		if (0) {                    \
			BootError(__VA_ARGS__); \
		}                           \
	} while (0)
#endif

#endif
