/* $Id$ */

#include <gui/winmgr.h>
#include <gui/ipc.h>

#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <wchar.h>
#include <assert.h>

#include <os/rtl.h>

typedef struct ipc_packet_t ipc_packet_t;
struct ipc_packet_t
{
	uint32_t code;
	size_t extra_length;
	wnd_params_t params;
};


static bool IpcReadFromPipe(handle_t file, void *buf, size_t length)
{
    size_t bytes, total;

    total = 0;
	errno = 0;
    while (total < length)
    {
        //printf("FsRead(file=%u, length=%u-%u=%u)\n",
            //file, length, total, length - total);
        if (!FsRead(file, (char*) buf + total, 0, length - total, &bytes) ||
			(length > 0 && bytes == 0))
		{
			_wdprintf(L"%s: IpcReadFromPipe(%u): bytes = %u, errno = %d\n", 
				ProcGetProcessInfo()->module_first->name, file, bytes, errno);
            return false;
		}

        total += bytes;
    }

    return true;
}


bool IpcCallExtra(handle_t ipc, uint32_t code, const wnd_params_t *params, 
				  const void *extra, size_t extra_length)
{
	ipc_packet_t packet = { 0 };

	if (ipc == NULL)
		ipc = IpcGetDefault();

	packet.code = code;
	if (params != NULL)
		packet.params = *params;
	packet.extra_length = extra_length;
	errno = 0;
	if (!FsWrite(ipc, &packet, 0, sizeof(packet), NULL))
	{
		_wdprintf(L"%s: IpcCallExtra: FsWrite(1) failed: %s\n",
			ProcGetProcessInfo()->module_first->name,
			_wcserror(errno));
		//return false;
	}

	if (extra_length > 0)
	{
		if (!FsWrite(ipc, extra, 0, extra_length, NULL))
		{
			_wdprintf(L"%s: IpcCallExtra: FsWrite(2) failed: %s\n",
				ProcGetProcessInfo()->module_first->name,
				_wcserror(errno));
			//return false;
		}
	}

	return true;
}


bool IpcCall(handle_t ipc, uint32_t code, const wnd_params_t *params)
{
	return IpcCallExtra(ipc, code, params, NULL, 0);
}


uint32_t IpcReceiveExtra(handle_t ipc, wnd_params_t *params, void **extra, 
						 size_t *extra_length)
{
	ipc_packet_t packet;

	if (ipc == NULL)
		ipc = IpcGetDefault();

	*extra = NULL;
	*extra_length = 0;

	if (!IpcReadFromPipe(ipc, &packet, sizeof(packet)))
		return 0;

	assert((packet.code & 0x80000000) == 0);

	*params = packet.params;
	if (packet.extra_length > 0)
	{
		assert(packet.extra_length < 0x10000);

		*extra = malloc(packet.extra_length);
		if (*extra == NULL)
			return 0;

		*extra_length = packet.extra_length;

		if (!IpcReadFromPipe(ipc, *extra, *extra_length))
		{
			free(*extra);
			*extra_length = 0;
			return 0;
		}
	}

	return packet.code;
}


uint32_t IpcReceive(handle_t ipc, wnd_params_t *params)
{
	void *extra;
	size_t extra_length;
	uint32_t code;

	code = IpcReceiveExtra(ipc, params, &extra, &extra_length);
	if (code == 0)
		return 0;

	assert(extra_length == 0);
	free(extra);
	return code;
}
