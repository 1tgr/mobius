/* $Id: rcdevice.cpp,v 1.1 2002/09/13 23:23:01 pavlovskii Exp $ */

#include <mgl/rc.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/video.h>
#include <os/rtl.h>

#include <stdlib.h>
#include <string.h>

using namespace mgl;

RcDevice::RcDevice(const wchar_t *video) : Rc(NULL)
{
    m_frame_buffer = NULL;
    m_video = FsOpen(video, 0);
    m_area = NULL;
    if (m_video != NULL)
        Init();
}

RcDevice::RcDevice(handle_t video) : Rc(NULL)
{
    m_frame_buffer = NULL;
    m_area = NULL;
    m_video = video;
    if (m_video != NULL)
        Init();
}

RcDevice::~RcDevice()
{
    if (m_frame_buffer != NULL)
        VmmFree(m_frame_buffer);
}

void RcDevice::Init()
{
    params_vid_t params;
    Bitmap *bitmap;
    BitmapDescriptor desc;

    if (m_frame_buffer != NULL)
    {
        /*VmmFree(m_frame_buffer);*/
        m_frame_buffer = NULL;
    }

    Attach(NULL);
    if (m_video != NULL &&
        FsRequestSync(m_video, VID_GETMODE, &params, sizeof(params), NULL))
    {
        m_mode = params.vid_getmode;

        if (m_mode.flags & VIDEO_MODE_TEXT)
        {
            SetMode(0, 0, 8);
            return;
        }

        if (m_area != NULL)
            HndClose(m_area);
        m_area = VmmOpenSharedArea(params.vid_getmode.framebuffer);
        m_frame_buffer = VmmMapSharedArea(m_area, NULL, 
            VM_MEM_USER | VM_MEM_READ | VM_MEM_WRITE);

        if (m_frame_buffer != NULL)
        {
            desc.bpp = params.vid_getmode.bitsPerPixel;
            desc.bytes_per_line = params.vid_getmode.bytesPerLine;
            desc.width = params.vid_getmode.width;
            desc.height = params.vid_getmode.height;
            bitmap = new Bitmap(m_frame_buffer, desc);
            Attach(bitmap);
            bitmap->Release();
        }
    }
}

bool RcDevice::SetMode(unsigned width, unsigned height, unsigned char bpp)
{
    params_vid_t params;

    if (m_video == NULL)
        return false;

    memset(&params, 0, sizeof(params));
    params.vid_setmode.width = width;
    params.vid_setmode.height = height;
    params.vid_setmode.bitsPerPixel = bpp;
    if (!FsRequestSync(m_video, VID_SETMODE, &params, sizeof(params), NULL))
        return false;

    Init();
    return true;
}
