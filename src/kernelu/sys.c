#include <os/os.h>

//! Sets the last error code for the current thread.
/*!
 *	The last error code is set by virtually all system functions on failure. 
 *	Use sysSetErrno to emulate this behaviour.
 *
 *	\param	errno	The error number code to set. C errno codes are 
 *		supported, as well as the system codes.
 */
void sysSetErrno(int errno)
{
	thrGetInfo()->last_error = errno;
}

//! Returns the error code set by the last failed function.
/*! \return The error code set by the last failed function. */
int sysErrno()
{
	return thrGetInfo()->last_error;
}

//! Returns the number of milliseconds elapsed since the system booted.
/*!
 *	This function is useful for measuring an elapsed period of time, such as a 
 *		timeout.
 */
dword sysUpTime()
{
	dword ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x102));
	return ret;
}

//! Retrieves the error message corresponding to the code provided;
bool sysGetErrorText(status_t hr, wchar_t* text, size_t max)
{
	return resLoadString(0x10030000, (word) hr, text, max);
}