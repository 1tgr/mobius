/* $Id: stdarg.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
/* Copyright (C) 1996 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#ifndef __dj_include_stdarg_h_
#define __dj_include_stdarg_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#define __dj_va_rounded_size(T)  \
  (((sizeof (T) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

#define va_arg(ap, T) \
    (ap = (va_list) ((char *) (ap) + __dj_va_rounded_size (T)),	\
     *((T *) (void *) ((char *) (ap) - __dj_va_rounded_size (T))))

#define va_end(ap)

#define va_start(ap, last_arg) \
 (ap = ((va_list) __builtin_next_arg (last_arg)))
  
#ifdef __cplusplus
}
#endif

#endif /* !__dj_include_stdarg_h_ */
