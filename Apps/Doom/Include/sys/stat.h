#ifndef AV_SYS_STAT_H
#define AV_SYS_STAT_H
#include <sys/types.h>
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_ISREG(m) (((m) & S_IFREG) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFDIR) == S_IFDIR)
struct stat {
	off_t st_size;
	unsigned int st_mode;
};
int stat(const char *Path, struct stat *St);
int mkdir(const char *Path, unsigned int Mode);
#endif
