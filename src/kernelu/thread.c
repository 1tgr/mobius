#include <os/os.h>

#if 0
//! Sets the thread's local storage pointer.
/*!
 *	The kernel reserves a 32-bits value for each thread for its own use.
 *	This function can be used to set that value.
 *
 *	\param	value	The 32-bit value to store for the current thread.
 */
void thrSetTls(dword value)
{
	asm("int $0x30" : : "a" (0x104) : "b" (value));
}

//! Retrieves the thread's local storage pointer.
/*!
 *	The kernel reserves a 32-bits value for each thread for its own use.
 *	This function can be used to retrieve that value.
 *
 *	\return	The 32-bit value associated with the current thread.
 */
dword thrGetTls()
{
	dword ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x105));
	return ret;
}
#endif

//! Calls a function in the context of the current thread.
/*!
 *	Pushes the parameters provided onto the thread's stack. The function
 *		will be called just before this function returns.
 *
 *	\param	addr	A pointer to the function to be executed. To avoid 
 *		stack corruption, this function must be declared __stdcall.
 *	\param	params	The parameters to be passed to the function. On the IA32
 *		architecture, these should be multiples of 32 bits in width and
 *		aligned on 32-bit boundaries.
 *	\param	sizeof_params	The size, in bytes, of the parameters pointed to
 *		by params. This should be a multiple of 32 bits on the IA32 
 *		architecture.
 */
void thrCall(void* addr, void* params, size_t sizeof_params)
{
	asm("int $0x30" : : "a" (0x103), "b" (addr), "c" (params), "d" (sizeof_params));
}

//! Pauses thread execution for the specified period.
/*!
 *	Puts the current thread into an efficient sleep state and schedules other
 *		threads until the specified time has elapsed.
 *	\param	ms	The time duration to sleep for, in milliseconds.
 */
void thrSleep(dword ms)
{
	asm("int $0x30" : : "a" (0x100), "b" (ms));
}

//! Pauses thread execution until the specified handle is signalled.
/*!
 *	Puts the current thread into an efficient sleep state and schedules other
 *		threads until the specified handle is signalled.
 *	\param	hnd	The handle to wait for.
 *	The meaning of "signalled" varies according to the type of handle used:
 *	+ Processes and threads are signalled when they terminate.
 */
unsigned thrWaitHandle(addr_t* hnd, unsigned num_handles, bool wait_all)
{
	unsigned r;
	asm("int $0x30" : "=a" (r) : "a" (0x101), "b" (hnd), "c" (num_handles), "d" (wait_all));
	return r;
}

//! Returns the ID of the current thread.
/*!
 *	\return The ID of the current thread. This is guaranteed to be unique
 *		within the current process.
 */
dword thrGetId()
{
	return thrGetInfo()->tid;
}

addr_t thrCreate(void* entry_point, dword param)
{
	addr_t ret;
	asm("int $0x30" : "=a" (ret) : "a" (0x107), "b" (entry_point), "c" (param));
	return ret;
}

void thrExit(int code)
{
	asm("int $0x30": : "a" (0x106), "b" (code));
}