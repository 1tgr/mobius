#include "console.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*****************************************************************************
 * CConsole                                                                  *
 *****************************************************************************/

CConsole::CConsole()
{
	m_refs = 0;
	m_con_x = 0;
	m_con_y = 0;
	m_width = m_height = 0;
	m_attrib = 0x0700;
	m_esc = 0;
}

// IUnknown methods
HRESULT CConsole::QueryInterface(REFIID iid, void ** ppvObject)
{
	//wprintf(L"QI");
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IDevice))
	{
		//wprintf(L"(IDevice)\n");
		AddRef();
		*ppvObject = (IDevice*) this;
		return S_OK;
	}
	else if (InlineIsEqualGUID(iid, IID_IStream))
	{
		//wprintf(L"(IStream)\n");
		AddRef();
		*ppvObject = (IStream*) new CStream(this);
		return S_OK;
	}

	/*wprintf(L"(fail) %p %p {%08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x} %p\n",
		this,
		iid.Data1, iid.Data2, iid.Data3, 
		iid.Data4[0], 
		iid.Data4[1], 
		iid.Data4[2], 
		iid.Data4[3], 
		iid.Data4[4], 
		iid.Data4[5], 
		iid.Data4[6], 
		iid.Data4[7],
		ppvObject);*/
	*ppvObject = NULL;
	return E_FAIL;
}

ULONG CConsole::AddRef()
{
	return ++m_refs;
}

ULONG CConsole::Release()
{
	if (m_refs == 0)
	{
		delete this;
		return 0;
	}
	else
		return --m_refs;
}

// IDevice method
HRESULT CConsole::GetInfo(device_t* buf)
{
	if (buf->size < sizeof(device_t))
		return E_FAIL;

	wcscpy(buf->name, L"Console");
	return S_OK;
}

HRESULT CConsole::DeviceOpen()
{
	return S_OK;
}

void CConsole::WriteCharacter(dword mode, wchar_t c)
{
	if (mode == ioRaw)
	{
		Output(m_con_x, m_con_y, c, 0);
		m_con_x++;

		if (m_con_x >= m_width)
		{
			m_con_x = 0;
			m_con_y++;
		}
	}
	else
	{
		if (m_esc == 1)
		{
			if (c == L'[')
			{
				m_esc++;
				m_esc1 = 0;
				return;
			}
		}
		else if (m_esc == 2)
		{
			if (iswdigit(c))
			{
				m_esc1 = m_esc1 * 10 + c - L'0';
				return;
			}
			else if (c == ';')
			{
				m_esc++;
				m_esc2 = 0;
				return;
			}
			else if (c == 'J')
			{
				if (m_esc1 == 2)
					Clear();
			}
			else if (c == 'm')
				SetAttrib(m_esc1);

			m_esc = 0;
			return;
		}
		else if (m_esc == 3)
		{
			if (iswdigit(c))
			{
				m_esc2 = m_esc2 * 10 + c - '0';
				return;
			}
			else if(c == ';')
			{
				m_esc++;	/* ESC[num1;num2; */
				m_esc3 = 0;
				return;
			}
			/* ESC[num1;num2H -- move cursor to num1,num2 */
			else if(c == 'H')
			{
				if(m_esc2 < m_width)
					m_con_x = m_esc2;
				if(m_esc1 < m_height)
					m_con_y = m_esc1;
			}
			/* ESC[num1;num2m -- set attributes num1,num2 */
			else if(c == 'm')
			{
				SetAttrib(m_esc1);
				SetAttrib(m_esc2);
			}
			m_esc = 0;
			return;
		}
		/* ESC[num1;num2;num3 */
		else if(m_esc == 4)
		{
			if (iswdigit(c))
			{
				m_esc3 = m_esc3 * 10 + c - '0';
				return;
			}
			/* ESC[num1;num2;num3m -- set attributes num1,num2,num3 */
			else if(c == 'm')
			{
				SetAttrib(m_esc1);
				SetAttrib(m_esc2);
				SetAttrib(m_esc3);
			}
			m_esc = 0;
			return;
		}

		m_esc = 0;
		switch (c)
		{
		case L'\n':
			m_con_x = 0;
			m_con_y++;
			break;

		case L'\r':
			m_con_x = 0;
			break;
		
		case L'\t':
			m_con_x = (m_con_x + 4) & ~3;
			break;

		case L'\b':
			if (m_con_x > 0)
				m_con_x--;
			break;

		case 27:
			m_esc = 1;
			break;

		default:
			Output(m_con_x, m_con_y, c, m_attrib);
			m_con_x++;

			if (m_con_x >= m_width)
			{
				m_con_x = 0;
				m_con_y++;
			}
		}
	}

	while (m_con_y >= m_height)
	{
		m_con_y--;
		Scroll(0, -1);
	}
}

