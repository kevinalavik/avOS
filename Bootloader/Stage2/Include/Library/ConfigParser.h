#ifndef LIB_CONFIGPARSER_H
#define LIB_CONFIGPARSER_H

#include <stdbool.h>
#include <stddef.h>

#define BootConfigPath "boot:/avboot.conf"
#define KernelConfigKey "kernel="
#define BootPathMax 128u

bool ParseKernelPath(const char *Config, char KernelPath[BootPathMax]);
bool ReadTextFile(const char *Path, char **BufferOut);

#endif
