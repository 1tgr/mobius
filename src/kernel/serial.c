#include <kernel/kernel.h>
#include <kernel/serial.h>

#define PIC 		PORT_8259M
#define PICM		(PORT_8259M + 1)

/*
;; ***********************************************************
;; ISR Service Jump Table
;; ***********************************************************
ISR_Service 	dd	offset	ISR_Exit		;; xxxxx00xb
				dd	offset	ISR_Transmit	;; xxxxx01xb
				dd	offset	ISR_Receive 	;; xxxxx10xb
				dd	offset	ISR_Exit		;; xxxxx11xb
*/

/*
;; ***********************************************************
;; Register Indexes for the 8250 UART
;; NOTE:	Indexes have been multiplied by 2 to get the
;;			correct location in the register table.
;; ***********************************************************
*/
#define THR				0				// Transmit Holding Register	(DLAB==0)
#define RBR				0				// Recieve Buffer Register		(DLAB==0)
#define BAUDL			0				// Baud Rate Divisor, Low byte	(DLAB==1)
#define BAUDH			1				// Baud Rate Divisor, High Byte (DLAB==1)
#define IER				1				// Interrupt Enable Register
#define IIR				2				// Interrupt Identification Register
#define FCR 			2				// FIFO Control Register (UARTS 16550+)
#define LCR				3				// Line Control Register
#define MCR				4				// Modem Control Register
#define LSR				5				// Line Status Register
#define MSR				6				// Modem Status Register

/*
;; ***********************************************************
;; COMM Routine Variables
;; ***********************************************************
*/

	/*
	;; ***********************************************************
	;; Multi-Port Asynchronous Variables (later version)
	;; ***********************************************************
	*/

typedef struct serial_t serial_t;
struct serial_t
{
	word Base;				// Port Base
	byte IRQ;				// Port IRQ
	byte Flags;				// Port Bit-Flags
};

#define UART	0x3		// UART Flags
#define MODE	0xc		// Port Mode

serial_t PortTable; 		// Single Port Routines
dword PortCode;			// Port Used Bit-Field (1-used)

/*
;; ***********************************************************
;; Variables set by ioOpenPort()
;; ***********************************************************
*/

word PortInUse = 0xff;		// Initialized COMM port flag
word Port;					// Port Base Address
word Registers[7];			// Register Index Map
byte Vector;				// Vector Number
byte EnableIRQ;				// Mask to enable 8259 IRQ
byte DisableIRQ;			// Mask to disable 8259 IRQ
word ISR_OldS;				// Old Vector Segment
dword ISR_OldO;				// Old Vector Offset

/*
;; ***********************************************************
;; Send and Receive Buffers
;; NOTE:	The buffer sizes (SendSize & RecvSize) must be of a
;;			size X such that log2(X+1)=Integer.  In otherwords,
;;			take a power of 2, subtract one, and that is a legal
;;			buffer size. (1, 3, 7, 15, 31, 63, 127 ... 2^X-1)
;; ***********************************************************
*/

/*#define SendSize		4095	// 2^X-1 Send Buffer Size (4095)
byte SendBuffer[SendSize];		// Send FIFO Buffer
word SendHead;					// Send FIFO Buffer Head Index
word SendTail;					// Send FIFO Buffer Tail Index

#define RecvSize		4095	// 2^X-1 Receive Buffer Size (4095)
byte RecvBuffer[RecvSize];		// Receive FIFO Buffer
word RecvHead;					// Receive FIFO Buffer Head Index
word RecvTail;					// Receive FIFO Buffer Tail Index

bool in_serial_isr;*/

//;; ************************************************************************

//;; **  C O D E	3 2  ******************************************************

/*
;; ***********************************************************
;; Routines callable from Watcom C/C++ 32
;; ***********************************************************
PUBLIC	ioOpenPort_, ioClosePort_				;; Set/Reset Serial I/O
PUBLIC	ioClearWrite_, ioClearRead_ 			;; Reset Serial Buffers
PUBLIC	ioReadByte_, ioWriteByte_				;; I/O with Serial Port
PUBLIC	ioReadStatus_, ioWriteStatus_			;; Check I/O Status
PUBLIC	ioGetBaud_, ioSetBaud_					;; Get/Set Baud Rate
PUBLIC	ioSetHandShake_, ioGetHandShake_		;; Get/Set Hand Shaking
PUBLIC	ioGetStatus_							;; Get Modem/Line Status
PUBLIC	ioGetControl_, ioSetControl_			;; Get/Set Line Control
PUBLIC	ioGetMode_, ioSetMode_					;; Get/Set ISR Mode
*/

