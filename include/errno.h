/* $Id: errno.h,v 1.8 2002/03/27 22:12:59 pavlovskii Exp $ */
/* Copyright (C) 1998 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#ifndef __dj_include_errno_h_
#define __dj_include_errno_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*extern int errno;*/
int *_geterrno(void);
#define errno	(*_geterrno())
  
#define SIOPENDING	-1
#define EDOM		1
#define EILSEQ		2
#define ERANGE		3
#define ENOTIMPL	4
#define EBUFFER		5
#define ENOTFOUND	6
#define EINVALID	7
#define EMFILE		8
#define ENOMEM		9
#define EHANDLE		10
#define EHARDWARE	11
#define EACCESS		12
#define EEOF		13
#define E2BIG		14
#define ENOTADIR	15
#define EFAULT		16
#define ENOCLIENT   17

#define EACCES		EACCESS
#define EINVAL		EINVALID
#define ENOENT		ENOTFOUND

#ifndef _POSIX_SOURCE

#define ENMFILE		38

extern char *		sys_errlist[];
extern wchar_t *	sys_werrlist[];
extern int		sys_nerr;
extern const char *	__sys_errlist[];
extern const wchar_t *	__sys_werrlist[];
extern int		__sys_nerr;

#endif /* !__dj_ENFORCE_ANSI_FREESTANDING */

#ifndef __dj_ENFORCE_FUNCTION_CALLS
#endif /* !__dj_ENFORCE_FUNCTION_CALLS */

#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_errno_h_ */
