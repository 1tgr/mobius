/*
** LL_COMM (R) Asynchronous Communication Routines
** Copyright (C) 1994 by James P. Ketrenos
**
** VERSION	:	1.2
** DATE 	:	7-4-94
** CODED BY :   James P. Ketrenos [ketrenoj@cs.pdx.edu]
**      aka :   Lord Logics (for those who care)
**
** Special thanks to all of the contributors to the Serial.FAQ
**
*
*	NOTE:  As of this release, these routines only allow for ONE port to be
*	opened at a time.  Hopefully, if all goes well, the next version will
*	allow for up to 32 different ports to be opened.  I have set up these
*	routines to be compatible with the future format, so using the next
*	version shouldn't require much, if any, change to working code.
*
*	Also, you probably have noticed that the assembly listings for this code
*	have not been included.  If you are interested in the assembly code,
*	please read the file LL_COMM.NFO, as it explains why it is not here, and
*	where/how you can acquire it.
*
**
*/

#ifndef _LL_COMM_H_
#define _LL_COMM_H_
#ifdef	__cplusplus
#define CEXT	extern "C"
#else
#define CEXT	extern
#endif

#include <sys/types.h>

typedef int 	COMM;
typedef char	*PACKET;

/* Initialization Routines	****/
CEXT	COMM	ioOpenPort(int Base, int IRQ);
CEXT	int 	ioClosePort(COMM Port);

/* Buffer Routines			****/
CEXT	void	ioClearWrite(COMM);
CEXT	void	ioClearRead(COMM);
CEXT	int 	ioReadStatus(COMM);
CEXT	int 	ioWriteStatus(COMM);

/* I/O Routines 			****/
CEXT	char	ioReadByte(COMM);
CEXT    int     ioWriteByte(COMM, char);
CEXT	int		ioWriteBuffer(COMM address, const void* buf, size_t len);
CEXT	int 	ioReadPacket(COMM, PACKET);
CEXT	int 	ioWritePacket(COMM, PACKET);

/* Mode Routines			****/
CEXT	int 	ioGetMode(COMM);
CEXT	int 	ioSetMode(COMM, int);

/* Port Setup Routines		****/
CEXT	int 	ioGetBaud(COMM);
CEXT    void    ioSetBaud(COMM, int);
CEXT	int 	ioGetHandShake(COMM);
CEXT	void	ioSetHandShake(COMM, int);
CEXT	int 	ioGetStatus(COMM);
CEXT	int 	ioGetControl(COMM);
CEXT	void	ioSetControl(COMM, int);
CEXT	int 	ioGetFIFO(COMM);
CEXT	int 	ioSetFIFO(COMM, int);

#include <os/serial.h>

#endif
