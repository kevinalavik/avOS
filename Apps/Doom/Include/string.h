#ifndef AV_STRING_H
#define AV_STRING_H
#include <stddef.h>
void *memset(void *Dst, int Ch, size_t Count);
void *memcpy(void *Dst, const void *Src, size_t Count);
void *memmove(void *Dst, const void *Src, size_t Count);
int memcmp(const void *A, const void *B, size_t Count);
size_t strlen(const char *S);
char *strcpy(char *Dst, const char *Src);
char *strncpy(char *Dst, const char *Src, size_t Count);
char *strcat(char *Dst, const char *Src);
char *strncat(char *Dst, const char *Src, size_t Count);
int strcmp(const char *A, const char *B);
int strncmp(const char *A, const char *B, size_t Count);
char *strchr(const char *S, int Ch);
char *strrchr(const char *S, int Ch);
char *strstr(const char *Haystack, const char *Needle);
char *strdup(const char *S);
char *strerror(int Errnum);
#endif
