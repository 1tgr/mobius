/* $Id: internal.h,v 1.1 2002/12/21 09:50:04 pavlovskii Exp $ */

#ifndef INTERNAL_H__
#define INTERNAL_H__

#ifdef WIDE

#define MAIN        wmain
#define CHAR        wchar_t
#define _istspace   iswspace
#define _tcslen     wcslen
#define __crt0_parseargs    __crt0_parseargs_wide

#else

#define MAIN                main
#define CHAR                char
#define _istspace           isspace
#define _tcslen             strlen
#define __crt0_parseargs    __crt0_parseargs_char

#endif

int MAIN(int argc, CHAR **argv);
void __main(void);

int __crt0_parseargs_wide(CHAR ***argv);
int __crt0_parseargs_char(CHAR ***argv);

typedef struct {
  unsigned mantissa:23;
  unsigned exponent:8;
  unsigned sign:1;
} float_t;

typedef struct {
  unsigned mantissal:32;
  unsigned mantissah:20;
  unsigned exponent:11;
  unsigned sign:1;
} double_t;

typedef struct {
  unsigned mantissal:32;
  unsigned mantissah:32;
  unsigned exponent:15;
  unsigned sign:1;
} long_double_t;

#endif
