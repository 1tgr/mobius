#include <conio.h>
#include <os/device.h>
#include <errno.h>

//! Returns true if there is a key waiting in the keyboard buffer, false otherwise.
/*!
 *	To wait for a keypress while maintaining co-operation with other threads
 *		in the system, use of thrWaitHandle() is recommended, rather than 
 *		using _kbhit() in a loop.
 */
int _kbhit()
{
	request_t req;
	size_t size;
	addr_t keyboard;

	keyboard = devOpen(L"keyboard", NULL);
	if (keyboard == NULL)
		return 0;

	req.code = CHR_GETSIZE;
	req.params.buffered.buffer = (addr_t) &size;
	req.params.buffered.length = sizeof(size);
	devUserRequestSync(keyboard, &req, sizeof(req));
	devClose(keyboard);

	if (req.result)
	{
		errno = req.result;
		return 0;
	}
	else
		return size;
}
