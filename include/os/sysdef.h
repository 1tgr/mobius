/* $Id: sysdef.h,v 1.14 2002/08/04 17:22:39 pavlovskii Exp $ */
#ifdef KERNEL

/* The kernel uses different names for some functions... */
#define ThrWaitHandle   SysThrWaitHandle
#define ThrSleep        SysThrSleep
#define ThrCreateThread SysThrCreateThread
#define EvtAlloc        SysEvtAlloc
#define EvtSignal       SysEvtSignal
#define EvtIsSignalled  SysEvtIsSignalled
#define HndClose        SysHndClose
#define VmmFree         SysVmmFree
#endif

#ifndef SYS_BEGIN_GROUP
#define SYS_BEGIN_GROUP(n)
#endif

#ifndef SYS_END_GROUP
#define SYS_END_GROUP(n)
#endif

/*!
 *    \ingroup    libsys
 *    \defgroup    sys    Syscall Interface
 *    @{
 */

#define SYS_DbgWrite            0x100
#define SYS_Hello               0x101
#define SYS_SysUpTime           0x102
#define SYS_SysGetInfo          0x103
#define SYS_SysGetTimes         0x104
#define SYS_SysShutdown         0x105
#define SYS_KeLeakBegin         0x106
#define SYS_KeLeakEnd           0x107
#define SYS_SysYield            0x108
#define SYS_KeSetSingleStep     0x109

#define SYS_ThrExitThread       0x200
#define SYS_ThrWaitHandle       0x201
#define SYS_ThrSleep            0x202
#define SYS_ThrCreateV86Thread  0x203
#define SYS_ThrGetV86Context    0x204
#define SYS_ThrSetV86Context    0x205
#define SYS_ThrContinueV86      0x206
#define SYS_ThrCreateThread     0x207

#define SYS_ProcExitProcess     0x300
#define SYS_ProcSpawnProcess    0x301

#define SYS_FsCreate            0x400
#define SYS_FsOpen              0x401
#define SYS_FsClose             0x402
#define SYS_FsRead              0x403
#define SYS_FsWrite             0x404
#define SYS_FsSeek              0x405
/*efine SYS_FsOpenSearch        0x406*/
#define SYS_FsOpenDir           0x406
#define SYS_FsQueryFile         0x407
#define SYS_FsRequestSync       0x408
#define SYS_FsIoCtl             0x409
#define SYS_FsReadDir           0x40a
#define SYS_FsQueryHandle       0x40b

#define SYS_VmmAlloc            0x500
#define SYS_VmmFree             0x501
#define SYS_VmmMapShared        0x502
#define SYS_VmmMapFile          0x503

#define SYS_EvtAlloc            0x600
#define SYS_HndClose            0x601
#define SYS_EvtSignal           0x602
#define SYS_EvtIsSignalled      0x603

#define SYS_WndCreate           0x700
#define SYS_WndClose            0x701
#define SYS_WndPostMessage      0x702
#define SYS_WndInvalidate       0x703
#define SYS_WndGetMessage       0x704
#define SYS_WndGetAttribute     0x705
#define SYS_WndSetAttribute     0x706
#define SYS_WndOwnRoot          0x707
#define SYS_WndQueueInput       0x708
#define SYS_WndSetFocus         0x709
#define SYS_WndHasFocus         0x70a
#define SYS_WndSetCapture       0x70b
#define SYS_WndGetClip          0x70c

/* 0 */
SYS_BEGIN_GROUP(0)
SYS_END_GROUP(0)

/* 1 */
SYS_BEGIN_GROUP(1)
SYSCALL(int, DbgWrite, 8, const wchar_t*, size_t)
SYSCALL(int, Hello, 8, int, int)
SYSCALL(unsigned, SysUpTime, 0, void)
SYSCALL(bool, SysGetInfo, 4, struct sysinfo_t *)
SYSCALL(bool, SysGetTimes, 4, struct systimes_t *)
SYSCALL(bool, SysShutdown, 4, unsigned)
SYSCALL(void, KeLeakBegin, 0, void)
SYSCALL(void, KeLeakEnd, 0, void)
SYSCALL(void, SysYield, 0, void)
SYSCALL(void, KeSetSingleStep, 4, bool)
SYS_END_GROUP(1)

