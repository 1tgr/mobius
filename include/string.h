/* $Id: string.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
/* Copyright (C) 1998 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#ifndef __dj_include_string_h_
#define __dj_include_string_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#ifndef NULL
#define NULL 0
#endif

void *	memchr(const void *_s, int _c, size_t _n);
int		memcmp(const void *_s1, const void *_s2, size_t _n);
void *	memcpy(void *_dest, const void *_src, size_t _n);
void *	memmove(void *_s1, const void *_s2, size_t _n);
void *	memset(void *_s, int _c, size_t _n);
char *	strcat(char *_s1, const char *_s2);
char *	strchr(const char *_s, int _c);
int		strcmp(const char *_s1, const char *_s2);
int		strcoll(const char *_s1, const char *_s2);
char *	strcpy(char *_s1, const char *_s2);
size_t	strcspn(const char *_s1, const char *_s2);
char *	strerror(int _errcode);
size_t	strlen(const char *_s);
char *	strncat(char *_s1, const char *_s2, size_t _n);
int		strncmp(const char *_s1, const char *_s2, size_t _n);
char *	strncpy(char *_s1, const char *_s2, size_t _n);
char *	strpbrk(const char *_s1, const char *_s2);
char *	strrchr(const char *_s, int _c);
size_t	strspn(const char *_s1, const char *_s2);
char *	strstr(const char *_s1, const char *_s2);
char *	strtok(char *_s1, const char *_s2);
size_t	strxfrm(char *_s1, const char *_s2, size_t _n);

int		_stricmp(const char *_s1, const char *_s2);

#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_string_h_ */
