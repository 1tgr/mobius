#ifndef __STRING_H
#define __STRING_H

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 *  \ingroup	libc
 *  \defgroup	string	String routines
 *  @{
 */

#include <sys/types.h>

/* ANSI-character functions */
size_t	strlen(const char* str);
int		stricmp(const char* str1, const char* str2);
int		strncmp(const char* str1, const char* str2, size_t count);
char	*strcpy(char *strDestination, const char *strSource);
int		strcmp(const char *string1, const char *string2);
char	*strtok(char *string, const char *control);
char	*strpbrk(const char *s1, const char *s2); 
char	*strncpy(char* Dst, const char* Src, unsigned int Count);

/* Buffer manipulation functions */
void	*memset(void *dest, int c, size_t count);
void	*memcpy(void *dest, const void *src, size_t count);
void	*memmove(void* dest, const void *src, size_t count);
int		memcmp(const void *buf1, const void *buf2, size_t count);

//@}

#ifdef __cplusplus
}
#endif

#endif