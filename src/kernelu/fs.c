#include <stdlib.h>
#include <os/os.h>
#include <os/fs.h>
#include <string.h>
#include <os/device.h>

addr_t fsOpen(const wchar_t* path)
{
	static wchar_t fullpath[MAX_PATH];
	addr_t ret;
	
	if (!fsFullPath(path, fullpath))
		return NULL;

	asm("int $0x30" : "=a" (ret) : "a" (0x700), "b" (fullpath));
	return ret;
}

bool fsClose(addr_t fd)
{
	bool ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x701), "b" (fd));
	return ret;
}

bool fsRequest(addr_t fd, request_t* req, size_t size)
{
	bool ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x702), "b" (fd), "c" (req), "d" (size));
	return ret;
}

bool fsRequestSync(addr_t fd, request_t* req, size_t size)
{
	if (!fsRequest(fd, req, size))
	{
		devUserFinishRequest(req, true);
		return false;
	}

	thrWaitHandle(&req->header.event, 1, true);
	devUserFinishRequest(req, true);
	if (req->header.result)
		sysSetErrno(req->header.result);

	return req->header.result == 0;
}

bool fsRead(addr_t fd, void* buffer, size_t* length)
{
	request_t req;
	
	req.header.code = FS_READ;
	req.params.fs_read.buffer = (addr_t) buffer;
	req.params.fs_read.length = *length;
	req.params.fs_read.fd = fd;

	if (!fsRequest(fd, &req, sizeof(req)))
	{
		devUserFinishRequest(&req, true);
		*length = req.params.fs_read.length;
		sysSetErrno(req.header.result);
		return false;
	}

	thrWaitHandle(&req.header.event, 1, true);
	devUserFinishRequest(&req, true);
	*length = req.params.fs_read.length;

	if (req.header.result)
		sysSetErrno(req.header.result);

	return req.header.result == 0;
}

bool fsWrite(addr_t fd, const void* buffer, size_t* length)
{
	request_t req;
	
	req.header.code = FS_WRITE;
	req.params.fs_write.buffer = (addr_t) buffer;
	req.params.fs_write.length = *length;
	req.params.fs_write.fd = fd;

	if (!fsRequest(fd, &req, sizeof(req)))
	{
		devUserFinishRequest(&req, true);
		*length = req.params.fs_read.length;
		return req.header.result;
	}

	thrWaitHandle(&req.header.event, 1, true);
	devUserFinishRequest(&req, true);

	if (req.header.result)
		sysSetErrno(req.header.result);

	*length = req.params.fs_read.length;
	return req.header.result == 0;
}

bool fsIoCtl(addr_t fd, dword code, void* params, size_t length)
{
	request_t req;
	req.header.code = FS_IOCTL;
	req.params.fs_ioctl.buffer = params;
	req.params.fs_ioctl.length = length;
	req.params.fs_ioctl.code = code;
	req.params.fs_ioctl.fd = fd;
	return fsRequestSync(fd, &req, sizeof(req));
}

bool fsSeek(addr_t fd, qword pos)
{
	bool ret;
	dword high, low;
	high = (dword) (pos >> 32);
	low = (dword) pos;
	asm("int $0x30" 
		: "=a" (ret) 
		: "a" (0x703), "b" (fd),  "c" (low), "d" (high));
	return ret;	
}

qword fsGetPosition(addr_t fd)
{
	dword pos;
	asm("int $0x30"
		: "=a" (pos)
		: "a" (0x704), "b" (fd)
		: "edx");
	return pos;
}

qword fsGetLength(addr_t fd)
{
	dword length;
	asm("int $0x30"
		: "=a" (length)
		: "a" (0x705), "b" (fd)
		: "edx");
	return length;
}