/* $Id: fontmanager.cpp,v 1.2 2002/12/18 23:06:10 pavlovskii Exp $ */

#include <mgl/fontmanager.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <os/defs.h>

using namespace mgl;

static const wchar_t *default_files[] =
{
    SYS_BOOT L"/lucsdm.ttf",
    SYS_BOOT L"/luctr.ttf",
};

FontManager *FontManager::m_instance;
Font *FontManager::m_defaults[_countof(default_files)];

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
        m_defaults[index] = new Font(default_files[index], 200);

    return m_defaults[index];
}
