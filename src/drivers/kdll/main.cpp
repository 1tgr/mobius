#include <os/os.h>
#include <os/filesys.h>
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <os/keyboard.h>
#include <kernel/kernel.h>
#include <stdlib.h>

#include <kernel/driver.h>
#include <kernel/sys.h>
#include "ne2000.h"

#include "pci.h"
#include "config.h"

IFolder *root, *devices;
dword pick_timeout;

extern "C"
{

void msleep(dword ms)
{
	dword end;

	end = sysUpTime() + ms;
	while (sysUpTime() < end)
		;

	/*__asm
	{
		mov eax, 100h
		mov ebx, ms
		int 30h
	}*/
}

void insw(unsigned short _port, unsigned short *_buf, unsigned _len)
{
	__asm
	{
		movzx	edx, _port
		mov		edi, _buf
		mov		ecx, _len
		cld
		rep insw
	}
}

}

extern "C"
{
	bool libc_init();
	void libc_exit();
	IFolder* Folder_Create();
	IFolder* FatRoot_Create(IBlockDevice* pDevice);
	IFolder* RamDisk_Create();
	IDevice* Serial_Create(word base, byte irq);
}

#define CLRSCR	L"\x1b[2J"

void DisplayConfigMenu()
{
	config_t* cfg;
	int y = 6;

	//_cputws(CLRSCR);
	_cputws(L"\x1b[3;8HThe M”bius -- Configuration Menu");
	
	for (cfg = cfg_first; cfg; cfg = cfg->next)
	{
		if (cfg == cfg_current)
			_cputws(L"\x1b[30;47m");
		else
			_cputws(L"\x1b[37;40m");

		wprintf(L"\x1b[%d;10H %s ", y, cfg->name);
		y += 2;
	}

	y += 4;
	_cputws(L"\x1b[37;40m");
	wprintf(L"\x1b[%d;8HChoose the required "
			L"\x1b[%d;8H  configuration with the arrow "
			L"\x1b[%d;8H  keys and press Enter. ", y, y + 1, y + 2);
}

bool PickConfigMenu()
{
	wint_t ch;
	dword now, old_secs = 0;

	while (1)
	{
		while (!_kbhit())
		{
			if (pick_timeout != (dword) -1)
			{
				now = sysUpTime();

				if (now >= pick_timeout)
					return true;

				if (old_secs != (pick_timeout - now) / 1000)
				{
					old_secs = (pick_timeout - now) / 1000;
					wprintf(L"\x1b[0;75H%4d", old_secs);
				}
			}
		}
		
		pick_timeout = (dword) -1;
		switch ((ch = _wgetch()))
		{
		case KEY_UP:
			cfg_current = cfg_current->prev;
			if (!cfg_current)
				cfg_current = cfg_last;
			return false;

		case KEY_DOWN:
			cfg_current = cfg_current->next;
			if (!cfg_current)
				cfg_current = cfg_first;
			return false;

		case '\n':
			return true;

		default:
			if (ch <= 0xffff)
			{
				putwchar(ch);
				putwchar('\b');
			}
			break;
		}
	}
}

#pragma data_seg(".info")
module_info_t __declspec(allocate(".info")) _info;
#pragma data_seg()

extern "C" bool STDCALL drvInit()
{
	IDevice* ne2000;
	IFolder* ram;
	config_t *cfg, *next;
	
	__asm
	{
		mov eax, [_info]
		mov [_info], eax
	}

	root = Folder_Create();
	thrGetInfo()->process->root = root;
	sysMount(L"root", (IUnknown*) root);

	ram = RamDisk_Create();
	root->Mount(L"boot", (IUnknown*) ram);

	devices = Folder_Create();
	sysMount(L"devmgr", (IUnknown*) devices);
	root->Mount(L"devices", (IUnknown*) devices);

	libc_init();
	//ProcessKernelRc();
	
	if (cfg_first->next)
	{
		pick_timeout = sysUpTime() + cfg_current->timeout;

		do
		{
			DisplayConfigMenu();
		} while (!PickConfigMenu());

		_cputws(CLRSCR);
	}
	
	/*if (cfg_current->display == config_t::dispGui)
	{
		
		
		libc_exit();
		libc_init();
	}*/
	
	_cputws_check(L" \n" CHECK_L2 L"NE2000 card\r\t");
	ne2000 = CNe2000::Detect();
	if (ne2000)
		sysExtRegisterDevice(L"ne2000", ne2000);

	_cputws_check(L" \n" CHECK_L2 L"Serial ports\r\t");
	sysExtRegisterDevice(L"serial0", Serial_Create(0x3f8, 4));
	sysExtRegisterDevice(L"serial1", Serial_Create(0x2f8, 3));

	for (cfg = cfg_first; cfg; cfg = next)
	{
		next = cfg->next;
		free(cfg);
	}

	cfg_first = cfg_last = cfg_current = NULL;
	return true;
}