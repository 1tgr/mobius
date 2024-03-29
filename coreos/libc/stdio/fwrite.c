/* Copyright (C) 1999 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1996 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libc/file.h>

size_t
fwrite(const void *vptr, size_t size, size_t count, FILE *f)
{
  const char *ptr = (const char *)vptr;
  register int s;

#if 0
  if (__libc_write_termios_hook
      && (f->_flag & (_IOTERM | _IONTERM)) == 0)
  {
    /* first time we see this handle--see if termios hooked it */
    /* xxx - need to replace isatty somehow */
    if (isatty(f->_file))
      f->_flag |= _IOTERM;
    else
      f->_flag |= _IONTERM;
  }
#endif

  s = size * count;
  /*if(!__is_text_file(f))*/
  {
    if (f->_flag & _IOLBF)
      while (s > 0) {
	if (--f->_cnt > -f->_bufsiz && *(const char *)ptr != '\n')
	  *f->_ptr++ = *(const char *)ptr++;
	else if (_flsbuf(*(const char *)ptr++, f) == EOF)
	  break;
	s--;
      }
    else while (s > 0) {
      if (f->_cnt < s) {
	if (f->_cnt > 0) {
	  memcpy(f->_ptr, ptr, f->_cnt);
	  ptr += f->_cnt;
	  f->_ptr += f->_cnt;
	  s -= f->_cnt;
	}
	if (_flsbuf(*(const unsigned char *)ptr++, f) == EOF)
	  break;
	s--;
      }
      if (f->_cnt >= s) {
	memcpy(f->_ptr, ptr, s);
	f->_ptr += s;
	f->_cnt -= s;
	return count;
      }
    }
  }
#if 0
  else
  {
    if (f->_flag & _IOLBF)
    {
      while (s > 0) {
        if (*ptr=='\n')
        {
          if (--f->_cnt > -f->_bufsiz)
            *f->_ptr++ = '\r';
          else
            if (_flsbuf('\r', f) == EOF)
	      break;
        }
	if (--f->_cnt > -f->_bufsiz && *ptr != '\n')
	  *f->_ptr++ = *ptr++;
	else if (_flsbuf(*(const unsigned char *)ptr++, f) == EOF)
	  break;
	s--;
      }
    }
    else
    {
      while (s > 0)
      {
	if (*ptr == '\n')
	{
	  if(--f->_cnt >= 0)
	    *f->_ptr++ = '\r';
	  else
	    if (_flsbuf('\r', f) == EOF)
	      break;
	}
	if (--f->_cnt >= 0)
	  *f->_ptr++ = *ptr++;
	else
	  if (_flsbuf(*(const unsigned char *)ptr++, f) == EOF)
	    break;
	s--;
      }
    }
  }
#endif
  return size != 0 ? count - ((s + size - 1) / size) : 0;
}
