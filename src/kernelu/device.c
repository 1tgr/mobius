#include <os/device.h>
#include <os/os.h>

addr_t devOpen(const wchar_t* name, const wchar_t* params)
{
	addr_t r;
	asm("int $0x30" : "=a" (r) : "a" (0x500), "b" (name), "c" (params));
	return r;
}

bool devClose(addr_t dev)
{
	bool r;
	asm("int $0x30" : "=a" (r) : "a" (0x501), "b" (dev));
	return r;
}

status_t devUserRequest(addr_t dev, request_t* req, size_t size)
{
	status_t r;
	asm("int $0x30" : "=a" (r) : "a" (0x502), "b" (dev), "c" (req), "d" (size));
	return r;
}

status_t devUserFinishRequest(request_t* req, bool delete_event)
{
	status_t r;
	asm("int $0x30" : "=a" (r) : "a" (0x503), "b" (req), "c" (delete_event));
	return r;
}

size_t devReadSync(addr_t dev, qword pos, void* buffer, size_t length)
{
	request_t req;

	req.code = DEV_READ;
	req.params.read.buffer = buffer;
	req.params.read.length = length;
	req.params.read.pos = pos;
	
	if (devUserRequest(dev, &req, sizeof(req)) == 0)
		thrWaitHandle(&req.event, 1, true);

	devUserFinishRequest(&req, true);
	if (req.result)
		sysSetErrno(req.result);

	return req.params.read.length;
}

size_t devWriteSync(addr_t dev, qword pos, void* buffer, size_t length)
{
	request_t req;

	req.code = DEV_WRITE;
	req.params.write.buffer = buffer;
	req.params.write.length = length;
	req.params.write.pos = pos;
	
	if (devUserRequest(dev, &req, sizeof(req)) == 0)
		thrWaitHandle(&req.event, 1, true);

	devUserFinishRequest(&req, true);
	if (req.result)
		sysSetErrno(req.result);

	return req.params.write.length;
}

status_t devUserRequestSync(addr_t dev, request_t* req, size_t size)
{
	if (devUserRequest(dev, req, size) == 0)
		thrWaitHandle(&req->event, 1, true);
	devUserFinishRequest(req, true);
	return req->result;
}
