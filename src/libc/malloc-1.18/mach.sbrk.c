/*
 * sbrk emulation for Mach.  Untested.  Note, this cannot be used with
 * _mal_sbrk, because it may trip the test for sbrk continually growing
 * upward.
 */

#include <mach.h>

caddr_t *
sbrk(incr)
int incr;
{
	kern_return_t rtn;
	vm_address_t data1;
	vm_size_t sz = incr;

	if ((rtn = vm_allocate(task_self(), &data1, sz, TRUE))
	     != KERN_SUCCESS) {
		return (caddr_t) -1;
	}
	return (caddr_t) data1;
}

int
getpagesize()
{
	return vm_page_size;
}
