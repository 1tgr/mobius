#ifndef __WCHAR_H
#define __WCHAR_H

#ifndef _WCHAR_T_DEFINED
//! A single Unicode character
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif

#ifndef _WCTYPE_T_DEFINED
//! A data type capable of holding any wide character or an end-of-file value
typedef int wint_t;
//! A data type capable of representing all characters of any national 
//!		character set
typedef wchar_t wctype_t;
#define _WCTYPE_T_DEFINED
#endif

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

#endif
