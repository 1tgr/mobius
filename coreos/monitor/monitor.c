/*
 * $History: monitor.c $
 * 
 * *****************  Version 3  *****************
 * User: Tim          Date: 9/03/04    Time: 21:47
 * Updated in $/coreos/monitor
 * Reinstated second shell
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 22/02/04   Time: 14:16
 * Updated in $/coreos/monitor
 * Reinstated shell
 * Added history block
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <wchar.h>

#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <os/framebuf.h>

uint16_t *text;
wchar_t boot_fsd[9];
wchar_t boot_device[MAX_PATH];
void *image_progress;
surface_t surf;
handle_t vid;


static void MonitorDoParameter(wchar_t *param_name, wchar_t *value)
{
    wchar_t *comma;

    if (wcscmp(param_name, L"boot") == 0)
    {
        comma = wcschr(value, ',');
        if (comma == NULL)
            wcscpy(boot_fsd, value);
        else
        {
            *comma = '\0';
            wcsncpy(boot_fsd, value, _countof(boot_fsd));
            wcscpy(boot_device, comma + 1);
        }
    }
}


static void MonitorParseCommandLine(wchar_t *command_line)
{
    int state;
    wchar_t *ch, param_name[20], value[256], *ptr;

    state = 0;
    ch = command_line;
    while (*ch != '\0')
    {
        switch (state)
        {
        case 0:     /* in kernel image name */
            if (*ch == ' ')
                state = 1;
            else if (*ch == '\\')
                state = 2;
            else
                ch++;
            break;

        case 1:     /* in white space */
            if (*ch != ' ')
                state = 2;
            else
                ch++;
            break;

        case 2:     /* at \ */
            if (*ch == '\\')
            {
                ch++;
                ptr = param_name;
                state = 3;
            }
            else if (*ch == ' ')
                state = 1;
            else
                ch++;
            break;

        case 3:     /* in parameter name */
            if (*ch == '=')
            {
                ch++;
                *ptr = '\0';
                ptr = value;
                state = 4;
            }
            else if (*ch == ' ')
                state = 1;
            else
                *ptr++ = *ch++;
            break;

        case 4:     /* in value */
            *ptr++ = *ch++;
            if (*ch == ' ' || *ch == '\0' || *ch == '\\')
            {
                if (*ch == '\\')
                    state = 2;
                else
                    state = 1;
                *ptr = '\0';
                MonitorDoParameter(param_name, value);
                state = 1;
            }
            break;
        }
    }
}