int ioGetMode(COMM address)
{
	return 0;
}

int ioSetMode(COMM address, int mode)
{
	return 0;
}

/*
;; ************************************************************************
;; Interrupt Service Routine for Serial I/O (in protected mode)
;; ************************************************************************
*/

/*
void serial_isr(dword context, int irq)
{
	byte intr, state;

	in_serial_isr = true;
	out(0x21, in(0x21) | ~0xef);

	//do
	//{
		intr = (in(Registers[IIR]) & 6) >> 1;
		wprintf(L"serial_isr: %x\n", intr);

		switch (intr)
		{
		case 0:	// nothing
		case 3:
			state = in(Registers[IER]);
			//wprintf(L"state = %x ", state);
			out(Registers[IER], 0);
			//_cputws(L"out(0) ");
			out(Registers[IER], state);
			//_cputws(L"out(state)\n");
			goto finished;

		case 1:	// transmit
			if (SendHead == SendTail)
			{
				out(Registers[IER], in(Registers[IER]) & 0xFD);
				goto finished;
			}
			else
			{
				wprintf(L"transmit: %c\n", SendBuffer[SendTail]);
				out(Registers[THR], SendBuffer[SendTail]);
				SendTail = (SendTail + 1) & SendSize;
			}

			break;

		case 2:	// receive
			RecvBuffer[RecvHead] = in(Registers[RBR]);
			if (((RecvHead + 1) & RecvSize) != RecvTail)
				RecvHead = (RecvHead + 1) & RecvSize;
			break;
		}
	//} while ((in(Registers[IIR]) & 1) == 0);

finished:

	_cputws(L"finish: ");
	out(Registers[MCR], in(Registers[MCR]) | 8);
	out(Registers[IER], 1);

	out(0x21, in(0x21) & 0xef);
	_cputws(L"done\n");
	in_serial_isr = false;
}
*/

/*
;;	************************************************************************
;;	int 		ioOpenPort(int BASE, int IRQ);
;;	PURPOSE:	Initializes COMM port and sets up the ISR for it.
;;	PASS:		BASE = EDX, IRQ = ECX
;;		Defaults are:
;;		COM1 = 03F8h 4	(INT 0Ch)
;;		COM2 = 02F8h 3	(INT 0Bh)
;;		COM3 = 03E8h 4	(INT 0Ch)
;;		COM4 = 02E8h 3	(INT 0Bh)
;;	RETURNS:	Returns a Port ID Structure Address for use when these
;;				routines are multi-port based.
;;				0 Failure | > 0 Success
;;	************************************************************************
*/
COMM ioOpenPort(int base, int irq)
{
	int i;

	/*
	;; ***********************************************************
	;; Check to see if COM Routines installed
	;; ***********************************************************
	*/
	if (!PortInUse)
		return 0;

	/*
	;; ***********************************************************
	;; Verify that all passed info is good
	;; ***********************************************************
	*/

	if (irq < 0 || irq >= 8)
		return 0;

	/*
	;; ***********************************************************
	;; Set up COMM variables
	;; ***********************************************************
	*/
	PortInUse = true;
	Port = base;
	//Vector = irq + 8;
	//DisableIRQ = 1 << irq;
	//EnableIRQ = ~DisableIRQ;
	
	/*
	;; ***********************************************************
	;; Set up the Register Port Indexes
	;; ***********************************************************
	*/
	for (i = 6; i > 0; i--)
		Registers[i] = base + i;

	/*
	;; ***********************************************************
	;; Initialize the Read/Write Buffers
	;; ***********************************************************
	*/
	
	ioClearWrite(NULL);
	ioClearRead(NULL);
	
	/*
	;; ***********************************************************
	;; Install the ISR
	;; ***********************************************************
	*/

	//sysRegisterIrq(irq, serial_isr, 0);

	//out(Registers[MCR], in(Registers[MCR]) | 8);
	//out(Registers[LCR], in(Registers[LCR]) & 0x7F);
	//out(Registers[IER], 1);
	
	/*
	;; ***********************************************************
	;; Clear the buffers again, just in case . . .
	;; ***********************************************************
	*/

	//ioClearWrite(NULL);
	//ioClearRead(NULL);
	
	/*
	;; ***********************************************************
	;; Exit with grace . . .
	;; ***********************************************************
	*/
	PortCode = (dword) &PortTable;
	return (COMM) &PortTable;
}

