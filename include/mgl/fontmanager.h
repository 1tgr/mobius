/* $Id: fontmanager.h,v 1.2 2002/12/18 23:54:44 pavlovskii Exp $ */

#ifndef __MGL_FONTMANAGER_H
#define __MGL_FONTMANAGER_H

struct FT_LibraryRec_;

#include "font.h"

namespace mgl
{

class MGL_EXPORT FontManager
{
protected:
    FT_LibraryRec_ *m_ft_library;

    static FontManager *m_instance;
    static Font *m_defaults[];
    friend class Font;

public:
    FontManager();
    virtual ~FontManager();

    static FontManager *GetInstance()
    {
        return m_instance;
    }

    static Font *GetDefault(int index);
};

};

#endif
