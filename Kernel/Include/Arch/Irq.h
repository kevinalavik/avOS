#ifndef ARCH_IRQ_H
#define ARCH_IRQ_H

#include <Arch/Idt.h>
#include <stdint.h>

#define IrqPit      0
#define IrqKeyboard 1

typedef void (*IrqHandler)(Frame *Frame);

void IrqInit(void);
void IrqRegisterHandler(uint8_t Irq, IrqHandler Handler);
void IrqDispatch(Frame *Frame);

#endif
