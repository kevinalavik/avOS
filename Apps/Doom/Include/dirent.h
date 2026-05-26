#ifndef AV_DIRENT_H
#define AV_DIRENT_H
typedef struct AvDir DIR;
struct dirent {
	char d_name[128];
};
DIR *opendir(const char *Path);
struct dirent *readdir(DIR *Dir);
int closedir(DIR *Dir);
#endif
