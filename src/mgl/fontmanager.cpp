/* $Id: fontmanager.cpp,v 1.1 2002/09/13 23:23:01 pavlovskii Exp $ */

#include <mgl/fontmanager.h>
#include <ft2build.h>
#include FT_FREETYPE_H

using namespace mgl;

FontManager *FontManager::m_instance;
Font *FontManager::m_defaults[1];

FontManager::FontManager()
{
    m_instance = this;
    FT_Init_FreeType(&m_ft_library);
}

FontManager::~FontManager()
{
    FT_Done_FreeType(m_ft_library);
    m_instance = NULL;
}

Font *FontManager::GetDefault(int index)
{
    if (index < 0 || index >= (int) _countof(m_defaults))
        return NULL;

    if (m_defaults[index] == NULL)
        m_defaults[index] = new Font(L"/System/Boot/lucsdm.ttf", 200);

    return m_defaults[index];
}
