#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <os/os.h>
#include <os/pe.h>

wchar_t *_getws(wchar_t *buffer)
{
	wchar_t* ch = buffer;

	while (true)
	{
		*ch = _wgetch();

		switch (*ch)
		{
		case 0:
			break;

		case L'\n':
			*ch = 0;
			putwchar(L'\n');
			return buffer;

		case L'\b':
			if (ch > buffer)
			{
				_cputws(L"\b \b");
				ch--;
			}
			break;

		default:
			putwchar(*ch);
			ch++;
		}
	}
}

#if 0
typedef struct exception_pointers_t exception_pointers_t;
struct exception_pointers_t
{
    exception_info_t* info;
    context_t* ctx; 
};

// Data structure(s) pointed to by Visual C++ extended exception frame

struct scopetable_entry
{
	dword       previousTryLevel;
	void*     lpfnFilter;
	void*     lpfnHandler;
};

typedef struct exception_registration_vc_t exception_registration_vc_t;
struct exception_registration_vc_t
{
	exception_registration_t reg;
	struct scopetable_entry *scopetable;
	int trylevel;
	int _ebp;
	exception_pointers_t* xpointers;
};

#define TRYLEVEL_NONE					0xffffffff
#define EXCEPTION_CONTINUE_EXECUTION	-1
#define EXCEPTION_CONTINUE_SEARCH		0
#define EXCEPTION_EXECUTE_HANDLER		1

// The extended exception frame used by Visual C++

struct VC_EXCEPTION_REGISTRATION
{
	exception_registration_t reg;
	struct scopetable_entry *  scopetable;
	int                 trylevel;
	int                 _ebp;
};

ExceptionDisposition _except_handler3(
	exception_info_t *ExceptionRecord,
	void * EstablisherFrame,
	context_t *ContextRecord,
	void * DispatcherContext);

//----------------------------------------------------------------------------
// Code
//----------------------------------------------------------------------------

//
// Display the information in one exception frame, along with its scopetable
//

void ShowSEHFrame( struct VC_EXCEPTION_REGISTRATION * pVCExcRec )
{
	struct scopetable_entry * pScopeTableEntry = pVCExcRec->scopetable;
	int i;
	
	wprintf( L"Frame: %08X  Handler: %08X  Prev: %08X  Scopetable: %08X\n",
		pVCExcRec, pVCExcRec->reg.handler, pVCExcRec->reg.prev,
		pVCExcRec->scopetable );
	
	for (i = 0; i <= pVCExcRec->trylevel; i++ )
	{
		wprintf( L"    scopetable[%u] PrevTryLevel: %08X  "
			L"filter: %08X  __except: %08X\n", i,
			pScopeTableEntry->previousTryLevel,
			pScopeTableEntry->lpfnFilter,
			pScopeTableEntry->lpfnHandler );
		
		pScopeTableEntry++;
	}
	
	//wprintf( L"\n" );
}   

//
// Walk the linked list of frames, displaying each in turn
//

void WalkSEHFrames( void )
{
	struct VC_EXCEPTION_REGISTRATION * pVCExcRec;
	
	// Print out the location of the __except_handler3 function
	wprintf( L"_except_handler3 is at address: %08X\n", _except_handler3 );
	//wprintf( L"\n" );
	
	// Get a pointer to the head of the chain at FS:[0]
	asm("mov %%fs(0), %%eax" : "=a" (pVCExcRec));
	
	// Walk the linked list of frames.  0xFFFFFFFF indicates the end of list
	while (  NULL != pVCExcRec )
	{
		ShowSEHFrame( pVCExcRec );
		pVCExcRec = (struct VC_EXCEPTION_REGISTRATION *)(pVCExcRec->reg.prev);
	}
}

