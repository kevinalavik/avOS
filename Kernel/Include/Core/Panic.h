#ifndef CORE_PANIC_H
#define CORE_PANIC_H

#include <Arch/Idt.h>

void Panic(Frame *frame, const char *Reason, ...);

#endif