#pragma pack(push, 2)
typedef struct tagBITMAPFILEHEADER {
  uint16_t bfType;
  uint32_t bfSize;
  uint16_t bfReserved1;
  uint16_t bfReserved2;
  uint32_t bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER{
  uint32_t biSize;
  int32_t  biWidth;
  int32_t  biHeight;
  uint16_t biPlanes;
  uint16_t biBitCount;
  uint32_t biCompression;
  uint32_t biSizeImage;
  int32_t  biXPelsPerMeter;
  int32_t  biYPelsPerMeter;
  uint32_t biClrUsed;
  uint32_t biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;
#pragma pack(pop)


void *MonitorLoadImage(const wchar_t *filename)
{
    handle_t file;
    void *data;
    dirent_standard_t dirent;

    file = FsOpen(filename, FILE_READ);
    if (file == 0)
    {
        _wdprintf(L"monitor: can't open %s\n", filename);
        goto error0;
    }

    if (!FsQueryHandle(file, FILE_QUERY_STANDARD, &dirent, sizeof(dirent)))
        goto error1;

    data = VmmMapFile(file, 0, dirent.length, VM_MEM_USER | VM_MEM_READ);
    if (data == NULL)
    {
        _wdprintf(L"monitor: can't map %s\n", filename);
        goto error1;
    }

    HndClose(file);
    return data;

error1:
    HndClose(file);
error0:
    return NULL;
}


void MonitorGetPalette(void *data, rgb_t *palette)
{
    BITMAPFILEHEADER *bfh;
    BITMAPINFOHEADER *bih;
    unsigned i;
    uint8_t *src;

    bfh = data;
    bih = (BITMAPINFOHEADER*) (bfh + 1);

    src = (uint8_t*) bih + bih->biSize;
    for (i = 0; i < 16; i++)
    {
        palette[i].red = src[i + 2];
        palette[i].green = src[i + 1];
        palette[i].blue = src[i + 0];
        src += 4;
    }
}


void MonitorGetSize(void *data, unsigned *width, unsigned *height)
{
    BITMAPFILEHEADER *bfh;
    BITMAPINFOHEADER *bih;

    bfh = data;
    bih = (BITMAPINFOHEADER*) (bfh + 1);
    *width = bih->biWidth;
    *height = bih->biHeight;
}


void MonitorDisplayImage(surface_t *surf, void *data, int x, int y, int width, int height)
{
    BITMAPFILEHEADER *bfh;
    BITMAPINFOHEADER *bih;
    char *bits;
    rect_t dest;
    int pitch;

    bfh = data;
    bih = (BITMAPINFOHEADER*) (bfh + 1);
    bits = (char*) bfh + bfh->bfOffBits;

    dest.left = x;
    dest.top = y;
    dest.right = x + width;
    dest.bottom = y + height;

    pitch = ((bih->biBitCount * bih->biWidth) / 8 + 1) & -2;
    surf->vtbl->SurfBltMemoryToScreen(surf,
        &dest,
        bits + pitch * (bih->biHeight - 1),
        -pitch);
    surf->vtbl->SurfFlush(surf);
}


bool MonitorShowSplash(void)
{
    static const wchar_t splash_name[] = L"/Mobius/mobius.bmp";
    static const wchar_t progress_name[] = L"/Mobius/progress.bmp";
    videomode_t old_mode;
    params_vid_t params;
    void *data;

    vid = FsOpen(SYS_DEVICES L"/Classes/video0", 0);
    if (vid == 0)
        goto error0;

    FsRequestSync(vid, VID_GETMODE, &params, sizeof(params), NULL);
    old_mode = params.vid_getmode;

    memset(&params, 0, sizeof(params));
    params.vid_setmode.width = 640;
    params.vid_setmode.height = 480;
    params.vid_setmode.bitsPerPixel = 4;
    if (!FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), NULL))
        goto error1;

    if (!AccelCreateSurface(&params.vid_setmode, vid, &surf))
        goto error2;

    surf.vtbl->SurfFillRect(&surf, 
        0, 0, 
        params.vid_setmode.width, params.vid_setmode.height,
        0);
    surf.vtbl->SurfFlush(&surf);

    data = MonitorLoadImage(splash_name);
    image_progress = MonitorLoadImage(progress_name);

    if (data != NULL)
    {
        unsigned width, height;
        /*rgb_t palette[16];

        MonitorGetPalette(data, palette);
        params.vid_storepalette.entries = palette;
        params.vid_storepalette.length = sizeof(palette);
        params.vid_storepalette.first_index = 0;
        FsRequestSync(vid, VID_STOREPALETTE, &params, sizeof(params), NULL);*/

        MonitorGetSize(data, &width, &height);
        MonitorDisplayImage(&surf, data, 0, 0, width, height);
        VmmFree(data);
    }

    return true;

error2:
    params.vid_setmode = old_mode;
    FsRequestSync(vid, VID_SETMODE, &params, sizeof(params), NULL);
error1:
    HndClose(vid);
error0:
    return false;
}


void TextWriteString(unsigned x, unsigned y, const wchar_t *str, 
                     size_t length, uint8_t attribs)
{
    char mbstr[80], *ch;
    uint16_t *ptr;

    wcsto437(mbstr, str, min(length, _countof(mbstr)));

    ch = mbstr;
    ptr = text + y * 80 + x;
    while (length > 0)
    {
        *ptr = (uint8_t) *ch | (attribs << 8);
        ptr++;
        ch++;
        length--;
    }
}


void MonitorUpdateProgress(unsigned i)
{
    unsigned progress, width, height;

    if (image_progress == NULL)
        TextWriteString(6 + i * 3, 20, L"\x2588\x2588", 2, 0x07);
    else
    {
        MonitorGetSize(image_progress, &width, &height);
        progress = i * 64;
        if (progress > width)
            progress = width;

        MonitorDisplayImage(&surf, image_progress, 64, 29, progress, height);
    }
}


#define MOUNT       0
#define DEVICE      1
#define SERVER      2
#define PROCESS     3
#define SPLASH      4
#define PROGRESS    5

typedef struct boot_t boot_t;
struct boot_t
{
    unsigned action;
    union
    {
        struct
        {
            const wchar_t *filesys;
            const wchar_t *path;
            const wchar_t *device;
        } mount;

        struct
        {
            const wchar_t *driver;
            const wchar_t *device;
        } device;

        struct
        {
            const wchar_t *name;
        } server, process;

        struct 
        {
            const wchar_t *filename;
        } image;
    } u;
};


static const boot_t boot[] =
{
    { DEVICE,   { { L"ata", L"ata-primary" } } },
    { PROGRESS },
    { DEVICE,   { { L"ata", L"ata-secondary" } } },
    { PROGRESS },
    { MOUNT,    { { boot_fsd, L"/", boot_device } } },
    { PROGRESS },
};


static const boot_t fdboot[] =
{
    { MOUNT,    { { boot_fsd, L"/", boot_device } } },
    { PROGRESS },
    { DEVICE,   { { L"ata", L"ata-primary" } } },
    { PROGRESS },
    { DEVICE,   { { L"ata", L"ata-secondary" } } },
    { PROGRESS },
};


