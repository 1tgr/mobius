/* $Id: console.c,v 1.5 2002/09/01 16:21:22 pavlovskii Exp $ */

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <stdio.h>

#include <os/port.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <os/ioctl.h>
#include <os/keyboard.h>

typedef struct console_t console_t;
struct console_t
{
    handle_t client;
} consoles[10];

unsigned num_consoles = 1;
console_t *current;

int *_geterrno(void)
{
    return &ThrGetThreadInfo()->status;
}

void ConKeyboardThread(void)
{
    uint32_t ch, code;
    handle_t keyboard;

    keyboard = FsOpen(SYS_DEVICES L"/keyboard", FILE_READ);
    while (FsReadSync(keyboard, &ch, sizeof(ch), NULL))
    {
        code = ch & ~KBD_BUCKY_ANY;
        if (code >= KEY_F1 && code <= KEY_F12)
            current = consoles + code - KEY_F1;
        else if (ch == (KEY_DEL | KBD_BUCKY_CTRL | KBD_BUCKY_ALT))
            SysShutdown(SHUTDOWN_REBOOT);
        else if (current != NULL && current->client != NULL)
            FsWriteSync(current->client, &ch, sizeof(ch), NULL);
    }
}

void ConClientThread(void)
{
    wchar_t dev[256];
    char buf[160];
    console_t *console;
    handle_t out;
    size_t bytes;

    console = ThrGetThreadInfo()->param;
    swprintf(dev, SYS_DEVICES L"/tty%u", console - consoles);
    out = FsOpen(dev, FILE_WRITE);
    if (current == NULL)
        current = console;

    while (FsReadSync(console->client, buf, sizeof(buf), &bytes))
        FsWriteSync(out, buf, bytes, NULL);

    HndClose(console->client);
    HndClose(out);
    if (current == console)
        current = NULL;

    ThrExitThread(0);
}

void mainCRTStartup(void)
{
    handle_t server, client;
    const wchar_t *server_name = SYS_PORTS L"/console",
        *shell_name = SYS_BOOT L"/shell.exe";
    process_info_t defaults = { 0 };
    unsigned i;

    server = FsCreate(server_name, 0);

    ThrCreateThread(ConKeyboardThread, NULL, 16);

    for (i = 0; i < 3; i++)
    {
        client = FsOpen(server_name, FILE_READ | FILE_WRITE);
        defaults.std_in = defaults.std_out = client;
        wcscpy(defaults.cwd, L"/");
        ProcSpawnProcess(shell_name, &defaults);
    }

    while ((client = PortAccept(server, FILE_READ | FILE_WRITE)))
    {
        if (num_consoles < _countof(consoles))
        {
            consoles[num_consoles].client = client;
            ThrCreateThread(ConClientThread, consoles + num_consoles, 15);
            num_consoles++;
        }
        else
            HndClose(client);
    }

    HndClose(server);
    ProcExitProcess(EXIT_SUCCESS);
}
