#ifndef AV_CTYPE_H
#define AV_CTYPE_H
static inline int isspace(int C)
{
	return C == ' ' || C == '\t' || C == '\n' || C == '\r' || C == '\f' ||
		   C == '\v';
}
static inline int isdigit(int C)
{
	return C >= '0' && C <= '9';
}
static inline int isalpha(int C)
{
	return (C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z');
}
static inline int isalnum(int C)
{
	return isalpha(C) || isdigit(C);
}
static inline int isxdigit(int C)
{
	return isdigit(C) || (C >= 'a' && C <= 'f') || (C >= 'A' && C <= 'F');
}
static inline int toupper(int C)
{
	return (C >= 'a' && C <= 'z') ? C - ('a' - 'A') : C;
}
static inline int tolower(int C)
{
	return (C >= 'A' && C <= 'Z') ? C + ('a' - 'A') : C;
}
#endif
