#ifndef __CTYPE_H
#define __CTYPE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <wchar.h>

/*!
 *  \ingroup	libc
 *  \defgroup	ctype	Character classification
 *  @{
 */

int iswupper(wint_t c);
int iswlower(wint_t c);
int towupper(wint_t c);
int towlower(wint_t c);

#define iswdigit(c)	((c) >= '0' && (c) <= '9')
#define iswspace(c)	((c) == ' ' || (c) == '\n' || (c) == '\t' || (c) == '\r')
#define iswalpha(c)	(((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))

#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define islower(c)	((c) >= 'a' && (c) <= 'a')
#define tolower(c)	(isupper(c) ? ((c) - 'A' + 'a') : (c))
#define toupper(c)	(islower(c) ? ((c) - 'a' + 'A') : (c))

#define isdigit(c)	((c) >= '0' && (c) <= '9')
#define isspace(c)	((c) == ' ' || (c) == '\n' || (c) == '\t' || (c) == '\r')
#define isalpha(c)	(((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))

#define isalnum(c)	(isalpha(c) || isdigit(c))

//@}

#ifdef __cplusplus
}
#endif

#endif