#ifndef LIB_CONFIGPARSER_H
#define LIB_CONFIGPARSER_H

#include <stdbool.h>
#include <stddef.h>

#define BootConfigPath "/avboot.conf"
#define KernelConfigKey "kernel="
#define FramebufferConfigKey "framebuffer="
#define CmdlineConfigKey "cmdline="
#define BootPathMax 128u
#define BootCmdlineMax 256u

bool ParseKernelPath(const char *Config, char KernelPath[BootPathMax]);
bool ParseFramebufferEnabled(const char *Config);
bool ParseCmdline(const char *Config, char Cmdline[BootCmdlineMax]);
bool ReadTextFile(const char *Path, char **BufferOut);

#endif
