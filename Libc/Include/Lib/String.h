#ifndef LIB_STRING_H
#define LIB_STRING_H

#include <System/Types.h>

Size StringLength(const char *Str);
int StringCompare(const char *Str1, const char *Str2);
void StringCopy(char *Dst, Size DstSize, const char *Src);
void StringAppend(char *Dst, Size DstSize, const char *Src);

#endif
