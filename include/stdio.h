#ifndef __STDIO_H
#define __STDIO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <stdarg.h>
#include <io.h>

/*!
 *  \ingroup	libc
 *  \defgroup	stdio	Standard input and output
 *  @{
 */

typedef struct FILE FILE;
struct FILE
{
	addr_t fd;
};

extern FILE __iobuf[3];
#define stdin	(__iobuf + 0)
#define stdout	(__iobuf + 1)
#define stderr	(__iobuf + 2)

int		wprintf(const wchar_t* fmt, ...);
int		vwprintf(const wchar_t* fmt, va_list ptr);
int		vswprintf(wchar_t *buffer, const wchar_t *format, va_list argptr);
int		swprintf(wchar_t *buffer, const wchar_t *format, ...);

int		printf(const char* fmt, ...);
int		vprintf(const char* fmt, va_list ptr);
int		vsprintf(char *buffer, const char *format, va_list argptr);
int		sprintf(char *buffer, const char *format, ...);

wint_t	putwchar(wint_t c);
int		putchar(int c);
int		_cputws(const wchar_t* str);
int		_cputs(const char* str);

int		fprintf(FILE* file, const char* fmt, ...);

FILE*	fopen(const char* filename, const char* mode);
int		fclose(FILE* f);
size_t	fread (void* pBuffer, size_t sizeObject, size_t sizeObjCount,
		FILE* fileRead);
size_t	fwrite (const void* pObjArray, size_t sizeObject, size_t sizeObjCount,
		FILE* fileWrite);
int		fseek(FILE *stream, long offset, int origin);
long	ftell(FILE *stream);

int		fputc(int c, FILE *stream);
wint_t	fputwc(wint_t c, FILE *stream);

void	_pwerror(const wchar_t* string);

#define	SEEK_CUR    1
#define	SEEK_END    2
#define	SEEK_SET    0

//@}

#ifdef __cplusplus
}
#endif

#endif