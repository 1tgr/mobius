/* $Id: internal.h,v 1.1 2002/08/05 17:10:45 pavlovskii Exp $ */

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

#endif
