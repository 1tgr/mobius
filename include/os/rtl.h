/* $Id: rtl.h,v 1.1.1.1 2002/12/31 01:26:22 pavlovskii Exp $ */
#ifndef __OS_RTL_H
#define __OS_RTL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <os/defs.h>
#include <os/pe.h>

/*!
 *  \addtogroup proc    Processes
 *  @{
 */

/*!
 *  \brief  Returns the current working directory of the process
 *
 *  Retrieves a pointer to the \b cwd field of the \b process_info_t structure.
 *
 *  \return Pointer to the path of the process's current working directory
 */
wchar_t *   ProcGetCwd(void);

/*!
 *  \brief  Returns the process's info structure
 *
 *  \return Pointer to the process's info structure
 */
process_info_t *ProcGetProcessInfo(void);

/*!
 *  \brief  Returns the base address of the process's main executable file
 *
 *  The base address indicates the start of a module's headers in memory.
 *
 *  \return Base address of the process's main executable
 */
addr_t	    ProcGetExeBase(void);

__attribute__((noreturn))
void		ProcExitProcess(int code);
/*!
 *  @}
 */

/*!
 *  \addtogroup thr Threads
 *  @{
 */

/*!
 *  \brief  Returns the current thread's info structure
 *
 *  \return Pointer to the current thread's info structure
 */
thread_info_t *ThrGetThreadInfo(void);

__attribute__((noreturn))
void		ThrExitThread(int code);
/*!
 *  @}
 */

/*!
 *  \addtogroup res Resources
 *  @{
 */
const void *ResFindResource(addr_t base, uint16_t type, uint16_t id, uint16_t language);
bool	    ResLoadString(addr_t base, uint16_t id, wchar_t* str, size_t str_max);
size_t      ResSizeOfResource(addr_t base, uint16_t type, uint16_t id, uint16_t language);
size_t      ResGetStringLength(addr_t base, uint16_t id);
/*!
 *  @}
 */

/*!
 *  \addtogroup   fs  Files
 *  @{
 */

/*!
 *  \brief  Converts a relative path into a fully-qualified path
 *
 *  This function performs minimal valiation of the input path's syntax, but
 *  does not determine whether the path exists.
 *
 *  \param  src Pointer to a relative or full path
 *  \param  dst Pointer to a buffer which will hold the full path. This buffer
 *      must be at least \b MAX_PATH characters in length.
 *
 *  \return \b true if the syntax of the input path was valid, or \b false 
 *      otherwise
 */
bool	    FsFullPath(const wchar_t* src, wchar_t* dst);

/*!
 *  \brief  Reads from a file
 *
 *  Blocks execution of the calling thread until the read completes. Error 
 *  information can be obtained from \b errno.
 *
 *  \param  file        Handle to the file to read from
 *  \param  buf         Memory buffer to read into
 *  \param  bytes       Number of bytes to read
 *  \param  bytes_read  Pointer to a variable where the number of bytes 
 *      actually read is stored, or \b NULL if the number of bytes read is not
 *      important. The number of bytes read may be fewer than \b bytes, even 
 *      if the read succeeds. For example, if a read operation reaches the end 
 *      of a file, it will succeed with a reduced byte count.
 *
 *  \return \b true if successful, \b false otherwise
 */
bool        FsRead(handle_t file, void *buf, uint64_t offset, size_t bytes, size_t *bytes_read);

/*!
 *  \brief  Writes to a file
 *
 *  Blocks execution of the calling thread until the write completes. Error 
 *  information can be obtained from \b errno.
 *
 *  \param  file            Handle to the file to write to
 *  \param  buf             Memory buffer to write from
 *  \param  bytes           Number of bytes to write
 *  \param  bytes_written   Pointer to a variable where the number of bytes 
 *      actually written is stored, or \b NULL if the number of bytes read is 
 *      not important. The number of bytes read may be fewer than \b bytes, 
 *      even if the write succeeds.
 *
 *  \return \b true if successful, \b false otherwise
 */
bool        FsWrite(handle_t file, const void *buf, uint64_t offset, size_t bytes, size_t *bytes_written);

/*!
 *  \brief  Executes a file system-specific request of a file
 *
 *  This function is most useful when dealing with devices under the 
 *  \b /System/Devices directory, which respond to various device-specific
 *  \b FsRequest/\b FsRequestSync codes. Error information can be obtained 
 *  from \b errno.
 *
 *  \param  file        Handle of the file to make the request of
 *  \param  code        Device-specific request code
 *  \param  buffer      Input and/or output parameter buffer
 *  \param  bytes       Number of input bytes
 *  \param  bytes_out   Pointer to a variable where the number of output
 *      bytes is stored, or \b NULL if the number of output bytes is not 
 *      important.
 *
 *  \return \b true if successful, \b false otherwise
 */
bool        FsRequestSync(handle_t file, uint32_t code, void *buffer, size_t bytes, size_t *bytes_out);

bool		FsGetFileLength(handle_t file, uint64_t *length);
/*!
 *  @}
 */


/*!
 *  \addtogroup dbg Debugging
 *  @{
 */
module_info_t *DbgFindModule(addr_t addr);
IMAGE_PE_HEADERS *DbgGetPeHeaders(addr_t base);
IMAGE_SECTION_HEADER *DbgFindSectionByName(addr_t base, const char *name);
bool        DbgLookupLineNumber(addr_t addr, char **path, char **file, unsigned *line);
/*!
 *  @}
 */

/*!
 *  \addtogroup hnd Handles
 *  @{
 */
void		LmuxInit(lmutex_t *mux);
void		LmuxDelete(lmutex_t *mux);
void		LmuxAcquire(lmutex_t *mux);
void		LmuxRelease(lmutex_t *mux);
/*!
 *  @}
 */

int			SysTestAndSet(int new_value, int *lock_pointer);
int			SysDecrementAndTest(int *refcount);
int			SysIncrement(int *count);

size_t 			wcsto437(char *mbstr, const wchar_t *wcstr, size_t count);

#ifdef __cplusplus
}
#endif

#endif
