/*****************************************************************************
serial mouse driver
*****************************************************************************/
#include <conio.h> /* kbhit(), getch() */
#include <stdio.h> /* printf() */
#include <dos.h> /* peek() for Turbo C, inportb(), outportb(), delay() */

#if defined(__TURBOC__)
#define	peek40(OFF)	peek(0x40, OFF)

#elif defined(__DJGPP__)
#include <sys/farptr.h> /* _farpeekw() */
#include <go32.h> /* _dos_ds */

#define	peek40(OFF)	_farpeekw(_dos_ds, 0x400 + (OFF))

#else
#error Not Turbo C, not DJGPP. Sorry.
#endif

#define	BUF_SIZE	64

typedef struct	/* circular queue */
{
	unsigned char *data;
	unsigned size, in_ptr, out_ptr;
} queue_t;

static unsigned char _irq, _buffer[BUF_SIZE];
static unsigned short _io_adr;
static queue_t _queue =
{
	_buffer, BUF_SIZE, 0, 0
};

/* which COM port to use (1=COM1, 2=COM2, etc.) */
unsigned char _com = 1;
/*////////////////////////////////////////////////////////////////////////////
	INTERRUPTS
////////////////////////////////////////////////////////////////////////////*/
static const unsigned char _irq_to_vector[] =
{
	8, 9, 10, 11, 12, 13, 14, 15,
	112, 113, 114, 115, 116, 117, 118, 119
};

#if defined(__TURBOC__)
#include <dos.h> /* getvect(), setvect() */

#define	INTERRUPT	interrupt

typedef void interrupt(*vector_t)(void);

#elif defined(__DJGPP__)
#include <dpmi.h>	/* _go32_dpmi_... */
#include <go32.h>	/* _my_cs() */

#define	INTERRUPT	/* nothing */

typedef _go32_dpmi_seginfo vector_t;

#else
#error Not Turbo C, not DJGPP. Sorry.
#endif
/*****************************************************************************
*****************************************************************************/
#if defined(__TURBOC__)
#define SAVE_VECT(num, vec)	vec = getvect(num)

#elif defined(__DJGPP__)
#define	SAVE_VECT(num, vec)	\
	_go32_dpmi_get_protected_mode_interrupt_vector(num, &vec)
#endif
/*****************************************************************************
*****************************************************************************/
#if defined(__TURBOC__)
#define	INSTALL_HANDLER(num, fn)	setvect(num, fn)

#elif defined(__DJGPP__)
#define	INSTALL_HANDLER(num, fn)					\
	{							\
		_go32_dpmi_seginfo new_vector;			\
								\
		new_vector.pm_selector = _my_cs();		\
		new_vector.pm_offset = (unsigned long)fn;	\
		_go32_dpmi_allocate_iret_wrapper(&new_vector);	\
		_go32_dpmi_set_protected_mode_interrupt_vector	\
			(num, &new_vector);			\
	}
#endif
/*****************************************************************************
*****************************************************************************/
#if defined(__TURBOC__)
#define	RESTORE_VECT(num, vec)	setvect(num, vec)

#elif defined(__DJGPP__)
#define	RESTORE_VECT(num, vec)	\
	_go32_dpmi_set_protected_mode_interrupt_vector(num, &vec)
