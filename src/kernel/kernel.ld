/* STARTUP(start.p) */
OUTPUT_FORMAT("pei-i386")
/* ENTRY(_start) */

SECTIONS
{
	_scode = 0xc0000000;
	
	/* kernel code */
	.text 0xc0001000 :
	{
		*(.text)
		_ecode = .;
		. = ALIGN(4096);
	}=0

	/* discardable stuff */
	.init :
	{
		*(.init)
		_einit = .;
		. = ALIGN(4096);
	}=0

	/* kernel data */
	.data :
	{
		_bdata = .;
		libc_first_ctor = . ;
		*(.ctor)
		libc_last_ctor = . ;
		libc_first_dtor = . ;
		*(.dtor)
		libc_last_dtor = . ;
		*(.data)
		*(.data.init)
		*(.eh_frame)
		*(.gcc_except_table)
		. = ALIGN(4096);
		_edata = .;
	}

	.edata :
	{
		*(.edata)
		. = ALIGN(4096);
	}

	/* kernel BSS */
	.bss :
	{
		_sbss = .;
		_stack = .;
		. += 8192;
		_stack_end = .;

		. = ALIGN(4096);
		_kernel_pagedir = .;
		. += 4096;

		*(.bss)
		*(COMMON)
		. = ALIGN(4096);
		_ebss = .;
	}

	_end = .;
}
