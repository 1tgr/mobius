#include <unistd.h>
#include <os/syscall.h>

off_t lseek(int fd, off_t offset, int whence)
{
	return FsSeek((handle_t) fd, offset, whence);
}
