/* Copyright (C) 1999-2001 Free Software Foundation, Inc.
   This file is part of the GNU LIBICONV Library.

   The GNU LIBICONV Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   The GNU LIBICONV Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU LIBICONV Library; see the file COPYING.LIB.
   If not, write to the Free Software Foundation, Inc., 59 Temple Place -
   Suite 330, Boston, MA 02111-1307, USA.  */

/* When installed, this file is called "iconv.h". */

#ifndef _LIBICONV_H
#define _LIBICONV_H

#define _LIBICONV_VERSION 0x0107    /* version number: (major<<8) + minor */

/* We would like to #include any system header file which could define
   iconv_t, 1. in order to eliminate the risk that the user gets compilation
   errors because some other system header file includes /usr/include/iconv.h
   which defines iconv_t or declares iconv after this file, 2. when compiling
   for LIBICONV_PLUG, we need the proper iconv_t type in order to produce
   binary compatible code.
   But gcc's #include_next is not portable. Thus, once libiconv's iconv.h
   has been installed in /usr/local/include, there is no way any more to
   include the original /usr/include/iconv.h. We simply have to get away
   without it.
   Ad 1. The risk that a system header file does
   #include "iconv.h"  or  #include_next "iconv.h"
   is small. They all do #include <iconv.h>.
   Ad 2. The iconv_t type is a pointer type in all cases I have seen. (It
   has to be a scalar type because (iconv_t)(-1) is a possible return value
   from iconv_open().) */

/* Define iconv_t ourselves. */
#undef iconv_t
#define iconv_t libiconv_t
typedef void* iconv_t;

/* Get size_t declaration. */
#include <stddef.h>

/* Get errno declaration and values. */
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Allocates descriptor for code conversion from encoding `fromcode' to
   encoding `tocode'. */
extern iconv_t iconv_open (const char* tocode, const char* fromcode);

/* Converts, using conversion descriptor `cd', at most `*inbytesleft' bytes
   starting at `*inbuf', writing at most `*outbytesleft' bytes starting at
   `*outbuf'.
   Decrements `*inbytesleft' and increments `*inbuf' by the same amount.
   Decrements `*outbytesleft' and increments `*outbuf' by the same amount. */
extern size_t iconv (iconv_t cd, const char* * inbuf, size_t *inbytesleft, char* * outbuf, size_t *outbytesleft);

/* Frees resources allocated for conversion descriptor `cd'. */
extern int iconv_close (iconv_t cd);


#ifdef __cplusplus
}
#endif


#endif /* _LIBICONV_H */
