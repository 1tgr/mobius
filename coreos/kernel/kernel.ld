/*
 * Mobius kernel linker script: kernel.ld
 * Modified from Cygwin's i386pe.x
 */

OUTPUT_FORMAT(pei-i386)
SEARCH_DIR(/mn/lib)
SECTIONS
{
  _scode = __image_base__;
  .text  __image_base__ + __section_alignment__  :
  {
     *(.init)
    *(.text)
    *(SORT(.text$*))
    *(.glue_7t)
    *(.glue_7)
     ___CTOR_LIST__ = .; __CTOR_LIST__ = . ;
			LONG (-1); *(.ctors); *(.ctor); LONG (0);
     ___DTOR_LIST__ = .; __DTOR_LIST__ = . ;
			LONG (-1); *(.dtors); *(.dtor);  LONG (0);
     *(.fini)
    /* ??? Why is .gcc_exc here?  */
     *(.gcc_exc)
  }
  .dtext BLOCK(__section_alignment__) :
  {
     *(.dtext)
     etext = .;
    *(.gcc_except_table)
  }
  /* The Cygwin32 library uses a section to avoid copying certain data
     on fork.  This used to be named ".data".  The linker used
     to include this between __data_start__ and __data_end__, but that
     breaks building the cygwin32 dll.  Instead, we name the section
     ".data_cygwin_nocopy" and explictly include it after __data_end__. */
  .data BLOCK(__section_alignment__) :
  {
    __data_start__ = . ;
    *(.data)
    *(.data2)
    *(.ddata)
    *(SORT(.data$*))
    __data_end__ = . ;
    *(.data_cygwin_nocopy)
  }
  .rdata BLOCK(__section_alignment__) :
  {
    *(.rdata)
    *(SORT(.rdata$*))
    *(.eh_frame)
  }
  .pdata BLOCK(__section_alignment__) :
  {
    *(.pdata)
  }
  .edata BLOCK(__section_alignment__) :
  {
    *(.edata)
  }
  /DISCARD/ :
  {
    *(.debug$S)
    *(.debug$T)
    *(.debug$F)
    *(.drectve)
  }

  /*
   * Mobius kernel: The kernel doesn't import anything.
   * We save 4096 bytes by omitting these five zeroes.
   */
  .idata BLOCK(__section_alignment__) :
  {
    /* This cannot currently be handled with grouped sections.
	See pe.em:sort_sections.  */
    SORT(*)(.idata$2)
    SORT(*)(.idata$3)
    /* These zeroes mark the end of the import list.  */
    LONG (0); LONG (0); LONG (0); LONG (0); LONG (0);
    SORT(*)(.idata$4)
    SORT(*)(.idata$5)
    SORT(*)(.idata$6)
    SORT(*)(.idata$7)
  }

  .CRT BLOCK(__section_alignment__) :
  {
    *(SORT(.CRT$*))
  }
  .endjunk BLOCK(__section_alignment__) :
  {
    /* end is deprecated, don't use it */
     end = .;
     _end = .;
     __end__ = .;
    _edata = .;
  }
  .rsrc BLOCK(__section_alignment__) :
  {
    *(.rsrc)
    *(SORT(.rsrc$*))
  }
  .reloc BLOCK(__section_alignment__) :
  {
    *(.reloc)
  }
  .bss BLOCK(__section_alignment__) :
  {
    __bss_start__ = . ;
	_sbss = .;

	/*
	 * Added stack and kernel page directory.
	 * These are in the linker script to ensure alignment and size:
	 *	it is vital that the page directory be page-aligned.
	 */

	/*_stack = .;
	. += 8192;
	_stack_end = .;
	_kernel_stack_end = .;*/

	_arch_df_stack = .;
	. += 4096;

	*(.pages)
	*(.bss)
    *(COMMON)

	. = ALIGN(4096);
	_ebss = .;
    __bss_end__ = . ;
  }
  .stab BLOCK(__section_alignment__) (NOLOAD) :
  {
    [ .stab ]
  }
  .stabstr BLOCK(__section_alignment__) (NOLOAD) :
  {
    [ .stabstr ]
  }
}
