#include <os/os.h>
#include <os/pe.h>

void conClose();

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
addr_t procLoad(const wchar_t* filespec, const wchar_t* cmdline, 
				unsigned priority, addr_t input, addr_t output)
{
	dword ret;
	asm("int $0x30" 
		: "=a" (ret) 
		: "a" (0x200), "b" (filespec), "c" (cmdline), "d" (priority),
			"S" (input), "D" (output));
	return ret;
}

//! Terminates the current process.
/*!
 *	Terminates all running threads and frees any resources (objects, files, 
 *		memory, etc.) opened by the process.
 *	\return	Does not return; the process is terminated.
 */
void procExit()
{
	/*process_info_t* proc = thrGetInfo()->process;
	module_info_t* mod;
	IMAGE_DOS_HEADER* dos;
	IMAGE_PE_HEADERS* pe;

	for (mod = proc->module_first; mod; mod = mod->next)
	{
		dos = (IMAGE_DOS_HEADER*) mod->base;
		pe = (IMAGE_PE_HEADERS*) (mod->base + dos->e_magic);
	}*/

	conClose();
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

addr_t procBase()
{
	return thrGetInfo()->process->base;
}