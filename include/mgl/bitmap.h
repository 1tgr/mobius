/* $Id: bitmap.h,v 1.2 2002/12/18 23:54:44 pavlovskii Exp $ */

#ifndef __MGL_BITMAP_H
#define __MGL_BITMAP_H

#include "types.h"
#include "refcount.h"

namespace mgl
{

struct MGL_EXPORT BitmapDescriptor
{
    unsigned char bpp;
    size_t bytes_per_line;
    unsigned width;
    unsigned height;

    BitmapDescriptor()
    {
        bpp = 0;
        bytes_per_line = 0;
        width = height = 0;
    }
};

class Rc;

class MGL_EXPORT Bitmap : public RefCount<Bitmap>
{
protected:
    void *m_buffer;
    BitmapDescriptor m_desc;
    friend class Rc;
    bool m_locked;
    bool m_owns_buffer;

    void Draw8bpp(Bitmap *dest, int x, int y);
    void Draw16bpp(Bitmap *dest, int x, int y);

public:
    /*Bitmap()
        : m_buffer(0)
    {
    }*/

    Bitmap(void *buffer, BitmapDescriptor& desc) 
        : m_buffer(buffer), m_desc(desc), m_owns_buffer(false)
    {
    }

    Bitmap(unsigned width, unsigned height, unsigned char bpp);
    virtual ~Bitmap();

    void Draw(Rc *dest, MGLreal x, MGLreal y);

    void *Lock()
    {
        /*if (m_locked)
            return 0;
        else*/
        {
            m_locked = true;
            return m_buffer;
        }
    }

    void Unlock()
    {
        m_locked = false;
    }

    unsigned char GetColourDepth() const
    {
        return m_desc.bpp;
    }

    size_t GetPitch() const
    {
        return m_desc.bytes_per_line;
    }

    unsigned GetWidth() const
    {
        return m_desc.width;
    }

    unsigned GetHeight() const
    {
        return m_desc.height;
    }
};

template<class T> class BitmapLock
{
protected:
    Bitmap *m_bitmap;
    T m_pointer;

public:
    BitmapLock(Bitmap *bitmap)
    {
        m_bitmap = bitmap;
        m_pointer = static_cast<T>(bitmap->Lock());
    }

    ~BitmapLock()
    {
        m_bitmap->Unlock();
    }

    operator T()
    {
        return m_pointer;
    }
};

};

#endif
