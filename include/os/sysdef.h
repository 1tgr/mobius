/* $Id: sysdef.h,v 1.2 2003/06/22 15:50:13 pavlovskii Exp $ */
#ifndef SYS_BEGIN_GROUP
#define SYS_BEGIN_GROUP(n)
#endif

#ifndef SYS_END_GROUP
#define SYS_END_GROUP(n)
#endif

#ifndef __DOXYGEN__
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

#define SYS_ThrKillThread       0x200
#define SYS_ThrWaitHandle       0x201
#define SYS_ThrSleep            0x202
#define SYS_ThrCreateV86Thread  0x203
#define SYS_ThrGetV86Context    0x204
#define SYS_ThrSetV86Context    0x205
#define SYS_ThrContinueV86      0x206
#define SYS_ThrCreateThread     0x207
#define SYS_ThrWaitMultiple     0x208
#define SYS_ThrContinue			0x209

#define SYS_ProcKillProcess     0x300
#define SYS_ProcSpawnProcess    0x301
#define SYS_ProcGetExitCode     0x302

#define SYS_FsCreate            0x400
#define SYS_FsOpen              0x401
#define SYS_FsReadAsync         0x402
#define SYS_FsWriteAsync        0x403
#define SYS_FsOpenDir           0x404
#define SYS_FsQueryFile         0x405
#define SYS_FsRequest           0x406
#define SYS_FsIoCtl             0x407
#define SYS_FsReadDir           0x408
#define SYS_FsQueryHandle       0x409
#define SYS_FsCreateDir         0x40a
#define SYS_FsChangeDir         0x40b
#define SYS_FsMount             0x40c
#define SYS_FsDismount          0x40d
#define SYS_FsCreatePipe        0x40e

#define SYS_VmmAlloc            0x500
#define SYS_VmmFree             0x501
#define SYS_VmmMapSharedArea    0x502
#define SYS_VmmMapFile          0x503
#define SYS_VmmReserveArea      0x504
#define SYS_VmmShare            0x505

#define SYS_EvtCreate           0x600
#define SYS_HndClose            0x601
#define SYS_EvtSignal           0x602
#define SYS_EvtIsSignalled      0x603
#define SYS_HndSetInheritable   0x604
//#define SYS_MuxCreate           0x605
//#define SYS_MuxAcquire          0x606
//#define SYS_MuxRelease          0x607
#define SYS_HndExport           0x605
#define SYS_HndOpen             0x606
#define SYS_SemCreate           0x607
#define SYS_SemUp               0x608
#define SYS_SemDown             0x609

#define SYS_CounterOpen         0x700
#define SYS_CounterSample       0x701

#define SYS_DevInstallIsaDevice	0x800
#endif

/* 0 */
SYS_BEGIN_GROUP(0)
SYS_END_GROUP(0)

/* 1 */
/*!
 *  \addtogroup ke System information
 *  @{
 */
SYS_BEGIN_GROUP(1)

/*!
 *  \brief  Writes a string to the debug log
 *
 *  \param  text    String to write. Does not need to be nul-terminated.
 *  \param  length  Length of the \b text parameter, in characters
 *
 *  \return The number of characters written
 */
SYSCALL(int, DbgWrite, 8, (const wchar_t *text, size_t length))

/*!
 *  \brief  Dummy system call for testing
 * 
 *  \param  a   A number
 *  \param  b   Another number
 *
 *  \return The number 42
 */
SYSCALL(int, Hello, 8, (int a, int b))

/*!
 *  \brief  Returns the number of milliseconds elapsed since the OS was booted
 *
 *  The resolution of the uptime counter is the same as the scheduler quantum.
 *  You can obtain the scheduler quantum using \b SysGetTimes().
 *
 *  \return The system uptime, in milliseconds
 */
SYSCALL(unsigned, SysUpTime, 0, (void))

/*!
 *  \brief  Obtains various system information
 *
 *  \param  info    Pointer to a \b sysinfo_t structure
 *
 *  \return \b true if successful, \b false otherwise
 */
SYSCALL(bool, SysGetInfo, 4, (struct sysinfo_t *info))

/*!
 *  \brief  Obtains system timing information
 *
 *  \param  times   Pointer to a \b systimes_t structure
 *
 *  \return \b true if successful, \b false otherwise
 */
SYSCALL(bool, SysGetTimes, 4, (struct systimes_t *times))

