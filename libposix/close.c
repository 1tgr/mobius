/* $Id: close.c,v 1.1 2002/12/21 09:50:19 pavlovskii Exp $ */

#include <io.h>
#include <os/syscall.h>

int close(int _fd)
{
	return FsClose((handle_t) _fd) ? 0 : -1;
}
