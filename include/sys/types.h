#ifndef __SYS_TYPES_H
#define __SYS_TYPES_H

//! An unsigned 8-bit number
typedef unsigned char byte;
//! An unsigned 16-bit number
typedef unsigned short word;
//! An unsigned 32-bit number
typedef unsigned int dword;

#ifdef _MSC_VER
typedef unsigned __int64 qword;
#else
//! An unsigned 64-bit number
typedef unsigned long long qword;
#endif

//! An address that should not be dereferenced.
/*!
 *	Often used to denote physical memory addresses, addresses within another
 *		process's address space, or addresses outside of memory (for example,
 *		on a magnetic storage device).
 */
typedef dword addr_t;

#ifdef _MSC_VER
/*
 * Visual C++ expects (?) size_t to be unsigned int, whereas gcc's built-in 
 * string functions expect it to be unsigned long.
 */

#ifndef _SIZE_T_DEFINED
typedef unsigned int size_t;
#define _SIZE_T_DEFINED
#endif

#else

#ifndef _SIZE_T_DEFINED
//! The result of the sizeof operator.
/*!
 *	Describes the sizes of objects up to 4GB (2<sup>32</sup>) in size.
 */
typedef unsigned int size_t;
#define _SIZE_T_DEFINED
#endif

#endif

#ifndef __cplusplus

typedef byte _Bool;

#ifndef bool
//! A boolean (true or false) value
#define bool _Bool
#endif

#ifndef true
//! Boolean true
#define true	1
#endif

#ifndef false
//! Boolean false
#define false	0
#endif

#endif

#ifndef NULL
//! The NULL pointer
#define NULL	0
#endif

typedef int status_t;

#ifndef _WCHAR_T_DEFINED

typedef unsigned short __wchar_ucs2_t;

//! A single Unicode character
#define wchar_t __wchar_ucs2_t
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

#endif
