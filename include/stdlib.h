#ifndef __STDLIB_H
#define __STDLIB_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>

/*!
 *  \ingroup	libc
 *  \defgroup	stdlib	Standard library definitions
 *  @{
 */

#define countof(a)	(sizeof(a) / sizeof(a[0]))

#ifndef min
#define min(a, b)	((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b)	((a) > (b) ? (a) : (b))
#endif

#include <malloc.h>

int		rand();
void	srand(unsigned long new_seed);
void	exit(int status);
int		atexit(void (__cdecl *func)(void));
int		match(const wchar_t *mask, const wchar_t *name);
long	strtol(const char *nptr, char **endptr, int base);
long	wcstol(const wchar_t *nptr, wchar_t **endptr, int base);
void	qsort(void *Base, size_t Num, size_t Width, 
			  int(*Compare) (const void *, const void *));

#define MAX_PATH	256

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1

//@}

#ifdef __cplusplus
}
#endif

#endif