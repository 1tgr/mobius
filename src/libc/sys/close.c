#include <io.h>
#include <os/syscall.h>

int __close(int _fd)
{
	return FsClose((handle_t) _fd) ? 0 : -1;
}
