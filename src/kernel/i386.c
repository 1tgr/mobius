#include <kernel/i386.h>

//! Sets up a a code or data entry in the IA32 GDT or LDT
/*!
 *	\param	item	Pointer to a descriptor table entry
 *	\param	base	Address of the base used for the descriptor
 *	\param	limit	Size of the end of the code or data referenced by the 
 *		descriptor, minus one
 *	\param	access	Access permissions applied to the descriptor
 *	\param	attribs	Attributes applied to the descriptor
 */
void i386_set_descriptor(descriptor_t *item, 
	dword base, dword limit, byte access, byte attribs)
{
	item->base_l = base & 0xFFFF;
	item->base_m = (base >> 16) & 0xFF;
	item->base_h = base >> 24;
	item->limit = limit & 0xFFFF;
	item->attribs = attribs | ((limit >> 16) & 0x0F);
	item->access = access;
}

//! Sets up an interrupt, task or call gate in the IA32 IDT
/*!
 *	\param	item	Pointer to an IDT entry
 *	\param	selector	The CS value referred to by the gate
 *	\param	offset	The EIP value referred to by the gate
*	\param	access	Access permissions applied to the gate
 *	\param	attribs	Attributes applied to the gate
 */
void i386_set_descriptor_int(descriptor_int_t *item,
	word selector, dword offset, byte access, byte param_cnt)
{
	item->selector = selector;
	item->offset_l = offset & 0xFFFF;
	item->offset_h = offset >> 16;
	item->access = access;
	item->param_cnt = param_cnt;
}
