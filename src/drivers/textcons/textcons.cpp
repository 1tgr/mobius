#define INITGUID
#include "textcons.h"
#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <string.h>

#define VGA_CRTC_INDEX      0x3D4
#define VGA_CRTC_DATA       0x3D5

#pragma code_seg(".init")

extern "C" bool STDCALL drvInit()
{
	_cputws_check(L" \n" CHECK_L2 L"Console\r\t");
	sysExtRegisterDevice(L"screen", new CTextConsole);
	return true;
}

/*****************************************************************************
 * CTextConsole                                                              *
 *****************************************************************************/

CTextConsole::CTextConsole()
{
	m_buffer = (word*) 0xb8000;
	//m_buffer = (dword*) 0xff000000;
	m_width = 80;
	m_height = 25;
	//Clear();
}

#pragma code_seg()

void CTextConsole::UpdateCursor()
{
	unsigned short Off;

	Off = (m_con_y * m_width + m_con_x) * 2;
	// extra shift because even/odd text mode uses word clocking
	out(VGA_CRTC_INDEX, 14);
	out(VGA_CRTC_DATA, Off >> 9);
	out(VGA_CRTC_INDEX, 15);
	out(VGA_CRTC_DATA, Off >> 1);
}

void CTextConsole::Output(int x, int y, wchar_t c, word attrib)
{
	char sc[2];
	wcstombs(sc, &c, 1);
	m_buffer[y * m_width + x] = (unsigned char) sc[0] | (attrib & 0xff00);
}

void CTextConsole::Clear()
{
	word w = m_attrib | ' ';
	m_con_x = m_con_y = 0;
	i386_lmemset32((addr_t) m_buffer, 
		(dword) w << 16 | w, 
		m_width * m_height * sizeof(*m_buffer));
}

void CTextConsole::Scroll(int dx, int dy)
{
	i386_llmemcpy((addr_t) m_buffer, (addr_t) (m_buffer - dy * m_width), 
		sizeof(*m_buffer) * m_width * (m_height + dy));

	i386_lmemset32((addr_t) (m_buffer + m_width * (m_height + dy)), 
		(dword) (m_attrib | ' ') << 16 | m_attrib | ' ', 
		sizeof(*m_buffer) * m_width * -dy);
}
