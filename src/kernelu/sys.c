#include <os/os.h>

//! Starts a new process
/*!
 *	Creates a new process, loads the specified executable, and starts
 *		execution, passing the command line provided.
 *
 *	\param	filespec	The path to the executable required.
 *	\param	cmdline		The command line to pass to the process, or NULL if
 *		no command line is needed.
 *	\return	A handle to the new process, or NULL if the process could not
 *		be started.
 */
dword sysExec(const wchar_t* filespec, const wchar_t* cmdline)
{
	dword ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x200), "b" (filespec), "c" (cmdline));
	return ret;
}

//! Opens an object in the kernel's name space
/*! The kernel's name space contains drivers and system objects. Use fsOpen() 
 *		to open user-mode objects.
 *	\param	filespec	The full path of the object to be opened.
 *
 *	\return	A pointer to the object opened, or NULL if the object could not 
 *		be found.
 */
addr_t sysOpen(const wchar_t* filespec)
{
	addr_t ret;
	asm("int $0x30" : "=a" (ret) : "a" (8), "b" (filespec));
	return ret;
}

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