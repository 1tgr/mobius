/* $Id: refcount.h,v 1.1.1.1 2002/12/31 01:26:22 pavlovskii Exp $ */

#ifndef __MGL_REFCOUNT_H
#define __MGL_REFCOUNT_H

namespace mgl
{

template<class _T> class RefCount
{
protected:
    unsigned m_count;

public:
    RefCount()
    {
        m_count = 0;
    }

    void Retain()
    {
        m_count++;
    }

    void Release()
    {
        if (m_count == 0)
            static_cast<_T*>(this)->FinalRelease();
        else
            m_count--;
    }

    void FinalRelease()
    {
        delete this;
    }
};

};

#endif