/*!
 *  \brief  Shuts down the system
 *
 *  Exits all processes (except in the case of \b SHUTDOWN_FLUSH) and flushes 
 *  unsaved data to disk, then carries out the action specified by the 
 *  \b action parameter.
 *
 *  \param  action  Action to take after shutting down. One of:
 *      - \b SHUTDOWN_HALT: does not take any action after shutting down, but 
 *          halts the system for a manual reboot
 *      - \b SHUTDOWN_REBOOT: reboots the system
 *      - \b SHUTDOWN_POWEROFF: turns the system power off
 *      - \b SHUTDOWN_FLUSH: returns to the application after flushing data to
 *          disk
 *
 *  \return \b true if, for \b SHUTDOWN_FLUSH, the flush succeeded. Otherwise,
 *      \b false if the shutdown failed.
 */
SYSCALL(bool, SysShutdown, 4, (unsigned action))

/*!
 *  \brief  Initiates kernel memory leak tracking
 */
SYSCALL(void, KeLeakBegin, 0, (void))

/*!
 *  \brief  Ends kernel memory leak tracking and prints the results
 */
SYSCALL(void, KeLeakEnd, 0, (void))

/*!
 *  \brief  Allows another thread to run
 *
 *  If there are any other lower-or-equal priority threads ready to run, they 
 *  will be run, and the calling thread will be rescheduled afterwards. 
 *  If there are no other threads ready to run, this function will return 
 *  immediately.
 *
 *  \note   Ready-to-run threads of higher priority will preempt the calling
 *      thread anyway, regardless of whether \b SysYield() is called.
 */
SYSCALL(void, SysYield, 0, (void))

/*!
 *  \brief  Enables or disables single-step mode for the current thread
 *
 *  \param  enable  Set to \b true to enable single-step mode, or \b false
 *      to disable it
 */
SYSCALL(void, KeSetSingleStep, 4, (bool enable))
SYS_END_GROUP(1)
/*!
 *  @}
 */

/* 2 */
/*!
 *  \addtogroup thr Threads
 *  @{
 */
SYS_BEGIN_GROUP(2)

/*!
 *  \brief  Exits the current thread
 *
 *  Any outstanding asynchronous I/O requests are cancelled.
 *
 *  \param  exitcode    Exit code of the thread
 */
SYSCALL(void, ThrKillThread, 4, (int exitcode))

/*!
 *  \brief  Waits for the specified handle to become signalled
 *
 *  If the handle is already signalled, this function returns immediately
 *
 *  \param  hnd Handle to wait on
 *
 *  \return \b true if the wait succeeded, \b false otherwise.
 */
SYSCALL(bool, ThrWaitHandle, 4, (handle_t hnd))

/*!
 *  \brief  Pauses for the specified number of milliseconds
 *
 *  The resolution of the delay is the same as the scheduler quantum.
 *  You can obtain the scheduler quantum using \b SysGetTimes().
 *
 *  \param  delay   Number of milliseconds to pause
 */
SYSCALL(void, ThrSleep, 4, (unsigned delay))

/*!
 *  \brief  Creates a thread to run V86-mode code
 *
 *  V86 mode is used to emulate real mode inside a protected-mode OS. 
 *  M&ouml;bius runs V86-mode code inside special threads. Associated with
 *  a V86-mode thread is a 32-bit handler routine, which is called to handle
 *  exceptions generated by the 16-bit code. Generally these occur due to 
 *  interrupt calls or I/O port accesses, which should be emulated using the
 *  normal M&ouml;bius system calls.
 *
 *  The \b entry and \b stack_top parameters are 16-bit far pointers. They 
 *  consist of a segment address in the high order word and an offset in the 
 *  low word.
 *
 *  \param  entry       Far pointer to entry point of thread
 *  \param  stack_top   Far pointer to the top of the thread's 16-bit stack
 *  \param  priority    Initial priority to assign to the thread
 *  \param  handler     Pointer to a 32-bit routine which is called to handle
 *      exceptions in V86 mode
 *  \param  name        Name to assign to the thread for debugging
 *
 *  \return If successful, a handle to the new thread; if unsuccessful, zero.
 *      You should close the returned handle (using \b HndClose()) when you
 *      have finished with it. Closing the handle will not terminate the 
 *      thread.
 */
SYSCALL(handle_t, ThrCreateV86Thread, 16, (uint32_t entry, uint32_t stack_top, unsigned priority, void (*handler)(void), const wchar_t *name))

/*!
 *  \brief  Obtains the values of the current 16-bit thread's registers
 *
 *  Must be called from inside a V86-mode thread's handler routine
 *
 *  \param  context Pointer to a \b context_v86_t structure, which
 *      receives the thread's registers
 *
 *  \return \b true if successful, \b false otherwise
 */
SYSCALL(bool, ThrGetV86Context, 4, (struct context_v86_t* context))

