#ifndef AV_STDIO_H
#define AV_STDIO_H
#include <stdarg.h>
#include <stddef.h>
#include <System/Types.h>
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define BUFSIZ 512
#define FILENAME_MAX 256
#define L_tmpnam 32
typedef struct AvFile FILE;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;
FILE *fopen(const char *Path, const char *Mode);
size_t fread(void *Ptr, size_t Size, size_t Count, FILE *Stream);
size_t fwrite(const void *Ptr, size_t Size, size_t Count, FILE *Stream);
int fseek(FILE *Stream, long Offset, int Whence);
long ftell(FILE *Stream);
int fclose(FILE *Stream);
int fflush(FILE *Stream);
int feof(FILE *Stream);
int ferror(FILE *Stream);
int fgetc(FILE *Stream);
char *fgets(char *S, int Size, FILE *Stream);
int fputs(const char *S, FILE *Stream);
int printf(const char *Format, ...);
int fprintf(FILE *Stream, const char *Format, ...);
int vfprintf(FILE *Stream, const char *Format, va_list Args);
int sprintf(char *Buffer, const char *Format, ...);
int snprintf(char *Buffer, size_t Size, const char *Format, ...);
int vsnprintf(char *Buffer, size_t Size, const char *Format, va_list Args);
int sscanf(const char *Str, const char *Format, ...);
int puts(const char *S);
int putchar(int C);
void perror(const char *S);
int remove(const char *Path);
int rename(const char *Old, const char *New);
#endif