ExceptionDisposition _except_handler3(
	exception_info_t *ExceptionRecord,
	void * EstablisherFrame,
	context_t *ContextRecord,
	void * DispatcherContext)
{
	struct VC_EXCEPTION_REGISTRATION * pVCExcRec;
	unsigned tryLevel, temp;
	int val;
	dword func;
	exception_pointers_t exceptPtrs;
	
	wprintf(L"Exception: %08x at %08x\n", ExceptionRecord->code, ExceptionRecord->address);
	//WalkSEHFrames();

	// Build the EXCEPTION_POINTERS structure on the stack
	exceptPtrs.info = ExceptionRecord;
	exceptPtrs.ctx = ContextRecord;

	// Get a pointer to the head of the chain at FS:[0]
	//__asm   mov eax, FS:[0]
	//__asm   mov [pVCExcRec], EAX
	pVCExcRec = (struct VC_EXCEPTION_REGISTRATION*) EstablisherFrame;

	// Put the pointer to the EXCEPTION_POINTERS 4 bytes below the
	// establisher frame.  See ASM code for GetExceptionInformation
	*(dword*)((byte*)pVCExcRec - 4) = (dword) &exceptPtrs;
	
	tryLevel = pVCExcRec->trylevel;
	while (tryLevel != -1)
	{
		temp = pVCExcRec->_ebp;
		func = (dword) pVCExcRec->scopetable[tryLevel].lpfnFilter;
		
		wprintf(L"tryLevel = %d, ebp = %x, filter = %x\n", 
			tryLevel, temp, func);

		__asm
		{
			mov		eax, func
			push	ebp
			mov		ebp, temp
			call	eax
			pop		ebp
			mov		val, eax
		}

		switch (val)
		{
		case EXCEPTION_CONTINUE_SEARCH:
			wprintf(L"Continue search\n");
			break;
		case EXCEPTION_EXECUTE_HANDLER:
			func = (dword) pVCExcRec->scopetable[tryLevel].lpfnHandler;
			wprintf(L"Execute handler = %x, ebp = %x, esp = %x\n", 
				func, temp, ((dword*) temp)[-0x18]);
			__asm
			{
				mov		eax, func
				push	ebp
				mov		ebp, temp
				call	eax
				pop		ebp
			}
			wprintf(L"Returned\n");
			break;
		case EXCEPTION_CONTINUE_EXECUTION:
			wprintf(L"Continue execution\n");
			return ExceptionContinueExecution;
		}

		//pVCExcRec = (struct VC_EXCEPTION_REGISTRATION *)(pVCExcRec->reg.prev);
		tryLevel = pVCExcRec->scopetable[tryLevel].previousTryLevel;
	}

	wprintf(L"Returning\n");
	return ExceptionContinueSearch;
}
#endif

int main()
{
	wchar_t buf[256], args[256], *ch;
	addr_t proc;
	bool async;
	
	//sysExec(L"console.exe", NULL);

	if (resLoadString(0x40000000, 1, buf, countof(buf)))
		_cputws(buf);
	
	while (true)
	{
		wprintf(L"[%s] ", thrGetInfo()->process->cwd);
		if (*_getws(buf))
		{
			ch = wcschr(buf, ' ');
			if (ch)
			{
				size_t len;

				wcscpy(args, ch + 1);
				*ch = '\0';

				len = wcslen(args);
				if (args[len - 1] == '&')
				{
					args[len - 1] = 0;
					async = true;
				}
				else
					async = false;
			}
			else
			{
				args[0] = '\0';
				async = false;
			}

			//for (i = 0; i < 800; i++)
			{
				//wprintf(L"%3d\t", i);
				if (wcsicmp(buf, L"exit") == 0)
					break;
				else if (wcsicmp(buf, L"cd") == 0)
					wcscpy(thrGetInfo()->process->cwd, args);
				else if (wcsicmp(buf, L"break") == 0)
					asm("int3");
				else
				{
					if (!(proc = sysExec(buf, args)))
					{
						wcscat(buf, L".exe");
						if (!(proc = sysExec(buf, args)))
						{
							wprintf(L"%s: not found\n", buf);
							continue;
						}
					}

					if (!async)
						thrWaitHandle(&proc, 1, false);
				}
			}
		}
	}

	return 0;
}
