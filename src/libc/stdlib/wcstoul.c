/* Copyright (C) 1999 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <libc/unconst.h>
#include <wchar.h>

/*
 * Convert a string to an unsigned long integer.
 *
 * Ignores `locale' stuff.  Assumes that the upper and lower case
 * alphabets and digits are each contiguous.
 */
unsigned long
wcstoul(const wchar_t *nptr, wchar_t **endptr, int base)
{
  const wchar_t *s = nptr;
  unsigned long acc;
  int c;
  unsigned long cutoff;
  int neg = 0, any, cutlim;

  /*
   * See strtol for comments as to the logic used.
   */
  do {
    c = *s++;
  } while (iswspace(c & 0xff));
  if (c == '-')
  {
    neg = 1;
    c = *s++;
  }
  else if (c == '+')
    c = *s++;
  if ((base == 0 || base == 16) &&
      c == '0' && (*s == 'x' || *s == 'X'))
  {
    c = s[1];
    s += 2;
    base = 16;
  }
  if (base == 0)
    base = c == '0' ? 8 : 10;
  cutoff = (unsigned long)ULONG_MAX / (unsigned long)base;
  cutlim = (unsigned long)ULONG_MAX % (unsigned long)base;
  for (acc = 0, any = 0;; c = *s++, c &= 0xff)
  {
    if (iswdigit(c))
      c -= '0';
    else if (iswalpha(c))
      c -= iswupper(c) ? 'A' - 10 : 'a' - 10;
    else
      break;
    if (c >= base)
      break;
    if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
      any = -1;
    else {
      any = 1;
      acc *= base;
      acc += c;
    }
  }
  if (any < 0)
  {
    acc = ULONG_MAX;
    errno = ERANGE;
  }
  else if (neg)
    acc = -acc;
  if (endptr != 0)
    *endptr = any ? unconst(s, wchar_t *) - 1 : unconst(nptr, wchar_t *);
  return acc;
}
