/* $Id: types.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
#ifndef __SYS_TYPES_H
#define __SYS_TYPES_H

typedef char _Bool;

#ifndef __bool_true_false_are_defined
#define bool _Bool
#define false 0
#define true 1
#define __bool_true_false_are_defined 1
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
typedef long ptrdiff_t;

#ifndef __cplusplus
typedef unsigned short wchar_t;
#endif

typedef int wint_t;
typedef void *va_list;
typedef addr_t handle_t;
	
#endif