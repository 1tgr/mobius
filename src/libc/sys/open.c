#include <io.h>
#include <fcntl.h>
#include <os/syscall.h>
#include <os/defs.h>

wchar_t *_towc(const char *mb);

int __open(const char *path, int oflag)
{
	wchar_t *wc = _towc(path);
	uint32_t mode;
	handle_t (*fn)(const wchar_t*, uint32_t);

	if (mode & _O_CREAT)
		fn = FsCreate;
	else
		fn = FsOpen;

	if (mode & _O_RDWR)
		mode = FILE_READ | FILE_WRITE;
	else if (mode & _O_WRONLY)
		mode = FILE_WRITE;
	else
		mode = FILE_READ;
	
	return fn(wc, mode);
}