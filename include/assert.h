/* $Id: assert.h,v 1.1.1.1 2002/12/31 01:26:19 pavlovskii Exp $ */
/* Copyright (C) 1997 DJ Delorie, see COPYING.DJ for details */
#ifndef __dj_ENFORCE_ANSI_FREESTANDING

#ifndef __dj_include_assert_h_
#define __dj_include_assert_h_

#ifdef __cplusplus
extern "C" {
#endif

#undef assert

#if defined(NDEBUG)
#define assert(test) ((void)0) 
#else

#ifdef KERNEL

void assert_failed(const char *test, const char *file, int line);
#define assert(test) \
	do \
	{ \
		if ((test) == 0) \
		{ \
			assert_failed(#test, __FILE__, __LINE__); \
			__asm__("int3"); \
		} \
	} while (0)

#else

void	__dj_assert(const char *,const char *,int);
#define assert(test) ((void)((test)||(__dj_assert(#test,__FILE__,__LINE__),0)))

#endif
#endif


#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_assert_h_ */

#endif /* !__dj_ENFORCE_ANSI_FREESTANDING */
