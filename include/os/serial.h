#ifndef __OS_SERIAL_H
#define __OS_SERIAL_H

/*
** ISR I/O Modes
*/
/********************************........xx..	*/
#define BYTE_MODE		0x00	/* 00000000b	*/
#define PACKET_MODE 	0x01	/* 00000001b	*/
#define VARIABLE_MODE	0x02	/* 00000010b	*/

/*
** Line Control Register
*/
/********************************........xx..	*/
#define CLEAR_BITS		0xFC	/* 11111100b	*/
#define BITS_5			0x00	/* 00000000b	*/
#define BITS_6			0x01	/* 00000001b	*/
#define BITS_7			0x02	/* 00000010b	*/
#define BITS_8			0x03	/* 00000011b	*/
/********************************.......x...	*/
#define CLEAR_STOP		0xFB	/* 11111011b	*/
#define STOP_1			0x00	/* 00000000b	*/
#define STOP_2			0x04	/* 00000100b (Doesn't work w/ BITS_5) */
/*********************************...xxx....	*/
#define CLEAR_PARITY	0xE7	/* 11000111b	*/
#define NO_PARITY		0x00	/* 00000000b	*/
#define ODD_PARITY		0x08	/* 00001000b	*/
#define EVEN_PARITY 	0x18	/* 00011000b	*/
#define MARK_PARITY 	0x28	/* 00101000b	*/
#define SPACE_PARITY	0x38	/* 00111000b	*/
/********************************...x.......	*/
#define CLEAR_MODE		0xBF	/* 10111111b	*/
#define NORMAL			0x00	/* 00000000b	*/
#define BREAK			0x40	/* 01000000b	*/

/*
** HandShaking
*/
#define	DTR				0x01	// 00000001b
#define	RTS				0x02	// 00000010b
#define LOOPBACK        0x10    // 00010000b

/*
** Modem/Line Status Register
*/
#define	D_CTS			0x0100	// 00000001 00000000b
#define	D_DSR			0x0200	// 00000010 00000000b
#define	D_RI			0x0400	// 00000100 00000000b
#define	D_DCD			0x0800	// 00001000 00000000b
#define	CTS				0x1000	// 00010000 00000000b
#define	DSR				0x2000	// 00100000 00000000b
#define	RI				0x4000	// 01000000 00000000b
#define	DCD				0x8000	// 10000000 00000000b
#define RBF 			0x0001	// 00000000 00000001b
#define OVR_ERROR		0x0002	// 00000000 00000010b
#define	PAR_ERROR		0x0004	// 00000000 00000100b
#define FRM_ERROR		0x0008	// 00000000 00001000b
#define	BRK_ERROR		0x0010	// 00000000 00010000b
#define	THREMPTY		0x0020	// 00000000 00100000b
#define TEMPTY			0x0040	// 00000000 01000000b
#define FFO_ERROR		0x0080	// 00000000 10000000b

#endif