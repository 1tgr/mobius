#include <os/stream.h>
#include <stdio.h>
#include <os/serial.h>
#include <string.h>
#include <kernel/driver.h>
#include <kernel/sys.h>

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
;; Send and Receive Buffers
;; NOTE:	The buffer sizes (SendSize & RecvSize) must be of a
;;			size X such that log2(X+1)=Integer.  In otherwords,
;;			take a power of 2, subtract one, and that is a legal
;;			buffer size. (1, 3, 7, 15, 31, 63, 127 ... 2^X-1)
;; ***********************************************************
*/

#define SendSize		4095	// 2^X-1 Send Buffer Size (4095)
#define RecvSize		4095	// 2^X-1 Receive Buffer Size (4095)

class CSerialPort : public IUnknown, public IDevice, public IStream
{
public:
	CSerialPort(word base, byte irq);
	virtual ~CSerialPort();

	/* IUnknown methods */
	STDMETHOD(QueryInterface)(REFIID iid, void** obj);
	IMPLEMENT_IUNKNOWN(CSerialPort);

	/* IDevice methods */
	STDMETHOD(GetInfo)(device_t* buf);
	STDMETHOD(DeviceOpen)();

	/* IStream methods */
	STDMETHOD_(size_t, Read)(void* buffer, size_t length);
	STDMETHOD_(size_t, Write)(const void* buffer, size_t length);
	STDMETHOD(SetIoMode)(dword mode);
	STDMETHOD(IsReady)();
	STDMETHOD(Stat)(folderitem_t* buf);
	STDMETHOD(Seek)(long offset, int origin);

protected:
	word m_port;			// Port Base Address
	word m_registers[7];	// Register Index Map
	byte m_irq;				// Vector Number
	bool m_in_isr;

	byte m_send_buffer[SendSize];		// Send FIFO Buffer
	word m_send_head;					// Send FIFO Buffer Head Index
	word m_send_tail;					// Send FIFO Buffer Tail Index

	byte m_receive_buffer[RecvSize];		// Receive FIFO Buffer
	word m_receive_head;					// Receive FIFO Buffer Head Index
	word m_receive_tail;					// Receive FIFO Buffer Tail Index

	static void Isr(void* context, int irq);
	void Interrupt();

	void ClosePort();
	void ClearRead();
	void ClearWrite();
	char HwRead();
	int HwWrite(char data);
	int ReadStatus();
	int WriteStatus();
	void SetBaud(int baud);
	int GetBaud();
	void SetHandShake(int hand);
	int GetHandShake();
	int GetStatus();
	int GetControl();
	void SetControl(int control);
};

extern "C" IDevice* Serial_Create(word port, byte irq)
{
	return new CSerialPort(port, irq);
}

void CSerialPort::Isr(void* context, int irq)
{
	_cputws(L"Serial IRQ\n");
	((CSerialPort*) context)->Interrupt();
}

void CSerialPort::Interrupt()
{
	byte intr, state;

	m_in_isr = true;
	out(0x21, in(0x21) | ~0xef);

	do
	{
		intr = (in(m_registers[IIR]) & 6) >> 1;
		wprintf(L"serial_isr: %x\n", intr);

		switch (intr)
		{
		case 0:	// nothing
		case 3:
			state = in(m_registers[IER]);
			//wprintf(L"state = %x ", state);
			out(m_registers[IER], 0);
			//_cputws(L"out(0) ");
			out(m_registers[IER], state);
			//_cputws(L"out(state)\n");
			goto finished;

		case 1:	// transmit
			if (m_send_head == m_send_tail)
			{
				out(m_registers[IER], in(m_registers[IER]) & 0xFD);
				goto finished;
			}
			else
			{
				wprintf(L"transmit: %c\n", m_send_buffer[m_send_tail]);
				out(m_registers[THR], m_send_buffer[m_send_tail]);
				m_send_tail = (m_send_tail + 1) & SendSize;
			}

			break;

		case 2:	// receive
			m_receive_buffer[m_receive_head] = in(m_registers[RBR]);
			if (((m_receive_head + 1) & RecvSize) != m_receive_tail)
				m_receive_head = (m_receive_head + 1) & RecvSize;
			break;
		}
	} while ((in(m_registers[IIR]) & 1) == 0);

finished:

	_cputws(L"finish: ");
	out(m_registers[MCR], in(m_registers[MCR]) | 8);
	out(m_registers[IER], 1);

	out(0x21, in(0x21) & 0xef);
	_cputws(L"done\n");
	m_in_isr = false;
}

CSerialPort::CSerialPort(word base, byte irq)
{
	int i;

	m_refs = 0;

	wprintf(L"CSerialPort::CSerialPort(%x, %d)\n", base, irq);

	/*
	;; ***********************************************************
	;; Set up COMM variables
	;; ***********************************************************
	*/
	m_port = base;
	m_irq = irq;
	
	/*
	;; ***********************************************************
	;; Set up the Register Port Indexes
	;; ***********************************************************
	*/
	for (i = 6; i > 0; i--)
		m_registers[i] = base + i;

	/*
	;; ***********************************************************
	;; Initialize the Read/Write Buffers
	;; ***********************************************************
	*/
	
	ClearWrite();
	ClearRead();
	
	/*
	;; ***********************************************************
	;; Install the ISR
	;; ***********************************************************
	*/

	sysRegisterIrq(m_irq, (void (*) (dword, int)) Isr, (dword) this);

	out(m_registers[MCR], in(m_registers[MCR]) | 8);
	out(m_registers[LCR], in(m_registers[LCR]) & 0x7F);
	out(m_registers[IER], 1);
	
	/*
	;; ***********************************************************
	;; Clear the buffers again, just in case . . .
	;; ***********************************************************
	*/

	ClearWrite();
	ClearRead();
}

