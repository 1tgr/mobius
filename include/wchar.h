/* $Id: wchar.h,v 1.3 2001/11/05 23:47:41 pavlovskii Exp $ */
#ifndef __WCHAR_H
#define __WCHAR_H

#include <sys/types.h>

    /* MACROS */
#ifndef NULL
#define NULL 0
#endif

#ifndef WCHAR_MAX
#define WCHAR_MAX 65535
#endif

#ifdef WCHAR_MIN
#define WCHAR_MIN 0
#endif

#ifndef WEOF
#define WEOF -1
#endif

struct FILE;
#define restrict

    /* TYPES */
#include <sys/types.h>

struct tm;
typedef int mbstate_t;

#ifdef __cplusplus
extern "C"
{
#endif

    /* FUNCTIONS */
wint_t fgetwc(struct FILE *stream);
wchar_t *fgetws(wchar_t *restrict s, int n, struct FILE *restrict stream);
wint_t fputwc(wchar_t c, struct FILE *stream);
int fputws(const wchar_t *restrict s, struct FILE *restrict stream);
int fwide(struct FILE *stream, int mode);
wint_t getwc(struct FILE *stream);
wint_t getwchar(void);
wint_t putwc(wchar_t c, struct FILE *stream);
wint_t putwchar(wchar_t c);
wint_t ungetwc(wint_t c, struct FILE *stream);

int fwscanf(struct FILE *restrict stream, const wchar_t *restrict format, ...);
int swscanf(const wchar_t *restrict s,
    const wchar_t *restrict format, ...);
int wscanf(const wchar_t *restrict format, ...);
int fwprintf(struct FILE *restrict stream, const wchar_t *restrict format, ...);
int swprintf(wchar_t *restrict s, const wchar_t *restrict format, ...);
int wprintf(const wchar_t *restrict format, ...);
int vfwscanf(struct FILE *restrict stream, const wchar_t *restrict format,
    va_list arg);
int vswscanf(const wchar_t *restrict s, size_t n, const wchar_t *restrict format,
    va_list arg);
int vwscanf(const wchar_t *restrict format,
    va_list arg);
int vfwprintf(struct FILE *restrict stream, const wchar_t *restrict format,
    va_list arg);
int vswprintf(wchar_t *restrict s, /*size_t n, */const wchar_t *restrict format,
    va_list arg);
int vwprintf(const wchar_t *restrict format,
    va_list arg);

size_t wcsftime(wchar_t *restrict s, size_t maxsize,
    const wchar_t *restrict format, const struct tm *restrict timeptr);

wint_t btowc(int c);
size_t mbrlen(const char *restrict s, size_t n,
    mbstate_t *restrict ps);
size_t mbrtowc(wchar_t *restrict pwc, const char *restrict s,
    size_t n, mbstate_t *restrict ps);
int mbsinit(const mbstate_t *ps);
size_t mbsrtowcs(wchar_t *restrict dst, const char **restrict src,
    size_t len, mbstate_t *restrict ps);
size_t wcrtomb(char *restrict s, wchar_t wc,
    mbstate_t *restrict ps);
size_t wcsrtombs(char *restrict dst, const wchar_t **restrict src,
    size_t len, mbstate_t *restrict ps);
double wcstod(const wchar_t *restrict nptr,
    wchar_t **restrict endptr);
float wcstof(const wchar_t *restrict nptr,
    wchar_t **restrict endptr);
long double wcstold(const wchar_t *restrict nptr,
    wchar_t **restrict endptr);
long long wcstoll(const wchar_t *restrict nptr, wchar_t **restrict endptr,
    int base);
unsigned long long wcstoull(const wchar_t *restrict nptr, wchar_t **restrict endptr,
    int base);
long wcstol(const wchar_t *restrict nptr, wchar_t **restrict endptr,
    int base);
unsigned long wcstoul(const wchar_t *restrict nptr, wchar_t **restrict endptr,
    int base);
int wctob(wint_t c);

wchar_t *wcscat(wchar_t *restrict s1, const wchar_t *restrict s2);
int wcscmp(const wchar_t *s1, const wchar_t *s2);
int _wcsicmp(const wchar_t *s1, const wchar_t *s2);
int _wcsnicmp(const wchar_t *s1, const wchar_t *s2, size_t n);
int wcscoll(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcscpy(wchar_t *restrict s1, const wchar_t *restrict s2);
size_t wcscspn(const wchar_t *s1, const wchar_t *s2);
size_t wcslen(const wchar_t *s);
wchar_t *wcsncat(wchar_t *restrict s1, const wchar_t *restrict s2,
    size_t n);
int wcsncmp(const wchar_t *s1, const wchar_t *s2,
    size_t n);
wchar_t *wcsncpy(wchar_t *restrict s1, const wchar_t *restrict s2,
    size_t n);
size_t wcsspn(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcsstr(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcstok(wchar_t *restrict s1, const wchar_t *restrict s2,
    wchar_t **restrict ptr);
size_t wcsxfrm(wchar_t *restrict s1, const wchar_t *restrict s2, size_t n);
int wmemcmp(const wchar_t *s1, const wchar_t *s2,
    size_t n);
wchar_t *wmemcpy(wchar_t *restrict s1, const wchar_t *restrict s2,
    size_t n);
wchar_t *wmemmove(wchar_t *s1, const wchar_t *s2,
    size_t n);
wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n);
wchar_t *wcschr(const wchar_t *s, wchar_t c);
wchar_t *wcspbrk(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
wchar_t *wcsstr(const wchar_t *s1, const wchar_t *s2);
wchar_t *wmemchr(const wchar_t *s, wchar_t c,
    size_t n);

wchar_t *_wcsdup(const wchar_t *s);
int		_wcsmatch(const wchar_t *mask, const wchar_t *name);

#define iswupper(c)		((c) >= 'A' && (c) <= 'Z')
#define iswlower(c)		((c) >= 'a' && (c) <= 'z')
#define towupper(c)		(iswlower(c) ? (c) - 'a' + 'A' : (c))
#define towlower(c)		(iswupper(c) ? (c) - 'A' + 'a' : (c))
#define iswdigit(c)		((c) >= '0' && (c) <= '9')

#ifdef __cplusplus
}
#endif

#endif