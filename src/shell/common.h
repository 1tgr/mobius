/* $Id: common.h,v 1.2 2002/08/17 22:52:13 pavlovskii Exp $ */

#ifndef COMMON_H__
#define COMMON_H__

#include <os/defs.h>

typedef struct shell_line_t shell_line_t;
struct shell_line_t
{
    shell_line_t *prev, *next;
    unsigned num;
    wchar_t *text;
    char *widths;
};

shell_line_t *sh_line_first, *sh_line_last;

typedef struct shell_command_t shell_command_t;
struct shell_command_t
{
    const wchar_t *name;
    void (*func)(const wchar_t*, wchar_t*);
    unsigned help_id;
};

typedef struct shell_action_t shell_action_t;
struct shell_action_t
{
    const wchar_t *name;
    const wchar_t *mimetype;
    const wchar_t *program;
};

extern shell_command_t sh_commands[];
extern shell_action_t sh_actions[];

#define PARAMSEP	    '\\'
#define ___CASSERT(a, b)    a##b
#define __CASSERT(a, b)	    ___CASSERT(a, b)
#define CASSERT(exp)	    extern char __CASSERT(__ERR, __LINE__)[(exp)!=0]

int _cputws(const wchar_t *str, size_t count);
int _cputs(const char *str, size_t count);

#pragma pack(push, 1)
typedef struct psp_t psp_t;
struct psp_t
{
    uint16_t int20;
    uint16_t memory_top;
    uint8_t reserved;
    uint8_t cpm_call[5];
    uint16_t com_bytes_available;
    uint32_t terminate_addr;
    uint32_t cbreak_addr;
    uint32_t error_addr;
    uint16_t parent_psp;
    uint8_t file_handles[20];
    uint16_t environment;
    uint32_t int21_stack;
    uint16_t handles_size;
    uint32_t handles_addr;
    uint32_t prev_psp_addr;
    uint8_t reserved1[20];
    uint8_t dos_dispatch[3];
    uint8_t reserved2[9];
    uint8_t fcb1[36];
    uint8_t fdb2[20];
    uint8_t cmdline_length;
    uint8_t cmdline[127];
};
#pragma pack(pop)

CASSERT(sizeof(psp_t) == 0x100);

uint32_t ShReadKey(void);
shell_line_t *ShReadLine(void);
wchar_t *ShPrompt(const wchar_t *prompt, wchar_t *params);

unsigned ShParseParams(wchar_t *params, wchar_t ***names, wchar_t ***values, 
		       wchar_t ** unnamed);
bool ShHasParam(wchar_t **names, const wchar_t *look);
wchar_t *ShFindParam(wchar_t **names, wchar_t **values, const wchar_t *look);

FARPTR i386LinearToFp(void *ptr);
void ShV86Handler(void);

#endif