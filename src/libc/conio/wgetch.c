#include <os/os.h>
#include <os/device.h>

//! Waits for a key to be pressed at the console.
/*!
 *	\return	The code of the key pressed. Codes 0-0xFFFF follow the Unicode UCS; 
 *		other codes are defined by the operating system and correspond to 
 *		non-printable keys on the keyboard.
 */
wint_t _wgetch()
{
	addr_t keyboard;
	request_t req;
	dword key;

	keyboard = devOpen(L"keyboard", NULL);
	if (keyboard == NULL)
		return 0;

	req.code = DEV_READ;
	req.params.read.buffer = &key;
	req.params.read.length = sizeof(key);
	if (!devUserRequest(keyboard, &req, sizeof(req)))
		return 0;

	thrWaitHandle(&req.event, 1, true);
	devUserFinishRequest(&req, true);

	devClose(keyboard);
	return key;
}
