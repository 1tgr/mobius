#include <os/os.h>

//! Terminates the current process.
/*!
 *	Terminates all running threads and frees any resources (objects, files, 
 *		memory, etc.) opened by the process.
 *	\return	Does not return; the process is terminated.
 */
void procExit()
{
	asm("int $0x30" : : "a" (3));
}

//! Returns a handle to the current process.
/*! \return	A handle to the current process. */
dword procCurrentId()
{
	dword ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x201));
	return ret;
}

//! Returns a pointer to the command line passed to the current process.
/*!
 *	The pointer returned is not const and refers to an area within the 
 *		process's information block (or elsewhere, if it is too long to
 *		fit in the same 4KB page). Hence, it is possible to modify the
 *		process's command line.
 *	\return	A pointer to the command line passed to the current process.
 */
wchar_t* procCmdLine()
{
	return thrGetInfo()->process->cmdline;
}

//! Returns a pointer to the process's current working directory.
/*!
 *	The pointer returned is not const and refers to an area within the 
 *		process's information block. Modifying the buffer returned will
 *		change the process's working directory.
 *	\return	A pointer to the current process's working directory.
 */
wchar_t* procCwd()
{
	return thrGetInfo()->process->cwd;
}