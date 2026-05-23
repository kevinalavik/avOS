#ifndef ARCH_LONGMODE_H
#define ARCH_LONGMODE_H

#include <stdint.h>

__attribute__((noreturn)) void EnterLongMode(uint32_t PageMapAddress,
											 uint64_t EntryAddress,
											 uint32_t BootInfoAddress);

#endif
