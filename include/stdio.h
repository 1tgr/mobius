/* $Id: stdio.h,v 1.2 2003/06/22 15:50:13 pavlovskii Exp $ */
/* Copyright (C) 1998 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */

#ifndef __dj_include_stdio_h_
#define __dj_include_stdio_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/version.h>
#include <sys/types.h>
  
#define _IOFBF          00001
#define _IONBF          00002
#define _IOLBF          00004

/* Some programs think they know better... */
#undef NULL

#define BUFSIZ          1024
#define EOF             (-1)
#define FILENAME_MAX    260
#define FOPEN_MAX       20
#define L_tmpnam        260
#define NULL            0
#define TMP_MAX         999999

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

typedef unsigned long long fpos_t;

/* Note that the definitions of these fields are NOT guaranteed!  They
   may change with any release without notice!  The fact that they
   are here at all is to comply with ANSI specifictions. */

#ifndef _FILE_DEFINED
typedef struct FILE {
  int      _cnt;
  char    *_ptr;
  char    *_base;
  int      _bufsiz;
  int      _flag;
  unsigned _file;
  char    *_name_to_remove;
  int      _fillsize;
  fpos_t   _offset;
} FILE;
#define _FILE_DEFINED
#endif

FILE *__get_stdin(void), *__get_stdout(void), *__get_stderr(void);

#ifdef __LIBC_STATIC
extern FILE __dj_stdin, __dj_stdout, __dj_stderr;
#define stdin   (&__dj_stdin)
#define stdout  (&__dj_stdout)
#define stderr  (&__dj_stdout)
#else
#define stdin   (__get_stdin())
#define stdout  (__get_stdout())
#define stderr  (__get_stderr())
#endif

void    clearerr(FILE *_stream);
int     fclose(FILE *_stream);
int     feof(FILE *_stream);
int     ferror(FILE *_stream);
int     fflush(FILE *_stream);
int     fgetc(FILE *_stream);
int     fgetpos(FILE *_stream, fpos_t *_pos);
char *  fgets(char *_s, int _n, FILE *_stream);
FILE *  fopen(const char *_filename, const char *_mode);
int     fprintf(FILE *_stream, const char *_format, ...);
int     fputc(int _c, FILE *_stream);
int     fputs(const char *_s, FILE *_stream);
size_t  fread(void *_ptr, size_t _size, size_t _nelem, FILE *_stream);
FILE *  freopen(const char *_filename, const char *_mode, FILE *_stream);
int     fscanf(FILE *_stream, const char *_format, ...);
int     fseek(FILE *_stream, long _offset, int _mode);
int     fsetpos(FILE *_stream, const fpos_t *_pos);
long    ftell(FILE *_stream);
size_t  fwrite(const void *_ptr, size_t _size, size_t _nelem, FILE *_stream);
int     getc(FILE *_stream);
int     getchar(void);
char *  gets(char *_s);
void    perror(const char *_s);
int     printf(const char *_format, ...);
int     putc(int _c, FILE *_stream);
int     putchar(int _c);
int     puts(const char *_s);
int     remove(const char *_filename);
int     rename(const char *_old, const char *_new);
void    rewind(FILE *_stream);
int     scanf(const char *_format, ...);
void    setbuf(FILE *_stream, char *_buf);
int     setvbuf(FILE *_stream, char *_buf, int _mode, size_t _size);
int     sprintf(char *_s, const char *_format, ...);
int     sscanf(const char *_s, const char *_format, ...);
FILE *  tmpfile(void);
char *  tmpnam(char *_s);
int     ungetc(int _c, FILE *_stream);
int     vfprintf(FILE *_stream, const char *_format, va_list _ap);
int     vprintf(const char *_format, va_list _ap);
int     vsprintf(char *_s, const char *_format, va_list _ap);

int     wprintf(const wchar_t *_format, ...);
FILE    *_wfopen( const wchar_t *filename, const wchar_t *mode );

/* This isn't ANSI at all... */
int     _cputws(const wchar_t *str, size_t count);
int     _wdprintf(const wchar_t *fmt, ...);

#ifdef _POSIX_SOURCE
FILE    *fdopen(int fildes, const char *mode);
#endif

#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_stdio_h_ */
