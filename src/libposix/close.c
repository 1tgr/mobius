/* $Id: close.c,v 1.1 2002/05/05 13:53:47 pavlovskii Exp $ */

#include <io.h>
#include <os/syscall.h>

int close(int _fd)
{
	return FsClose((handle_t) _fd) ? 0 : -1;
}