void CConsole::SetAttrib(byte att)
{
	static const char ansi_to_vga[] =
	{
		0, 4, 2, 6, 1, 5, 3, 7
	};
	unsigned char new_att;

	new_att = m_attrib >> 8;
	if(att == 0)
		new_att &= ~0x08;		/* bold off */
	else if(att == 1)
		new_att |= 0x08;		/* bold on */
	else if(att >= 30 && att <= 37)
	{
		att = ansi_to_vga[att - 30];
		new_att = (new_att & ~0x07) | att;/* fg color */
	}
	else if(att >= 40 && att <= 47)
	{
		att = ansi_to_vga[att - 40] << 4;
		new_att = (new_att & ~0x70) | att;/* bg color */
	}

	m_attrib = new_att << 8;
}

/*****************************************************************************
 * CConsole::CStream                                                         *
 *****************************************************************************/

CConsole::CStream::CStream(CConsole* con)
{
	m_con = con;
	m_con->AddRef();
	m_mode = ioUnicode;
	m_refs = 0;
}

CConsole::CStream::~CStream()
{
	if (m_con)
		m_con->Release();
}

// IUnknown methods
HRESULT CConsole::CStream::QueryInterface(REFIID iid, void ** ppvObject)
{
	//wprintf(L"QI");
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IStream))
	{
		//wprintf(L"(IDevice)\n");
		AddRef();
		*ppvObject = (IStream*) this;
		return S_OK;
	}
	else if (InlineIsEqualGUID(iid, IID_IDevice))
	{
		//wprintf(L"(IStream)\n");
		m_con->AddRef();
		*ppvObject = (IDevice*) m_con;
		return S_OK;
	}

	//wprintf(L"(fail)\n");
	*ppvObject = NULL;
	return E_FAIL;
}

ULONG CConsole::CStream::AddRef()
{
	return ++m_refs;
}

ULONG CConsole::CStream::Release()
{
	if (m_refs == 0)
	{
		delete this;
		return 0;
	}
	else
		return --m_refs;	
}

// IStream methods
size_t CConsole::CStream::Read(void* buffer, size_t length)
{
	return 0;
}

size_t CConsole::CStream::Write(const void* buffer, size_t length)
{
	word* ch;
	size_t written = 0;

	for (ch = (word*) buffer; written < length; )
	{
		switch (m_mode)
		{
		case ioRaw:
		case ioUnicode:
			m_con->WriteCharacter(m_mode, *ch);
			ch++;
			written += sizeof(wchar_t);
			break;

		case ioAnsi:
			m_con->WriteCharacter(m_mode, (char) *ch);
			ch = (word*) ((char*) ch + 1);
			written += sizeof(char);
			break;
		}
	}

	m_con->UpdateCursor();
	return written;
}

HRESULT CConsole::CStream::SetIoMode(dword mode)
{
	m_mode = mode;
	return S_OK;
}

HRESULT CConsole::CStream::IsReady()
{
	return S_OK;
}

HRESULT CConsole::CStream::Stat(folderitem_t* buf)
{
	return E_FAIL;
}

HRESULT CConsole::CStream::Seek(long offset, int origin)
{
	return E_FAIL;
}