/* $Id: types.h,v 1.6 2002/05/05 13:46:33 pavlovskii Exp $ */
#ifndef __SYS_TYPES_H
#define __SYS_TYPES_H

#ifndef __cplusplus
/* bool is a keyword in C++ */
typedef char _Bool;

#ifndef __bool_true_false_are_defined
#define bool _Bool
#define false 0
#define true 1
#define __bool_true_false_are_defined 1
#endif

#endif

typedef signed char int8_t;
typedef short int16_t;
typedef long int32_t;
typedef long long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;

typedef uint32_t addr_t;
typedef unsigned int size_t;
typedef int ssize_t;
typedef long ptrdiff_t;

typedef addr_t off_t;
typedef uint64_t time_t;

#ifndef __cplusplus
typedef unsigned short wchar_t;
#endif

typedef int wint_t;
typedef void *va_list;

#ifdef __MOBIUS__
typedef addr_t handle_t;
typedef int32_t status_t;
#endif

#ifdef _POSIX_SOURCE

typedef unsigned gid_t;
typedef unsigned uid_t;
typedef unsigned pid_t;
typedef unsigned dev_t;
typedef unsigned ino_t;
typedef unsigned mode_t;
typedef unsigned nlink_t;

#endif
	
#endif