CSerialPort::~CSerialPort()
{
	ClosePort();
}

void CSerialPort::ClosePort()
{
	/*
	;; ***********************************************************
	;; Disable the 8250 Interrupt (DLAB clear perhaps before this)
	;; ***********************************************************
	*/

	out(m_registers[IER], 0);
	out(m_registers[MCR], in(m_registers[MCR]) & 0xF7);
	
	/*
	;; ***********************************************************
	;; ISR is disabled, so reset old ISR Vector
	;; ***********************************************************
	*/

	sysRegisterIrq(m_irq, NULL, 0);
}

void CSerialPort::ClearRead()
{
	disable();
	m_receive_head = m_receive_tail = 0;
	enable();
}

void CSerialPort::ClearWrite()
{
	disable();
	m_send_head = m_send_tail = 0;
	disable();
}

char CSerialPort::HwRead()
{
	byte data;

	if (m_receive_head == m_receive_tail)
		return 0;

	data = m_receive_buffer[m_receive_tail];
	m_receive_tail = (m_receive_tail + 1) & RecvSize;
	return data;
}

int CSerialPort::HwWrite(char data)
{
	/*
	;; ***********************************************************
	;; Check if buffer is FULL
	;; ***********************************************************
	*/

	if (((m_send_head + 1) & SendSize) == m_send_tail ||
		m_in_isr)
		return 0;

	m_send_buffer[m_send_head] = data;
	m_send_head = (m_send_head + 1) & SendSize;

	/*
	;; ***********************************************************
	;; Enable the THRE Interrupt
	;; ***********************************************************
	*/

	out(m_registers[IER], in(m_registers[IER]) | 2);
	
	return -1;
}

int CSerialPort::ReadStatus()
{
	if (m_receive_head >= m_receive_tail)
		return m_receive_head - m_receive_tail;
	else
		return m_receive_head - m_receive_tail + RecvSize;
}

int CSerialPort::WriteStatus()
{
	if (m_send_head >= m_send_tail)
		return m_send_head - m_send_tail;
	else
		return m_send_head - m_send_tail + SendSize;
}

void CSerialPort::SetBaud(int baud)
{
	byte divisor;

	if (baud == 0)
		return;

	divisor = 115200 / baud;
	
	disable();
	out(m_registers[LCR], in(m_registers[LCR]) | 0x80);

	out(m_registers[BAUDL], divisor % 256);
	out(m_registers[BAUDH], divisor / 256);
	
	out(m_registers[LCR], in(m_registers[LCR]) & 0x7F);
	enable();
}

int CSerialPort::GetBaud()
{
	word divisor;

	disable();
	out(m_registers[LCR], in(m_registers[LCR]) | 0x80);
	divisor = in(m_registers[BAUDL]) | in(m_registers[BAUDH]) << 8;
	out(m_registers[LCR], in(m_registers[LCR]) & 0x7F);
	enable();

	if (divisor == 0)
		return 0;
	
	return 115200 / divisor;
}

void CSerialPort::SetHandShake(int hand)
{
	out(m_registers[MCR], (byte) hand | 8);
}

int CSerialPort::GetHandShake()
{
	return in(m_registers[MCR]);
}

int CSerialPort::GetStatus()
{
	return in(m_registers[MSR]) << 8 | in(m_registers[LSR]);
}

int CSerialPort::GetControl()
{
	return in(m_registers[LCR]) & 0xf;
}

void CSerialPort::SetControl(int control)
{
	out(m_registers[LCR], (byte) control & 0x7F);
}

HRESULT CSerialPort::QueryInterface(REFIID iid, void** obj)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IStream))
	{
		AddRef();
		*obj = this;
		return S_OK;
	}
	else
		return E_FAIL;
}

HRESULT CSerialPort::GetInfo(device_t* buf)
{
	if (buf->size < sizeof(device_t))
		return E_FAIL;

	wcsncpy(buf->name, L"Serial port", buf->name_max);
	return S_OK;
}

HRESULT CSerialPort::DeviceOpen()
{
	_cputws(L"CSerialPort::DeviceOpen\n");
	ClearRead();
	ClearWrite();
	SetBaud(9600);
	SetControl(BITS_8 | NO_PARITY | STOP_1);
	SetHandShake(0);
	return S_OK;
}

size_t CSerialPort::Read(void* buffer, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++)
	{
		while (ReadStatus() == 0)
			;

		((byte*) buffer)[i] = HwRead();
	}

	return i;
}

size_t CSerialPort::Write(const void* buffer, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++)
		if (HwWrite(((const byte*) buffer)[i]) == 0)
			break;

	return i;
}

HRESULT CSerialPort::SetIoMode(dword mode)
{
	return S_OK;
}

HRESULT CSerialPort::IsReady()
{
	return S_OK;
}

HRESULT CSerialPort::Stat(folderitem_t* buf)
{
	return E_FAIL;
}

HRESULT CSerialPort::Seek(long offset, int origin)
{
	return E_FAIL;
}