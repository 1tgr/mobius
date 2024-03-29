/* $Id: stdlib.h,v 1.1.1.1 2002/12/31 01:26:20 pavlovskii Exp $ */
/* Copyright (C) 1999 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1998 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#ifndef __dj_include_stdlib_h_
#define __dj_include_stdlib_h_

#ifdef __cplusplus
extern "C" {
#endif

/* Some programs think they know better... */
#undef NULL

#define EXIT_FAILURE    1
#define EXIT_SUCCESS    0
#define MB_CUR_MAX      7
#define NULL            0
#define RAND_MAX        2147483647


typedef struct {
  int quot;
  int rem;
} div_t;

typedef struct {
  long quot;
  long rem;
} ldiv_t;

#ifndef _MSC_VER
typedef struct {
  long long quot;
  long long rem;
} lldiv_t;
#endif

#include <sys/types.h>

#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifdef _MSC_VER
#define __noreturn
#else
#define __noreturn __attribute__((noreturn))
#endif

void    abort(void) __noreturn;
int     abs(int _i);
int     atexit(void (*_func)(void));
double  atof(const char *_s);
int     atoi(const char *_s);
long    atol(const char *_s);
void *  bsearch(const void *_key, const void *_base, size_t _nelem,
                size_t _size, int (*_cmp)(const void *_ck, const void *_ce));
void *  calloc(size_t _nelem, size_t _size);
div_t   div(int _numer, int _denom);
void    exit(int _status) __noreturn;
char *  getenv(const char *_name);
long    labs(long _i);
ldiv_t  ldiv(long _numer, long _denom);
int     mblen(const char *_s, size_t _n);
size_t  mbstowcs(wchar_t *_wcs, const char *_s, size_t _n);
int     mbtowc(wchar_t *_pwc, const char *_s, size_t _n);
void    qsort(void *_base, size_t _nelem, size_t _size,
              int (*_cmp)(const void *_e1, const void *_e2));
int     rand(void);
void    srand(unsigned _seed);
double  strtod(const char *_s, char **_endptr);
long    strtol(const char *_s, char **_endptr, int _base);
unsigned long   strtoul(const char *_s, char **_endptr, int _base);
int     system(const char *_s);
size_t  wcstombs(char *_s, const wchar_t *_wcs, size_t _n);
int     wctomb(char *_s, wchar_t _wchar);

void *  calloc(size_t _nelem, size_t _size);
void *  memalign(size_t alignment, size_t n);
void *  valloc(size_t n);

void *  __malloc(size_t _size, const char *, int);
void    __free(void *_ptr, const char *, int);
void *  __realloc(void *_ptr, size_t _size, const char *, int);
wchar_t *__wcsdup(const wchar_t *_str, const char *, int);
char *  __strdup(const char *_str, const char *, int);
void    __malloc_leak(int tag);
void    __malloc_leak_dump(void);
int		__malloc_lock(int lock_unlock, unsigned long *lock);
bool    __malloc_check_all(void);

#define malloc(x)       __malloc((x), __FILE__, __LINE__)
#define calloc(x, n)    __calloc((x), (n), __FILE__, __LINE__)
#define free(p)         __free((p), __FILE__, __LINE__)
#define realloc(p, x)   __realloc((p), (x), __FILE__, __LINE__)
#define _strdup(p)      __strdup((p), __FILE__, __LINE__)
#define _wcsdup(p)      __wcsdup((p), __FILE__, __LINE__)

long double _strtold(const char *s, char **sret);
long double _atold(const char *ascii);

#ifdef _POSIX_SOURCE
long    random(void);
void    srandom(unsigned);
#endif

#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_stdlib_h_ */
