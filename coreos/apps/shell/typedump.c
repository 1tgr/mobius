/*
 * $History: typedump.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 13/03/04   Time: 22:28
 * Updated in $/coreos/apps/shell
 * Read 16 bytes at a time
 * Added a history block
 */
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "common.h"

static void ShDumpFile(const wchar_t *name, void (*fn)(const void*, addr_t, size_t))
{
    static char buf[16 + 1];

    handle_t file;
    size_t len;
    addr_t origin;
    dirent_standard_t di;
    char *ptr, *dyn;

    di.length = 0;
    if (!FsQueryFile(name, FILE_QUERY_STANDARD, &di, sizeof(di)) ||
        di.length == 0)
    {
        di.length = sizeof(buf) - 1;
        ptr = buf;
        dyn = NULL;
    }
    else
    {
        dyn = malloc(di.length);
        ptr = dyn;
    }

    printf("ShDumpFile(%S): reading in chunks of %lu\n", name, (uint32_t) di.length);
    file = FsOpen(name, FILE_READ);
    if (file == NULL)
    {
        _pwerror(name);
        return;
    }

    origin = 0;
    do
    {
        //KeLeakBegin();

        //printf("[b]");
		if (!FsRead(file, ptr, origin, di.length, &len))
		{
			_pwerror(name);
			break;
		}

		//KeLeakEnd();

        if (len == 0)
        {
            printf("FSD read zero bytes but didn't report an error\n");
            break;
        }

        if (len < di.length)
            ptr[len] = '\0';
        /*printf("%u", len);*/
        fn(ptr, origin, len);

        origin += len;
        if (len < di.length)
        {
            printf("FSD hit the end of the file: successful but only %u bytes read\n", len);
            break;
        }

        //printf("[e]");
        fflush(stdout);
    } while (true);

    free(dyn);
    HndClose(file);
}

static void ShTypeOutput(const void *buf, addr_t origin, size_t len)
{
    fwrite(buf, len, 1, stdout);
    fflush(stdout);
}

void ShCmdType(const wchar_t *command, wchar_t *params)
{
    params = ShPrompt(L" File? ", params);
    if (*params == '\0')
        return;

    ShDumpFile(params, ShTypeOutput);
}

static void ShDumpOutput(const void *buf, addr_t origin, size_t size)
{
    const uint8_t *ptr;
    int i, j, columns;

	columns = (sh_width - 9) / 3;
	for (i = 1; i < columns; i *= 2)
		;

	columns = i / 2;

    ptr = (const uint8_t*) buf;
    for (j = 0; j < size; j += i, ptr += i)
    {
        printf("%8lx ", origin + j);
        for (i = 0; i < columns; i++)
        {
            printf("%02x ", ptr[i]);
            if (i + j >= size)
                break;
        }

        printf("\n");
    }
}

void ShCmdDump(const wchar_t *command, wchar_t *params)
{
    params = ShPrompt(L" File? ", params);
    if (*params == '\0')
        return;

    ShDumpFile(params, ShDumpOutput);
}

static void ShThrashOutput(const void *buf, addr_t origin, size_t size)
{
    printf("%lu\r", origin);
}

void ShCmdThrash(const wchar_t *command, wchar_t *params)
{
    params = ShPrompt(L" File? ", params);
    if (*params == '\0')
        return;

    ShDumpFile(params, ShThrashOutput);
}
