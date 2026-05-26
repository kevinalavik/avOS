#ifndef CORE_ELF_H
#define CORE_ELF_H

#include <Core/Scheduler.h>

#include <stdbool.h>

Process *ElfSpawn(const char *Path);
bool ElfExec(const char *Path);

#endif
