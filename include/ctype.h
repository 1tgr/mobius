#ifndef __CTYPE_H
#define __CTYPE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <wchar.h>

int iswupper(wint_t c);
int iswlower(wint_t c);
int towupper(wint_t c);
int towlower(wint_t c);

#define iswdigit(c)	((c) >= '0' && (c) <= '9')
#define iswspace(c)	((c) == ' ' || (c) == '\n' || (c) == '\t' || (c) == '\r')
#define iswalpha(c)	(((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))

int isupper(int c);
int islower(int c);
int toupper(int c);
int tolower(int c);

#ifdef __cplusplus
}
#endif

#endif