#if 0

#define INITGUID
#include <os/os.h>
#include <os/stream.h>
#include <stdlib.h>
#include <string.h>

const wchar_t *msg1[] =
{
	L"Divide error", L"Debug exception", L"NMI", L"INT3",
	L"INTO", L"BOUND overrange", L"Invalid opcode", L"No coprocessor",
	L"Double fault", L"Coprocessor segment overrun",
		L"Bad TSS", L"Segment not present",
	L"Stack fault", L"GPF", L"Page fault", L"??",
	L"Coprocessor error", L"Alignment check", L"??", L"??",
	L"??", L"??", L"??", L"??",
	L"??", L"??", L"??", L"??",
	L"??", L"??", L"??", L"??"
};

const wchar_t *msg2[] =
{
	L"Missing DLL", L"Missing import", L"DllMain returned false"
};

void itow(dword value, wchar_t* string, int radix)
{
	wchar_t buf[9], *ch = buf + countof(buf) - 1;
	byte n;
	
	*ch = 0;
	do
	{
		ch--;
		n = value % radix;
		if (n > 9)
			*ch = 'a' + n - 10;
		else
			*ch = '0' + n;
		value /= radix;
	} while (value);

	while (*ch)
		*string++ = *ch++;

	*string = 0;
}

void fputws(const wchar_t* text, IStream* strm)
{
	IStream_SetIoMode(strm, ioUnicode);
	IStream_Write(strm, text, wcslen(text) * sizeof(wchar_t));
}

void fputs(const char* text, IStream* strm)
{
	IStream_SetIoMode(strm, ioAnsi);
	IStream_Write(strm, text, strlen(text) * sizeof(char));
}

//! Performs default handling for an unhandled exception in the current process.
/*!
 * This function is called by sysException if a suitable exception handler was
 *	not found after an exception was triggered.
 * It prints information about the exception, and the process that caused it, 
 *	on the console, and terminates the process.
 * \return	This function terminates the process; it does not return.
 */
void STDCALL sysUnhandledException(dword code, addr_t address, addr_t eip)
{
	IUnknown* unk;
	module_info_t *info, *module;

	unk = sysOpen(L"devices/screen");
	if (unk)
	{
		IStream* strm;
		wchar_t num[9];

		if (SUCCEEDED(IUnknown_QueryInterface(unk, &IID_IStream, &strm)))
		{
			module = NULL;
			for (info = thrGetInfo()->process->module_first; info; info = info->next)
			{
				if (address >= info->base && address < info->base + info->length)
				{
					module = info;
					break;
				}
			}
		
			if (code < 0x80000000)
				fputws(msg1[code], strm);
			else
				fputws(msg2[code - 0x80000000], strm);
			
			fputws(L" in ", strm);
			if (module)
				fputws(module->name, strm);
			else
				fputws(L"(unknown)", strm);
			
			fputws(L" at address 0x", strm);
			itow(eip, num, 16);
			fputws(num, strm);

			switch (code)
			{
			case EXCEPTION_PAGE_FAULT:
				fputws(L": accessed 0x", strm);
				itow(address, num, 16);
				fputws(num, strm);
				break;

			case EXCEPTION_MISSING_DLL:
			case EXCEPTION_MISSING_IMPORT:
				fputws(L" ", strm);
				fputs((const char*) address, strm);
				break;
			}

			fputws(L"\n", strm);
			IUnknown_Release(strm);
		}

		IUnknown_Release(unk);
	}

	procExit();
}

//! Triggers an exception in the current process.
/*!
 * Called by the kernel in the event of an unhandled user-mode exception.
 * This function loops through the exception handler frames currently set
 *	up, calling each one in turn. If a suitable handler was not found, it
 *	calls sysUnhandledException to terminate the process.
 * \return No return value. If the exception is not handled, this function
 *	will not return.
 */
void sysException(dword code, addr_t address, addr_t eip)
{
	exception_registration_t* reg = NULL;
	exception_info_t info = { code, address, eip };
		
	__asm
	{
		mov eax, fs:[0]
		mov reg, eax
	}
	
	while (reg)
	{
		if (reg->handler)
		{
			switch (reg->handler(&info, reg, (context_t*) 0x12345678, NULL))
			{
			case ExceptionContinueExecution:
				return;
			}
		}

		reg = reg->prev;
	}

	//sysUnhandledException(code, address, eip);
	__asm int 3
}

#endif