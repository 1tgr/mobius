/* $Id: fs.c,v 1.2 2002/05/05 13:54:36 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/rtl.h>
#include <os/defs.h>
#include <errno.h>

bool FsReadSync(handle_t file, void *buf, size_t bytes, size_t *bytes_read)
{
    fileop_t op;

    op.event = file;
    if (!FsRead(file, buf, bytes, &op))
    {
	errno = op.result;
	return false;
    }

    while (op.result == SIOPENDING)
	ThrWaitHandle(op.event);

    if (op.result != 0)
    {
	errno = op.result;
	return false;
    }

    if (bytes_read != 0)
	*bytes_read = op.bytes;

    return true;
}

bool FsWriteSync(handle_t file, const void *buf, size_t bytes, size_t *bytes_written)
{
    fileop_t op;

    op.event = file;
    if (!FsWrite(file, buf, bytes, &op))
    {
	errno = op.result;
	return false;
    }

    while (op.result == SIOPENDING)
	ThrWaitHandle(op.event);

    if (op.result != 0)
    {
	errno = op.result;
	return false;
    }

    if (bytes_written != 0)
	*bytes_written = op.bytes;

    return true;
}