/*
;;	************************************************************************
;;	int 		ioClosePort(COMM Address);
;;	PURPOSE:	Shuts off the COMM port and resets the ISR for it.
;;	PASS:		Address = EAX  Address of the PortID structure
;;	RETURNS:	-1 Failure | 0 Success
;;
;;	NOTE:	In this release, the passed address does nothing (no multi port).
;;	************************************************************************
*/

int ioClosePort(COMM address)
{
	/*
	;; ***********************************************************
	;; First, make sure there is a COMM port initialized
	;; ***********************************************************
	*/

	if (!PortInUse)
		return -1;

	/*
	;; ***********************************************************
	;; Shut off the 8259 IRQ
	;; ***********************************************************
@@:	in		al,PICM 			;; Fetch PIC Mask
	or		al,DisableIRQ		;; : Toggle OFF the ISR's IRQ line
	out		PICM,al 			;; :
	*/

	/*
	;; ***********************************************************
	;; Disable the 8250 Interrupt (DLAB clear perhaps before this)
	;; ***********************************************************
	*/

	out(Registers[IER], 0);
	out(Registers[MCR], in(Registers[MCR]) & 0xF7);
	
	/*
	;; ***********************************************************
	;; ISR is disabled, so reset old ISR Vector
	;; ***********************************************************
	*/

	//sysRegisterIrq(Vector, NULL, 0);
	
	PortInUse = false;
	
	return 0;
}

/*
;;	************************************************************************
;;	void		ioClearRead(COMM Address);
;;	PASS:		EAX = Address Address of the PortID Structure
;;	PURPOSE:	Clears the Read buffer
;;
;;	NOTE:	In this release, the passed address does nothing (no multi port).
;;	************************************************************************
*/

void ioClearRead(COMM address)
{
	//disable();
	//RecvHead = RecvTail = 0;
	//enable();
}

/*
;;	************************************************************************
;;	void		ioClearWrite(COMM Address);
;;	PASS:		EAX = Address Address of the PortID Structure
;;	PURPOSE:	Clears the Write buffer
;;
;;	NOTE:	In this release, the passed address does nothing (no multi port).
;;	************************************************************************
*/

void ioClearWrite(COMM address)
{
	//disable();
	//SendHead = SendTail = 0;
	//enable();
}

/*
;;	************************************************************************
;;	char		ioReadByte(COMM Address);
;;	PURPOSE:	Fetches a byte from the Read buffer.
;;	PASS:		EAX = Address Address of PortID Structure
;;	RETURNS:	duh....  If buffer is empty, then it returns NUL, else ...
;;
;;	NOTE:	Blah!!!!
;;	************************************************************************
*/

char ioReadByte(COMM address)
{
	//byte data;

	//if (RecvHead == RecvTail)
		//return 0;

	//data = RecvBuffer[RecvTail];
	//RecvTail = (RecvTail + 1) & RecvSize;
	//return data;

	return in(Registers[RBR]);
}

/*
;;	************************************************************************
;;	char		int ioWriteByte(COMM Address, char byte);
;;	PURPOSE:	Places a byte into the Send buffer.
;;	PASS:		Address = EAX, BYTE = ECX
;;	RETURNS:	!0 = Success | 0 == Failure (buffer full)
;;
;;	NOTE:	Same old thang . . . get the next version for multi-port.
;;	************************************************************************
*/

int ioWriteByte(COMM address, char data)
{
	/*
	;; ***********************************************************
	;; Check if buffer is FULL
	;; ***********************************************************
	*/

	//if (((SendHead + 1) & SendSize) == SendTail ||
		//in_serial_isr)
		//return 0;

	//SendBuffer[SendHead] = data;
	//SendHead = (SendHead + 1) & SendSize;

	/*
	;; ***********************************************************
	;; Enable the THRE Interrupt
	;; ***********************************************************
	*/

	//out(Registers[IER], in(Registers[IER]) | 2);
	
	out(Registers[THR], data);
	return -1;
}

/*
;;	************************************************************************
;;	int 		ioReadStatus(COMM Address);
;;	PURPOSE:	Reports the number of bytes in the Receive Buffer
;;	PASS:		You know the number by now . . .
;;	RETURNS:	Number of bytes in buffer
;;	************************************************************************
*/

int ioReadStatus(COMM address)
{
	/*if (RecvHead >= RecvTail)
		return RecvHead - RecvTail;
	else
		return RecvHead - RecvTail + RecvSize;*/
	return 0;
}

