/*
 * $History: lmutex.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 14:15
 * Updated in $/coreos/libsys
 * Added #define to choose between fast and slow implementations
 * Tidied up
 * Added history block
 */

#include <os/rtl.h>
#include <os/syscall.h>

#define FAST

void LmuxInit(lmutex_t *mux)
{
    mux->locks = 0;
    mux->eip = 0;
    mux->owner = 0;
#ifdef FAST
    mux->mutex = SemCreate(0);
#else
    mux->mutex = SemCreate(1);
#endif
}


void LmuxDelete(lmutex_t *mux)
{
    HndClose(mux->mutex);
}


#ifdef FAST

void LmuxAcquire(lmutex_t *mux)
{
    unsigned my_id;
    volatile lmutex_t *vmux = (volatile lmutex_t*) mux;

    my_id = ThrGetThreadInfo()->id;

    if (vmux->owner == my_id)
    {
	_wdprintf(L"LmuxAcquire(%p/%u): recursive acquisition by %u\n",
	    vmux, vmux->mutex, my_id);
	__asm__("int3");
    }

    if (SysIncrement(&vmux->locks) > 0)
    {
	if (!SemDown(vmux->mutex))
	    __asm__("int3");

	if (vmux->owner != 0)
	{
	    _wdprintf(L"LmuxAcquire(%p/%u): still locked by %u, %u contend\n",
		vmux, vmux->mutex, vmux->owner, my_id);
	    __asm__("int3");
	}

	vmux->eip = *(&mux - 1);
	vmux->locks--;
	vmux->owner = my_id;
    }
    else
    {
	vmux->eip = *(&mux - 1);
	vmux->owner = my_id;
    }
}


void LmuxRelease(lmutex_t *mux)
{
    unsigned my_id;

    my_id = ThrGetThreadInfo()->id;
    if (mux->locks > 1)
    {
	if (mux->owner != my_id)
	{
	    _wdprintf(L"LmuxRelease(%p/%u): incorrect owner %u, should be %u\n",
		mux, mux->mutex, mux->owner, my_id);
	    __asm__("int3");
	}

	mux->eip = 0;
	mux->owner = 0;
	if (!SemUp(mux->mutex))
	    __asm__("int3");
    }
    else
    {
	mux->eip = 0;
	mux->locks = 0;
	mux->owner = 0;
    }
}

#else

void LmuxAcquire(lmutex_t *mux)
{
    SemDown(mux->mutex);
}


void LmuxRelease(lmutex_t *mux)
{
    SemUp(mux->mutex);
}

#endif
