#ifndef __STDIO_H
#define __STDIO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <stdarg.h>
#include <io.h>

typedef struct FILE FILE;
struct FILE;

extern FILE *stdin, *stdout, *stderr;

int wprintf(const wchar_t* fmt, ...);
int vwprintf(const wchar_t* fmt, va_list ptr);
int vswprintf(wchar_t *buffer, const wchar_t *format, va_list argptr);
int swprintf(wchar_t *buffer, const wchar_t *format, ...);

int printf(const char* fmt, ...);
int vprintf(const char* fmt, va_list ptr);
int vsprintf(char *buffer, const char *format, va_list argptr);
int sprintf(char *buffer, const char *format, ...);

wint_t putwchar(wint_t c);
int putchar(int c);
int _cputws(const wchar_t* str);
int _cputs(const char* str);

int fprintf(FILE* file, const char* fmt, ...);

FILE* fopen(const char* filename, const char* mode);
int fclose(FILE* f);
size_t	fread (void* pBuffer, size_t sizeObject, size_t sizeObjCount,
		FILE* fileRead);
size_t	fwrite (const void* pObjArray, size_t sizeObject, size_t sizeObjCount,
		FILE* fileWrite);

void _pwerror(const wchar_t* string);

#define SEEK_CUR    1
#define SEEK_END    2
#define SEEK_SET    0

#ifdef __cplusplus
}
#endif

#endif