/*
;;	************************************************************************
;;	int 		ioWriteStatus();
;;	PURPOSE:	Reports the number of bytes in the Send Buffer
;;	PASS:		Blah . . .
;;	RETURNS:	Number of bytes in buffer
;;	************************************************************************
*/

int ioWriteStatus(COMM address)
{
	/*if (SendHead >= SendTail)
		return SendHead - SendTail;
	else
		return SendHead - SendTail + SendSize;*/
	return 0;
}

/*
;;	************************************************************************
;;	void		void ioSetBaud(COMM Address, int BAUD);
;;	PURPOSE:	Set the BAUD rate for the initialized port.
;;	PASS:		Address = EAX, BAUD = EBX
;;	RATES:		150, 300, 600, 1200, 2400, 4800, 9600,
;;				19200, 28800, 38400, 57600, 115200
;;	************************************************************************
*/

void ioSetBaud(COMM address, int baud)
{
	byte divisor;

	if (baud == 0)
		return;

	divisor = 115200 / baud;
	
	//disable();
	out(Registers[LCR], in(Registers[LCR]) | 0x80);

	out(Registers[BAUDL], divisor % 256);
	out(Registers[BAUDH], divisor / 256);
	
	out(Registers[LCR], in(Registers[LCR]) & 0x7F);
	//enable();
}

/*
;;	************************************************************************
;;	int 		ioGetBaud(COMM Address);
;;	PURPOSE:	Get the BAUD rate from the initialized port.
;;	PASS:		Address = EAX
;;	RETURNS:	BAUD Rate = EAX
;;	************************************************************************
*/

int ioGetBaud(COMM address)
{
	word divisor;

	//disable();
	out(Registers[LCR], in(Registers[LCR]) | 0x80);
	divisor = in(Registers[BAUDL]) | in(Registers[BAUDH]) << 8;
	out(Registers[LCR], in(Registers[LCR]) & 0x7F);
	//enable();

	if (divisor == 0)
		return 0;
	
	return 115200 / divisor;
}

/*
;;	************************************************************************
;;	void		ioSetHandShake(COMM Address, int HAND);
;;	PURPOSE:	Set various handshaking lines.
;;	PASS:		Address = ECX, HAND = EAX
;;	Bit fields are:
;;	(80h)	(40h)	(20h)	(10h)	(08h)	(04h)	(02h)	(01h)
;;	Bit 7	Bit 6	Bit 5	Bit 4	Bit 3	Bit 2	Bit 1	Bit 0
;;	0		0		0		Loop	OUT2	OUT1	RTS 	DTR
;;	************************************************************************
*/

void ioSetHandShake(COMM address, int hand)
{
	out(Registers[MCR], (byte) hand | 8);
}

/*
;;	************************************************************************
;;	int 		ioGetHandShake(COMM Address);
;;	PURPOSE:	Get current handshaking status.
;;	PASS:		...
;;	RETURNS:	Handshaking bitfield (See SetHandShake() for field info)
;;	************************************************************************
*/

int ioGetHandShake(COMM address)
{
	return in(Registers[MCR]);
}

/*
;;	************************************************************************
;;	int 		ioGetStatus(COMM Address);
;;	PURPOSE:	Fetches Modem/Line Status Register
;;	RETURNS:	Upper 8-bits==MSR & Lower 8-bits==LSR
;;	************************************************************************
*/

int ioGetStatus(COMM address)
{
	return in(Registers[MSR]) << 8 | in(Registers[LSR]);
}

/*
;;	************************************************************************
;;	int 		GetControl(COMM Address);
;;	PURPOSE:	Fetches Line Control Register
;;	PASS:		Blah blah blah.  Give me money and I'll let u pass sometin.
;;	RETURNS:	Lower 8-bits==LCR
;;	************************************************************************
*/

int ioGetControl(COMM address)
{
	return in(Registers[LCR]) & 0xf;
}

/*
;;	************************************************************************
;;	void		ioSetControl(COMM Address, int Control);
;;	PURPOSE:	Sets the Line Control Register
;;	PASS:		ECX = Address, Control = Lower 8-bits==LCR = EAX
;;	************************************************************************
*/

void ioSetControl(COMM address, int control)
{
	out(Registers[LCR], (byte) control & 0x7F);
}

int ioWriteBuffer(COMM address, const void* buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		if (ioWriteByte(address, ((const byte*) buf)[i]) == 0)
			return 0;

	return -1;
}
