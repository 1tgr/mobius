/* Copyright (C) 1999 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1998 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1996 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <libc/stubs.h>
#include <stdlib.h>
/*#include <unistd.h>
#include <fcntl.h>
#include <io.h>*/
#include <libc/atexit.h>
#include <os/rtl.h>

struct __atexit *__atexit_ptr = 0;

void (*__stdio_cleanup_hook)(void);

/* A hook to close down the file system extensions if any where opened.
   This does not force them to be linked in. */
void (*__FSEXT_exit_hook)(void) = NULL;

typedef void (*FUNC)(void);

void
exit(int status)
{
	extern void (*__DTOR_LIST__[])();
	void (**pfunc)() = __DTOR_LIST__;
	struct __atexit *a,*o;

	a = __atexit_ptr;
	__atexit_ptr = 0; /* to prevent infinite loops */
	while (a)
	{
		(a->__function)();
		o = a;
		a = a->__next;
		free(o);
	}

	/* Destructors should probably be called after functions registered
		with atexit(), this is the way it happens in Linux anyway. */
	while (*++pfunc)
		;
	while (--pfunc > __DTOR_LIST__)
		(*pfunc) ();


	/* Do this last so that everyone else may write to files
		during shutdown */
	if (__stdio_cleanup_hook)
		__stdio_cleanup_hook();

	ProcExitProcess(status);
}
