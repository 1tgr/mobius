#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <os/filesys.h>

typedef struct _config_t config_t;
struct _config_t
{
	config_t *prev, *next;
	wchar_t name[40];
	enum { dispText, dispGui } display;
	dword timeout;
};

extern config_t *cfg_current, *cfg_first, *cfg_last;

void ProcessKernelRc();
bool ProcessKernelRcSection(IStream* strm, config_t* cfg);
void ProcessKernelRcLine(const wchar_t* line, const wchar_t* setting, config_t* cfg);

#ifdef __cplusplus
}
#endif

#endif