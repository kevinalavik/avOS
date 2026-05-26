#include <Lib/String.h>

Size StringLength(const char *Str)
{
	Size N = 0;
	if (Str == 0)
		return 0;
	while (Str[N] != '\0')
		++N;
	return N;
}

int StringCompare(const char *Str1, const char *Str2)
{
	Size I = 0;
	while (Str1[I] != '\0' && Str2[I] != '\0' && Str1[I] == Str2[I])
		++I;
	return (int)(unsigned char)Str1[I] - (int)(unsigned char)Str2[I];
}

void StringCopy(char *Dst, Size DstSize, const char *Src)
{
	Size I = 0;
	if (Dst == 0 || DstSize == 0)
		return;
	while (Src != 0 && Src[I] != '\0' && I + 1 < DstSize) {
		Dst[I] = Src[I];
		++I;
	}
	Dst[I] = '\0';
}

void StringAppend(char *Dst, Size DstSize, const char *Src)
{
	Size Len = StringLength(Dst);
	Size I = 0;
	while (Src != 0 && Src[I] != '\0' && Len + 1 < DstSize) {
		Dst[Len++] = Src[I++];
	}
	Dst[Len] = '\0';
}
