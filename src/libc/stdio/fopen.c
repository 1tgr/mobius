/* $Id: fopen.c,v 1.1 2002/02/22 16:51:35 pavlovskii Exp $ */

#include <stdio.h>
#include <os/syscall.h>
#include <os/defs.h>

#define COMP1(s1, s2)	((s1)[0] == (s2)[0] && \
						 (s1)[1] == '\0')
#define COMP2(s1, s2)	((s1)[0] == (s2)[0] && \
						 (s1)[1] == (s2)[1] && \
						 (s1)[2] == '\0')
#define COMP3(s1, s2)	((s1)[0] == (s2)[0] && \
						 (s1)[1] == (s2)[1] && \
						 (s1)[2] == (s2)[2] && \
						 (s1)[3] == '\0')

/*FILE *fopen(const char *filename, const char *mode)
{
	FILE *f;
	wchar_t temp[MAX_PATH + 1];
	size_t len;
	uint32_t flags;
	const char *ch;

	len = mbstowcs(temp, filename, _countof(temp));
	if (len == -1)
		return NULL;

	temp[len] = '\0';

	f = malloc(sizeof(FILE));
	if (f == NULL)
		return NULL;

	flags = 0;
	if (COMP1(mode, "r"))
		flags = FILE_READ;
	else if (COMP1(mode, "w"))
		flags = FILE_WRITE;
	else if (COMP1(mode, "a"))
		flags = FILE_WRITE;
	f->_osfhnd = FsOpen(temp, 
}*/