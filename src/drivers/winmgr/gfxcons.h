#ifndef __GFXCONS_H
#define __GFXCONS_H

#include "console.h"
#include <gui/winserve.h>

class CGfxConsole : public CConsole
{
protected:
	IFont* m_font;
	ISurface* m_surf;
	IWindow* m_wnd;

	virtual ~CGfxConsole();
	virtual void UpdateCursor();
	virtual void Output(int x, int y, wchar_t c, word attrib);
	virtual void Clear();
	virtual void Scroll(int dx, int dy);

public:
	CGfxConsole();
};

#endif