#endif
/*****************************************************************************
*****************************************************************************/
static void enable_irq_at_8259(unsigned short irq)
{
	unsigned char mask;

	if(irq < 8)
	{
		mask = 1 << irq;
		outportb(0x21, inportb(0x21) & ~mask);
	}
	else if(irq < 16)
	{
		irq -= 8;
		mask = 1 << irq;
		outportb(0xA1, inportb(0xA1) & ~mask);
		outportb(0x21, inportb(0x21) & ~0x04);
	}
}
/*****************************************************************************
*****************************************************************************/
static void disable_irq_at_8259(unsigned short irq)
{
	unsigned char mask;

	if(irq < 8)
	{
		mask = 1 << irq;
		outportb(0x21, inportb(0x21) | mask);
	}
	else if(irq < 16)
	{
		irq -= 8;
		mask = 1 << irq;
		outportb(0xA1, inportb(0xA1) | mask);
	}
}
/*****************************************************************************
*****************************************************************************/
#if defined(__TURBOC__)
static unsigned crit_begin(void)
{
	unsigned ret_val;

	asm pushf;
	asm pop [ret_val];
	return ret_val;
}
/*****************************************************************************
*****************************************************************************/
static void crit_end(unsigned flags)
{
	asm push [flags];
	asm popf;
}
#elif defined(__DJGPP__)
/*****************************************************************************
*****************************************************************************/
static __inline__ unsigned crit_begin(void)
{
	unsigned ret_val;

	__asm__ __volatile__("pushfl\n"
		"popl %0\n"
		"cli"
		: "=a"(ret_val)
		:);
	return ret_val;
}
/*****************************************************************************
*****************************************************************************/
static __inline__ void crit_end(unsigned flags)
{
	__asm__ __volatile__("pushl %0\n"
		"popfl"
		:
		: "m"(flags)
		);
}
#endif
/*****************************************************************************
*****************************************************************************/
static int inq(queue_t *q, unsigned char data)
{
	unsigned flags, temp;

	flags = crit_begin();
	temp = q->in_ptr + 1;
	if(temp >= q->size)
		temp = 0;
/* if in_ptr reaches out_ptr, the queue is full */
	if(temp == q->out_ptr)
	{
		crit_end(flags);
		return -1;
	}
	q->data[q->in_ptr] = data;
	q->in_ptr = temp;
	crit_end(flags);
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static int deq(queue_t *q, unsigned char *data)
{
	unsigned flags;

	flags = crit_begin();
/* if in_ptr == out_ptr, the queue is empty */
	if(q->in_ptr == q->out_ptr)
	{
		crit_end(flags);
		return -1;
	}
	*data = q->data[q->out_ptr];
	q->out_ptr++;
	if(q->out_ptr >= q->size)
		q->out_ptr = 0;
	crit_end(flags);
	return 0;
}
/*****************************************************************************
*****************************************************************************/
static void INTERRUPT com_irq(void)
{
	unsigned char temp;

#if defined(__TURBOC__)
pokeb(0xB800, 0, peekb(0xB800, 0) + 1);
#endif
	temp = inportb(_io_adr  + 5);
	if(temp & 0x01)
	{
		temp = inportb(_io_adr + 0);
		(void)inq(&_queue, temp);
	}
	(void)inportb(_io_adr + 2);
/* acknowledge interrupt at 8259s */
	outportb(0x20, 0x20);
	if(_irq >= 8)
		outportb(0xA0, 0x20);
}
/*****************************************************************************
*****************************************************************************/
#define	MAX_ID	16

int main(void)
{
	static const unsigned char com_to_irq[] =
	{
		4, 3, 4, 3
	};
/**/
	unsigned char vect_num, byte, id[MAX_ID], *ptr;
	vector_t old_vector;
	unsigned temp;

	if(_com < 1 || _com > 4)
	{
		printf("Invalid COM port %u (must be 1-4)\n", _com);
		return 1;
	}
/* get COM port I/O address from BIOS data segment */
	_io_adr = peek40(0 + (_com - 1) * 2);
	if(_io_adr == 0)
	{
		printf("Sorry, no COM%u serial port on this PC\n", _com);
		return 2;
	}
	printf("activating mouse on COM%u...\n", _com);
	_irq = com_to_irq[_com - 1];
	vect_num = _irq_to_vector[_irq];
/* install handler */
	SAVE_VECT(vect_num, old_vector);
	INSTALL_HANDLER(vect_num, com_irq);
/* set up serial chip
turn off handshaking lines to power-down mouse, then delay */
	outportb(_io_adr + 4, 0);
	delay(500);
	outportb(_io_adr + 3, 0x80);	/* set DLAB to access baud divisor */
	outportb(_io_adr + 1, 0);	/* 1200 baud */
	outportb(_io_adr + 0, 96);
	outportb(_io_adr + 3, 0x02);	/* clear DLAB bit, set 7N1 format */
	outportb(_io_adr + 2, 0);	/* turn off FIFO, if any */
/* activate Out2, RTS, and DTR to power the mouse */
	outportb(_io_adr + 4, 0x0B);
/* enable receiver interrupts */
	outportb(_io_adr + 1, 0x01);
/* enable interrupts at 8259s */
	enable_irq_at_8259(_irq);
/* wait a moment to get ID bytes from mouse. Microsoft mice just return
a single 'M' byte, some 3-button mice return "M3", my Logitech mouse
returns a string of bytes. I could find no documentation on these,
but I figured out some of it on my own. */
	ptr = id;
	for(temp = 750; temp != 0; temp--)
	{
		while(deq(&_queue, &byte) == 0)
		{
			printf("%02X ", byte);
			*ptr = byte;
			ptr++;
			if(ptr >= id + MAX_ID)
				goto FOO;
		}
		delay(1);
	}
	printf("\n");
FOO:
	if(ptr >= id + 11)
	{
/* find the 'M' */
		for(ptr = id; ptr < id + MAX_ID - 7; ptr++)
		{
			if(*ptr == 'M')
				break;
		}
		if(ptr < id + MAX_ID)
		{
/* four bytes that each encode 4 bits of the numeric portion
of the PnP ID. Each byte has b4 set (i.e. ORed with 0x10) */
			temp = (ptr[8] & 0x0F);
			temp <<= 4;
			temp |= (ptr[9] & 0x0F);
			temp <<= 4;
			temp |= (ptr[10] & 0x0F);
			temp <<= 4;
			temp |= (ptr[11] & 0x0F);
/* three bytes that each encode one character of the character portion
of the PnP ID. Each byte has b5 set (i.e. ORed with 0x20) */
			printf("mouse PnP ID: %c%c%c%04X\n",
				'@' + (ptr[5] & 0x1F),
				'@' + (ptr[6] & 0x1F),
				'@' + (ptr[7] & 0x1F), temp);
		}
/* example: Logitech serial mouse LGI8001
('L' - '@') | 0x20 == 0x2C
('G' - '@') | 0x20 == 0x27
('I' - '@') | 0x20 == 0x29
('8' - '0') | 0x10 == 0x18
('0' - '0') | 0x10 == 0x10
('0' - '0') | 0x10 == 0x10
('1' - '0') | 0x10 == 0x11 */
	}
	printf("Press a key to quit\n");
	while(!kbhit())
	{
		if(deq(&_queue, &byte) == 0)
			printf("%02X ", byte);
	}
	if(getch() == 0)
		(void)getch();
/* disable interrupts at serial chip */
	outportb(_io_adr + 1, 0);
/* disable interrupts at 8259s */
	disable_irq_at_8259(_irq);
/* restore old vector */
	RESTORE_VECT(vect_num, old_vector);
	return 0;
}
