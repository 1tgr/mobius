#ifndef __TEXTCONS_H
#define __TEXTCONS_H

#include "console.h"

class CTextConsole : public CConsole
{
protected:
	word* m_buffer;

	virtual void UpdateCursor();
	virtual void Output(int x, int y, wchar_t c, word attrib);
	virtual void Clear();
	virtual void Scroll(int dx, int dy);

public:
	CTextConsole();
};

#endif