/* 2 */
SYS_BEGIN_GROUP(2)
SYSCALL(void, ThrExitThread, 4, int)
SYSCALL(bool, ThrWaitHandle, 4, handle_t)
SYSCALL(void, ThrSleep, 4, unsigned)
SYSCALL(handle_t, ThrCreateV86Thread, 16, uint32_t, uint32_t, unsigned, void (*)(void))
SYSCALL(bool, ThrGetV86Context, 4, struct context_v86_t*)
SYSCALL(bool, ThrSetV86Context, 4, const struct context_v86_t*)
SYSCALL(bool, ThrContinueV86, 0, void)
SYSCALL(handle_t, ThrCreateThread, 12, void (*)(void), void*, unsigned)
SYS_END_GROUP(2)

/* 3 */
SYS_BEGIN_GROUP(3)
SYSCALL(void, ProcExitProcess, 4, int)
SYSCALL(handle_t, ProcSpawnProcess, 8, const wchar_t*, const struct process_info_t *)
SYS_END_GROUP(3)

/* 4 */
SYS_BEGIN_GROUP(4)
SYSCALL(handle_t, FsCreate, 8, const wchar_t*, uint32_t)
SYSCALL(handle_t, FsOpen, 8, const wchar_t*, uint32_t)
SYSCALL(bool, FsClose, 4, handle_t)
SYSCALL(bool, FsRead, 16, handle_t, void*, size_t, struct fileop_t *)
SYSCALL(bool, FsWrite, 16, handle_t, const void*, size_t, struct fileop_t *)
SYSCALL(off_t, FsSeek, 12, handle_t, off_t, unsigned)
/*SYSCALL(handle_t, FsOpenSearch, 4, const wchar_t*)*/
SYSCALL(handle_t, FsOpenDir, 4, const wchar_t*)
SYSCALL(bool, FsQueryFile, 16, const wchar_t*, uint32_t, void *, size_t)
SYSCALL(bool, FsRequestSync, 20, handle_t, uint32_t, void *, size_t, struct fileop_t*)
SYSCALL(bool, FsIoCtl, 20, handle_t, uint32_t, void *, size_t, struct fileop_t*)
SYSCALL(bool, FsReadDir, 12, handle_t, struct dirent_t *, size_t)
SYSCALL(bool, FsQueryHandle, 16, handle_t, uint32_t, void *, size_t)
SYS_END_GROUP(4)

/* 5 */
SYS_BEGIN_GROUP(5)
SYSCALL(void *, VmmAlloc, 12, size_t, addr_t, uint32_t)
SYSCALL(bool, VmmFree, 4, void*)
SYSCALL(void *, VmmMapShared, 12, const wchar_t *, addr_t, uint32_t)
SYSCALL(void *, VmmMapFile, 16, handle_t, addr_t, size_t, uint32_t)
SYS_END_GROUP(5)

/* 6 */
SYS_BEGIN_GROUP(6)
SYSCALL(handle_t, EvtAlloc, 0, void)
SYSCALL(bool, HndClose, 4, handle_t)
SYSCALL(void, EvtSignal, 4, handle_t)
SYSCALL(bool, EvtIsSignalled, 4, handle_t)
SYS_END_GROUP(6)

/* 7 */
SYS_BEGIN_GROUP(7)
SYSCALL(handle_t, WndCreate, 12, handle_t, const struct wndattr_t *, unsigned)
SYSCALL(bool, WndClose, 4, handle_t)
SYSCALL(bool, WndPostMessage, 8, handle_t, const struct msg_t *)
SYSCALL(bool, WndInvalidate, 8, handle_t, const struct MGLrect *)
SYSCALL(bool, WndGetMessage, 4, struct msg_t*)
SYSCALL(bool, WndGetAttribute, 20, handle_t, uint32_t, uint32_t, void *, size_t*)
SYSCALL(bool, WndSetAttribute, 20, handle_t, uint32_t, uint32_t, const void *, size_t)
SYSCALL(handle_t, WndOwnRoot, 0, void)
SYSCALL(bool, WndQueueInput, 4, const struct wndinput_t*)
SYSCALL(bool, WndSetFocus, 4, handle_t)
SYSCALL(bool, WndHasFocus, 4, handle_t)
SYSCALL(bool, WndSetCapture, 8, handle_t, bool)
SYSCALL(bool, WndGetClip, 12, handle_t, struct MGLrect *, size_t*)
SYS_END_GROUP(7)

/*! @} */

#ifdef KERNEL
#undef ThrWaitHandle
#undef ThrSleep
#undef ThrCreateThread
#undef EvtAlloc
#undef EvtSignal
#undef EvtIsSignalled
#undef HndClose
#undef VmmFree
#endif
