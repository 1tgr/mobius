#include <wchar.h>
#include <os/syscall.h>
#include <os/defs.h>

process_info_t *ProcGetProcessInfo(void);

int _cputws(const wchar_t *str, size_t count)
{
	/*return DbgWrite(str, count);*/
	return FsWrite(ProcGetProcessInfo()->std_out,
		str,
		count * sizeof(wchar_t)) / sizeof(wchar_t);
}