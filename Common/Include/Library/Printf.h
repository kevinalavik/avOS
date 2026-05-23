#ifndef LIB_PRINTF_H
#define LIB_PRINTF_H

#include <stdarg.h>

int PrintChar(int Character);
int PrintLine(const char *String);
int VPrintf(const char *Format, va_list Arguments);
int Printf(const char *Format, ...);

#endif
