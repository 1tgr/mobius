#ifndef __DIRENT_H
#define __DIRENT_H

typedef struct DIR DIR;
struct dirent
{
	int d_reclen;
	char d_name[20];
};

#define opendir(p)	NULL
#define closedir(d)	

#endif