/*!
 *  \brief  Updates the values of the current 16-bit thread's registers
 *
 *  Must be called from inside a V86-mode thread's handler routine
 *
 *  \param  context Pointer to a \b context_v86_t structure, which
 *      specifies the thread's registers
 *
 *  \return \b true if successful, \b false otherwise
 */
SYSCALL(bool, ThrSetV86Context, 4, (const struct context_v86_t *context))

/*!
 *  \brief  Continues a 16-bit thread's execution
 *
 *  This function must be called from inside a V86-mode thread's handler
 *  routine. It will not return, but 16-bit execution will continue until
 *  the next time the handler is called. Do not use \b return to return
 *  from a V86 handler.
 *
 *  \return \b false if unsuccessful
 */
SYSCALL(bool, ThrContinueV86, 0, (void))

/*!
 *  \brief  Creates a new thread
 *
 *  \param  entry       Entry point function for the new thread
 *  \param  param       Parameter to pass to the thread. To obtain this 
 *      parameter from within the new thread, use \b ThrGetThreadInfo()->param.
 *  \param  priority    Initial priority to assign to the new thread
 *  \param  name        Name to assign to the thread for debugging
 *
 *  \return If successful, a handle to the new thread; if unsuccessful, zero.
 *      You should close the returned handle (using \b HndClose()) when you
 *      have finished with it. Closing the handle will not terminate the 
 *      thread.
 */
SYSCALL(handle_t, ThrCreateThread, 16, (int (*entry)(void*), void *param, unsigned priority, const wchar_t *name))

SYSCALL(int, ThrWaitMultiple, 12, (const handle_t *handles, unsigned count, bool wait_all))
SYSCALL(bool, ThrContinue, 4, (const struct context_t *ctx))
SYS_END_GROUP(2)
/*!
 *  @}
 */

/* 3 */
/*!
 *  \addtogroup proc    Processes
 *  @{
 */
SYS_BEGIN_GROUP(3)
/*!
 *  \brief  Exits the current process
 *
 *  All threads in the process are exited (see \b ThrExitThread()), all 
 *  handles opened by the process are closed, and all memory allocated by
 *  the process is freed.
 *
 *  Shared handles (e.g. those opened using \b HndOpen()) and shared memory 
 *  areas (e.g. those mapped using \b VmmMapSharedArea()) are destroyed when
 *  the last reference to them is released. This can happen either when the
 *  last process using them exits, when the last \b HndClose() is called, or
 *  when the last \b VmmFree() is called.
 *
 *  \param  exitcode    Exit code of the process. This code can be retrieved 
 *      elsewhere using \b ProcGetExitCode().
 */
SYSCALL(void, ProcKillProcess, 4, (int exitcode))

/*!
 *  \brief  Creates a new process
 *
 *  \param  exe     Name of the executable file that the process is to run
 *  \param  info    Pointer to some initial info to assign to the new process 
 *      (see \b process_info_t). Pass \b NULL to inherit the calling process's
 *      info.
 *
 *  \note   If a non-trivial error occurs (for example, if the executable or an 
 *      associated DLL could not be opened), the error is not reported to the 
 *      parent process, although the new process will exit immediately.
 *
 *  \return Handle to the new process, or zero if an error occurred while 
 *      creating the process object. You should close the returned handle 
 *      (using \b HndClose()) when you have finished with it. Closing the 
 *      handle will not terminate the process.
 */
SYSCALL(handle_t, ProcSpawnProcess, 8, (const wchar_t *exe, const struct process_info_t *info))

/*!
 *  \brief  Returns the exit code of another process
 *
 *  \param  proc    Handle to the process
 *
 *  \return The process exit code, or zero if the process is still running
 */
SYSCALL(int, ProcGetExitCode, 4, (handle_t proc))
SYS_END_GROUP(3)
/*!
 *  @}
 */

