#ifndef DEVICE_TEXTCONSOLE_H
#define DEVICE_TEXTCONSOLE_H

#include <stdbool.h>
#include <stdint.h>

void TextConsoleInit(void);
bool TextConsoleReady(void);
void TextConsoleSetBufferAddress(uint64_t Address);
void TextConsolePutc(char Character);

#endif
