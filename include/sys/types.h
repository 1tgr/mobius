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

#include "wchar.h"

#ifndef __cplusplus

#ifndef bool
//! A boolean (true or false) value
typedef byte bool;
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

#endif
