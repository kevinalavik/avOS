#ifndef AV_STDLIB_H
#define AV_STDLIB_H
#include <stddef.h>
#include <System/Memory.h>
#define RAND_MAX 32767
int atoi(const char *S);
long atol(const char *S);
long strtol(const char *Nptr, char **Endptr, int Base);
unsigned long strtoul(const char *Nptr, char **Endptr, int Base);
double strtod(const char *Nptr, char **Endptr);
float strtof(const char *Nptr, char **Endptr);
long double strtold(const char *Nptr, char **Endptr);
double atof(const char *S);
int abs(int X);
long labs(long X);
void srand(unsigned int Seed);
int rand(void);
int atexit(void (*Function)(void));
void abort(void) __attribute__((noreturn));
void exit(int Status) __attribute__((noreturn));
char *getenv(const char *Name);
int system(const char *Command);
#endif