/* 4 */
/*!
 *  \addtogroup fs  Files
 *
 *  These functions interact with the file system and the files within.
 *
 *  Files accessible through \b FsCreate() and \b FsOpen() fall into one of 
 *  three broad categories:
 *      - On-disk files
 *      - Devices, located under \b /System/Devices
 *      - Ports, located under \b /System/Ports
 *
 *  \par Special Files
 *  The directory \b /System contains the \b Boot, \b Devices and \b Ports 
 *  subdirectories. The files in \b Boot are the modules loaded before the OS
 *  was booted, and are read-only.
 *
 *  \par
 *  \b Devices contains the directory structure maintained by the device 
 *  manager. Immediately within the \b Devices directory are non-PnP devices,
 *  such as the keyboard, floppy drive, and mouse, as well as the roots of PnP
 *  buses, such as PCI and ISAPnP. Also within the \b Devices directory is the
 *  \b Classes subdirectory, which contains each device in the system indexed
 *  by its class. For instance, you could list all disk volumes installed by
 *  listing \b /System/Devices/Classes/volume*.
 *
 *  \par
 *  The files in \b Ports are IPC (inter-process communication) ports. To 
 *  write a port server application, create a port within \b /System/Ports
 *  using \b FsCreate(). Call \b PortAccept() in a loop; it will return
 *  a pipe handle each time a client connects using \b FsOpen(). You can use 
 *  this pipe to communicate with the client.
 *
 *  \par Input and Output
 *  Most I/O in M&ouml;bius is asynchronous internally. This means that 
 *  \b FsRead() and \b FsWrite() can return immediately with a result code of
 *  \b SIOPENDING. In this case, the calling thread will continue to run, and
 *  the OS will signal the event specified in the \b fileop_t structure when
 *  the operation has completed. If you need to block until completion, use
 *  \b ThrWaitHandle() to wait on the event. If only one thread has I/O queued
 *  on a file handle at any one time, the file handle can be used in the 
 *  \b fileop_t structure instead of an event handle. The system also 
 *  supplies synchronous wrappers, \b FsReadSync() and \b FsWriteSync(), 
 *  which will not return until the I/O operation has completed.
 *  @{
 */
SYS_BEGIN_GROUP(4)

/*!
 *  \brief  Creates a new file
 *
 *  \param  name    Name of the new file
 *  \param  flags   Options to apply to this file handle. One or more of:
 *      - \b FILE_READ: allows reading from the file
 *      - \b FILE_WRITE: allows writing to the file
 *      - \b FILE_FORCE_CREATE: forces creation of the file, even if it 
 *          already exists
 *
 *  \return A handle to the file, or zero if unsuccessful
 */
SYSCALL(handle_t, FsCreate, 8, (const wchar_t *name, uint32_t flags))

/*!
 *  \brief  Opens an existing file
 *
 *  \param  name    Name of the file
 *  \param  flags   Options to apply to this file handle. One or more of:
 *      - \b FILE_READ: allows reading from the file
 *      - \b FILE_WRITE: allows writing to the file
 *      - \b FILE_FORCE_OPEN: creates the file if it does not already exist
 *
 *  \return A handle to the file, or zero if unsuccessful
 */
SYSCALL(handle_t, FsOpen, 8, (const wchar_t *name, uint32_t flags))

/*!
 *  \brief  Reads from a file asynchronously
 *
 *  \param  file    Handle to the file to read from
 *  \param  buffer  Memory buffer to read into
 *  \param  bytes   Number of bytes to read
 *  \param  op      Pointer to a \b fileop_t which is filled in with the 
 *      results on completion of the read
 *
 *  \return \b true if successful, \b false otherwise
 */
SYSCALL(status_t, FsReadAsync, 24, (handle_t file, void *buffer, uint64_t offset, size_t bytes, struct fileop_t *op))

/*!
 *  \brief  Writes to a file asynchronously
 *
 *  \param  file    Handle to the file to write to
 *  \param  buffer  Memory buffer to write from
 *  \param  bytes   Number of bytes to write
 *  \param  op      Pointer to a \b fileop_t which is filled in with the 
 *      results on completion of the write
 *
 *  \return \b true if successful, \b false otherwise
 */
SYSCALL(status_t, FsWriteAsync, 24, (handle_t file, const void *buffer, uint64_t offset, size_t bytes, struct fileop_t *op))

/*!
 *  \brief  Opens a directory, which can be used to obtain directory listings
 *
 *  \param  Name of the directory
 *
 *  \return A handle to the directory, or zero if unsuccessful
 */
SYSCALL(handle_t, FsOpenDir, 4, (const wchar_t *name))

/*!
 *  \brief  Obtains information on a file
 *
 *  \param  name    Name of the file
 *  \param  code    Class of information to query. One of the following, or a 
 *      file system-specific code:
 *      - \b FILE_QUERY_NONE: tests for the file's presence; does not return 
 *          any information
 *      - \b FILE_QUERY_DIRENT: returns a \b dirent_t structure
 *      - \b FILE_QUERY_STANDARD: returns a \b dirent_standard_t structure
 *      - \b FILE_QUERY_DEVICE: for devices, returns a \b dirent_device_t 
 *          structure
 *  \param  buffer  Pointer to a structure where file information is placed. 
 *      The layout of this structure depends on the \b code parameter.
 *  \param  size    Size of the structure at \b buffer
 *
 *  \return \b true if successful, \b false otherwise
 */
