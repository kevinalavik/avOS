#ifndef AV_UNISTD_H
#define AV_UNISTD_H
#include <stddef.h>
typedef unsigned long useconds_t;
#define F_OK 0
#define R_OK 4
int access(const char *Path, int Mode);
unsigned int sleep(unsigned int Seconds);
int usleep(useconds_t Usec);
#endif
