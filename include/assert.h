/* $Id: assert.h,v 1.3 2002/08/14 16:30:53 pavlovskii Exp $ */
/* Copyright (C) 1997 DJ Delorie, see COPYING.DJ for details */
#ifndef __dj_ENFORCE_ANSI_FREESTANDING

#undef assert

#if defined(NDEBUG)
#define assert(test) ((void)0) 
#else
#define assert(test) ((void)((test)||(__dj_assert(#test,__FILE__,__LINE__),0)))
#endif

#ifndef __dj_include_assert_h_
#define __dj_include_assert_h_

#ifdef __cplusplus
extern "C" {
#endif

void	__dj_assert(const char *,const char *,int);

#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_assert_h_ */

#endif /* !__dj_ENFORCE_ANSI_FREESTANDING */
