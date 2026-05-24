#ifndef DEVICE_TEXTCONSOLE_H
#define DEVICE_TEXTCONSOLE_H

#include <stdbool.h>

void TextConsoleInit(void);
bool TextConsoleReady(void);
void TextConsolePutc(char Character);

#endif
