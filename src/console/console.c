/* $Id: console.c,v 1.4 2002/08/17 22:52:06 pavlovskii Exp $ */

#include <stdlib.h>
#include <wchar.h>
#include <errno.h>
#include <stdio.h>

#include <os/port.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <os/ioctl.h>

handle_t hnd;

/*bool FsReadSync(handle_t file, void *buf, size_t bytes, size_t *bytes_read)
{
    fileop_t op;

    op.event = hnd;
    if (!FsRead(file, buf, bytes, &op))
    {
        errno = op.result;
        return false;
    }

    if (op.result == SIOPENDING)
        ThrWaitHandle(op.event);

    if (op.result != 0)
    {
        errno = op.result;
        return false;
    }

    if (bytes_read != NULL)
        *bytes_read = op.bytes;

    return true;
}*/

void ConClientThread(void)
{
    char buf[16];
    size_t size;
    handle_t client;
    fileop_t op;

    client = (handle_t) ThrGetThreadInfo()->param;
    if (client != NULL)
    {
        fprintf(stderr, "console: got client\n");
        while (FsIoCtl(client, IOCTL_BYTES_AVAILABLE, &size, sizeof(size), &op))
            if (size > 0)
            {
                fprintf(stderr, "console: got %u bytes\n", size);
                if (size > sizeof(buf) - 1)
                    size = sizeof(buf) - 1;

                if (!FsReadSync(client, buf, size, &size))
                    break;

                buf[size] = '\0';
                fprintf(stderr, "console: got string: %u bytes: %s", size, buf);
            }

        perror("console");
        FsClose(client);
    }
    else
        fprintf(stderr, "console: NULL client\n");

    ThrExitThread(0);
}

int main(void)
{
    handle_t server;

    fprintf(stderr, "Hello from the console!\n");
    server = FsCreate(SYS_PORTS L"/console", 0);
    hnd = EvtAlloc();

    while (PortListen(server))
    {
        if (!ThrWaitHandle(server))
            break;

        ThrCreateThread(ConClientThread, 
            (void*) PortAccept(server, FILE_READ | FILE_WRITE), 16);
    }

    fprintf(stderr, "console: finished\n");
    FsClose(server);
    HndClose(hnd);
    return EXIT_SUCCESS;
}
