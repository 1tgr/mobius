/* $Id: errno.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
/* Copyright (C) 1998 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#ifndef __dj_include_errno_h_
#define __dj_include_errno_h_

#ifdef __cplusplus
extern "C" {
#endif

extern int errno;
  
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

#define EACCES		EACCESS
#define EINVAL		EINVALID
#define ENOENT		ENOTFOUND

#ifndef _POSIX_SOURCE

#define ENMFILE		38

extern char *		sys_errlist[];
extern int		sys_nerr;
extern const char *	__sys_errlist[];
extern int		__sys_nerr;
extern int		_doserrno;

#endif /* !__dj_ENFORCE_ANSI_FREESTANDING */

#ifndef __dj_ENFORCE_FUNCTION_CALLS
#endif /* !__dj_ENFORCE_FUNCTION_CALLS */

#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_errno_h_ */
