#include <conio.h>
#include <os/stream.h>

#if 0
extern IStream *keyboard;

//! Returns true if there is a key waiting in the keyboard buffer, false otherwise.
/*!
 *	To wait for a keypress while maintaining co-operation with other threads
 *		in the system, use of thrWaitHandle() is recommended, rather than 
 *		using _kbhit() in a loop.
 */
int _kbhit()
{
	return (IStream_IsReady(keyboard) == S_OK) ? 1 : 0;
}
#else
int _kbhit()
{
	return 0;
}
#endif