SYSCALL(bool, FsQueryFile, 16, (const wchar_t *name, uint32_t code, void *buffer, size_t size))

/*!
 *  \brief  Executes a file system-specific request of a file
 *
 *  This function is most useful when dealing with devices under the 
 *  \b /System/Devices directory, which respond to various device-specific
 *  \b FsRequest/\b FsRequestSync codes.
 *
 *  \param  file    Handle of the file to make the request of
 *  \param  code    Device-specific request code
 *  \param  buffer  Input and/or output parameter buffer
 *  \param  bytes   Number of input bytes
 *  \param  op      Pointer to a \b fileop_t which is filled in with the 
 *      results on completion of the request
 *
 *  \return \b true if successful, \b false otherwise
 */
SYSCALL(bool, FsRequest, 20, (handle_t file, uint32_t code, void *buffer, size_t bytes, struct fileop_t* op))

/*!
 *  \brief  Issues an ioctl request on a device
 *
 *  \deprecated See \b FsRequest
 */
SYSCALL(bool, FsIoCtl, 20, (handle_t file, uint32_t code, void *buffer, size_t bytes, struct fileop_t* op))

SYSCALL(bool, FsReadDir, 12, (handle_t dir, struct dirent_t *buffer, size_t bytes))
SYSCALL(bool, FsQueryHandle, 16, (handle_t file, uint32_t code, void *buffer, size_t bytes))
SYSCALL(bool, FsCreateDir, 4, (const wchar_t* dir))
SYSCALL(bool, FsChangeDir, 4, (const wchar_t* dir))
SYSCALL(bool, FsMount, 12, (const wchar_t *path, const wchar_t *filesys, const wchar_t *dest))
SYSCALL(bool, FsDismount, 4, (const wchar_t *path))
SYSCALL(bool, FsCreatePipe, 4, (handle_t *ends))
SYS_END_GROUP(4)
/*!
 *  @}
 */

/* 5 */
/*!
 *  \addtogroup vmm Virtual memory manager
 *  @{
 */
SYS_BEGIN_GROUP(5)
SYSCALL(void *, VmmAlloc, 12, (size_t, addr_t, uint32_t))
SYSCALL(bool, VmmFree, 4, (void*))
SYSCALL(void *, VmmMapSharedArea, 12, (handle_t, addr_t, uint32_t))
SYSCALL(void *, VmmMapFile, 16, (handle_t, addr_t, size_t, uint32_t))
SYSCALL(void *, VmmReserveArea, 8, (size_t, addr_t))
SYSCALL(handle_t, VmmShare, 8, (void *base, const wchar_t *name))
SYS_END_GROUP(5)
/*!
 *  @}
 */

/* 6 */
/*!
 *  \addtogroup hnd Handles
 *  @{
 */
SYS_BEGIN_GROUP(6)
SYSCALL(handle_t, EvtCreate, 4, (bool is_signalled))
SYSCALL(bool, HndClose, 4, (handle_t handle))
SYSCALL(bool, EvtSignal, 8, (handle_t event, bool is_signalled))
SYSCALL(bool, EvtIsSignalled, 4, (handle_t event))
SYSCALL(bool, HndSetInheritable, 8, (handle_t handle, bool inheritable))
//SYSCALL(handle_t, MuxCreate, 0, (void))
//SYSCALL(bool, MuxAcquire, 4, (handle_t))
//SYSCALL(bool, MuxRelease, 4, (handle_t))
SYSCALL(bool, HndExport, 8, (handle_t, const wchar_t*))
SYSCALL(handle_t, HndOpen, 4, (const wchar_t*))
SYSCALL(handle_t, SemCreate, 4, (int count))
SYSCALL(bool, SemUp, 4, (handle_t))
SYSCALL(bool, SemDown, 4, (handle_t))
SYS_END_GROUP(6)
/*!
 *  @}
 */


/* 7 */
/*!
 *	\addtogroup	counter	Counters
 *	@{
 */
SYS_BEGIN_GROUP(7)
SYSCALL(handle_t, CounterOpen, 4, (const wchar_t *name))
SYSCALL(int, CounterSample, 4, (handle_t counter))
SYS_END_GROUP(7)
/*!
 *  @}
 */


/* 8 */
/*!
 *	\addtogroup	dev	Device Manager
 *	@{
 */
SYS_BEGIN_GROUP(8)
SYSCALL(bool, DevInstallIsaDevice, 8, (const wchar_t *driver, const wchar_t *name))
SYS_END_GROUP(8)
/*!
 *  @}
 */
