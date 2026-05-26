#ifndef LIB_STDOUT_H
#define LIB_STDOUT_H

#include <stdbool.h>

extern void (*StdoutPutc)(char Character);
void StdoutLock(void);
void StdoutUnlock(void);

#endif
