/* $Id: close.c,v 1.2 2002/03/07 16:26:04 pavlovskii Exp $ */

#include <io.h>
#include <os/syscall.h>

int __close(int _fd)
{
	return FsClose((handle_t) _fd) ? 0 : -1;
}
