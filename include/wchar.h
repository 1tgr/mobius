#ifndef __WCHAR_H
#define __WCHAR_H

#ifdef __cplusplus
extern "C"
{
#endif

/*!
 *  \ingroup	libc
 *  \defgroup	wchar	Wide character routines
 *  @{
 */

#include <sys/types.h>

//! Describes attributes relating to a particular Unicode code point
struct wchar_info_t
{
	//! Specifies the 16-bit value of the character
	wchar_t code_point;
	//! Specifies the classes to which the character belongs
	unsigned short char_class;
	//! Specifes the same character in upper case, if appropriate
	wchar_t upper, 
	//! Specifes the same character in lower case, if appropriate
		lower;
};
typedef struct wchar_info_t wchar_info_t;

/* Wide-character functions */
size_t	wcslen(const wchar_t* str);
wchar_t	*wcscpy(wchar_t *strDestination, const wchar_t *strSource);
wchar_t	*wcsncpy(wchar_t *strDestination, const wchar_t *strSource, size_t count);
int	wcscmp(const wchar_t* str1, const wchar_t* str2);
int	wcsicmp(const wchar_t* str1, const wchar_t* str2);
wchar_t *wcschr(const wchar_t* str, int c);
wchar_t *wcsrchr(const wchar_t* str, int c);
wchar_t *wcscat(wchar_t* dest, const wchar_t* src);
wchar_t *wcsdup(const wchar_t* str);
wchar_t *wcspbrk(const wchar_t *s1, const wchar_t *s2);

/* ANSI/wide conversion functions */
size_t	mbstowcs(wchar_t *wcstr, const char *mbstr, size_t count);
size_t	wcstombs(char *mbstr, const wchar_t *wcstr, size_t count);
wchar_t	*_wcserror(int errcode);

//@}

#ifdef __cplusplus
}
#endif

#endif
