#ifndef LOADER_ELF64_H
#define LOADER_ELF64_H

#include <stdbool.h>
#include <stdint.h>

bool Elf64Load(const char *Path, uint64_t *EntryAddress);

#endif