static const boot_t common[] =
{
    { MOUNT,    { { L"devfs", SYS_DEVICES, L"" } } },
    { PROGRESS },
    { DEVICE,   { { L"video", L"video" } } },
    { PROGRESS },
    { SPLASH,   { { L"/Mobius/mobius.bmp" } } },
    { MOUNT,    { { L"portfs", SYS_PORTS, L"" } } },
    { PROGRESS },
    { DEVICE,   { { L"keyboard", L"keyboard" } } },
    { PROGRESS },
    { DEVICE,   { { L"ps2mouse", L"ps2mouse" } } },
    { PROGRESS },
    { DEVICE,   { { L"sermouse", L"sermouse" } } },
    { PROGRESS },
    { DEVICE,   { { L"fdc", L"fdc" } } },
    { PROGRESS },
    //{ SERVER, { { L"/Mobius/winmgr.exe" } } },
    { SERVER,   { { L"/Mobius/console.exe" } } },
    { PROCESS,  { { L"/Mobius/shell.exe" } } },
    { PROCESS,  { { L"/Mobius/shell.exe" } } },
};


void ThrMiniStartup(void)
{
    thread_info_t *thr;
    thr = ThrGetThreadInfo();
    ThrExitThread(thr->entry(thr->param));
}


int MonitorDeviceFinderThread(void *param);

void mainCRTStartup(void)
{
    static const wchar_t top_message[] = L"Starting The Möbius";
    unsigned i, first_count, progress;
    handle_t text_handle, proc;
    const boot_t *first_part, *entry;
    process_info_t *proc_info;

    proc_info = ProcGetProcessInfo();
    proc_info->thread_entry = ThrMiniStartup;

    text_handle = HndOpen(L"_fb_text");
    text = VmmMapSharedArea(text_handle, 0, 
        VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);
    HndClose(text_handle);

    MonitorParseCommandLine(proc_info->cmdline);

    if (wcscmp(boot_fsd, L"tarfs") == 0)
    {
        first_part = fdboot;
        first_count = _countof(fdboot);
    }
    else
    {
        first_part = boot;
        first_count = _countof(boot);
    }

    memset(text, 0, 80 * 25 * sizeof(*text));
    TextWriteString(6, 1, top_message, _countof(top_message), 0x07);
    progress = 0;

    for (i = 0; i < first_count + _countof(common); i++)
    {
        if (i < first_count)
            entry = first_part + i;
        else
            entry = common - first_count + i;

        switch (entry->action)
        {
        case MOUNT:
            _wdprintf(L"---- FsMount(%s, %s, %s)\n",
                entry->u.mount.path, entry->u.mount.filesys, entry->u.mount.device);
            FsMount(entry->u.mount.path, 
                entry->u.mount.filesys, 
                entry->u.mount.device);
            if (_wcsicmp(entry->u.mount.path, SYS_DEVICES) == 0)
            {
                handle_t c;
                c = ThrCreateThread(MonitorDeviceFinderThread, NULL, 16, L"MonitorDeviceFinderThread");
                HndClose(c);
            }
            break;

        case DEVICE:
            _wdprintf(L"---- DevInstallIsaDevice(%s, %s)\n",
                entry->u.device.driver, entry->u.device.device);
            DevInstallIsaDevice(entry->u.device.driver, 
                entry->u.device.device);
            break;

        case SERVER:
            _wdprintf(L"---- SERVER(%s)\n",
                entry->u.server.name);
            proc = ProcSpawnProcess(entry->u.server.name, 
                ProcGetProcessInfo());
            HndClose(proc);
            break;

        case PROCESS:
            {
                process_info_t info;
                handle_t client;

                info = *ProcGetProcessInfo();
                do
                {
                    client = FsOpen(SYS_PORTS L"/console", FILE_READ | FILE_WRITE);
                    if (client == 0)
                        ThrSleep(500);
                } while (client == 0);

                info.std_in = info.std_out = client;
                memset(info.cmdline, 0, sizeof(info.cmdline));
                wcscpy(info.cmdline, entry->u.process.name);
                memset(info.cwd, 0, sizeof(info.cwd));
                wcscpy(info.cwd, L"/");

                _wdprintf(L"---- PROCESS(%s, %s)\n",
                    entry->u.process.name,
                    info.cwd);
                proc = ProcSpawnProcess(entry->u.process.name,
                    &info);
                HndClose(proc);
            }
            break;

        case SPLASH:
            MonitorShowSplash();
            progress = 0;
            break;

        case PROGRESS:
            MonitorUpdateProgress(progress);
            progress++;
            break;
        }
    }

    ThrSleep(0x7fffffff);
    VmmFree(text);

    if (image_progress != NULL)
        VmmFree(image_progress);

    HndClose(vid);
    ProcExitProcess(0);
}
