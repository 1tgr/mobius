/* Copyright (C) 1997 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1996 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <libc/stubs.h>
#include <stdio.h>
#include <libc/file.h>
#include <os/syscall.h>
#include <os/rtl.h>

static int do_seek(FILE *f, long offset, int ptrname)
{
	uint64_t length;
	int64_t new_offset;

	if (!FsGetFileLength(fileno(f), &length))
		length = 0;

	switch (ptrname)
	{
	case SEEK_SET:
		new_offset = offset;
		break;

	case SEEK_CUR:
		new_offset = f->_offset + offset;
		break;

	case SEEK_END:
		new_offset = length - offset;
		break;
	}

	if (new_offset < 0 || new_offset > length)
		return -1;
	else
	{
		f->_offset = new_offset;
		return 0;
	}
}

int
fseek(FILE *f, long offset, int ptrname)
{
	long p = -1;			/* can't happen? */

	/* See comment in filbuf.c */
	f->_fillsize = 512;

	f->_flag &= ~_IOEOF;
	if (f->_flag & _IOREAD)
	{
		if (f->_base && !(f->_flag & _IONBF))
		{
			p = ftell(f);
			if (ptrname == SEEK_CUR)
			{
				offset += p;
				ptrname = SEEK_SET;
			}
			/* check if the target position is in the buffer and
			optimize seek by moving inside the buffer */
			if (ptrname == SEEK_SET && (f->_flag & (_IOUNGETC|_IORW)) == 0
				&& p-offset <= f->_ptr-f->_base && offset-p <= f->_cnt)
			{
				f->_ptr+=offset-p;
				f->_cnt+=p-offset;
				return 0;
			}
		}

		if (f->_flag & _IORW)
			f->_flag &= ~_IOREAD;

		p = do_seek(f, offset, ptrname);
		f->_cnt = 0;
		f->_ptr = f->_base;
		f->_flag &= ~_IOUNGETC;
	}
	else if (f->_flag & (_IOWRT|_IORW))
	{
		p = fflush(f);
		return do_seek(f, offset, ptrname) == -1 || p == EOF ?
			-1 : 0;
	}
	return p==-1 ? -1 : 0;
}
