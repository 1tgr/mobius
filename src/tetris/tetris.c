#include <stdlib.h> /* random() */
#include <stdio.h> /* printf() */
#include <conio.h> /* KEY_nnn, getch() */
#include <os/keyboard.h>
#include <os/os.h>
#include <os/device.h>

/*!
 *	\ingroup	programs
 *	\defgroup	tetris	Tetris
 *	@{
 */

/* dimensions of playing area */
#define	SCN_WID		15
#define	SCN_HT		20
/* direction vectors */
#define	DIR_UP		{ 0, -1 }
#define	DIR_DN		{ 0, +1 }
#define	DIR_LT		{ -1, 0 }
#define	DIR_RT		{ +1, 0 }
#define	DIR_UP2		{ 0, -2 }
#define	DIR_DN2		{ 0, +2 }
#define	DIR_LT2		{ -2, 0 }
#define	DIR_RT2		{ +2, 0 }
/* ANSI colors */
#define	COLOR_BLACK	0
#define	COLOR_RED	1
#define	COLOR_GREEN	2
#define	COLOR_YELLOW	3
#define	COLOR_BLUE	4
#define	COLOR_MAGENTA	5
#define	COLOR_CYAN	6
#define	COLOR_WHITE	7

typedef struct
{	short int DeltaX, DeltaY; } vector;

typedef struct
{	char Plus90, Minus90;	/* pointer to shape rotated +/- 90 degrees */
	char Color;		/* shape color */
	vector Dir[4]; } shape;	/* drawing instructions for this shape */

shape Shapes[]=
/* shape #0:			cube */
{	{ 0, 0, COLOR_BLUE, { DIR_UP, DIR_RT, DIR_DN, DIR_LT }},
/* shapes #1 & #2:		bar */
	{ 2, 2, COLOR_GREEN, { DIR_LT, DIR_RT, DIR_RT, DIR_RT }},
	{ 1, 1, COLOR_GREEN, { DIR_UP, DIR_DN, DIR_DN, DIR_DN }},
/* shapes #3 & #4:		'Z' shape */
	{ 4, 4, COLOR_CYAN, { DIR_LT, DIR_RT, DIR_DN, DIR_RT }},
	{ 3, 3, COLOR_CYAN, { DIR_UP, DIR_DN, DIR_LT, DIR_DN }},
/* shapes #5 & #6:		'S' shape */
	{ 6, 6, COLOR_RED, { DIR_RT, DIR_LT, DIR_DN, DIR_LT }},
	{ 5, 5, COLOR_RED, { DIR_UP, DIR_DN, DIR_RT, DIR_DN }},
/* shapes #7, #8, #9, #10:	'J' shape */
	{ 8, 10, COLOR_MAGENTA, { DIR_RT, DIR_LT, DIR_LT, DIR_UP }},
	{ 9, 7, COLOR_MAGENTA, { DIR_UP, DIR_DN, DIR_DN, DIR_LT }},
	{ 10, 8, COLOR_MAGENTA, { DIR_LT, DIR_RT, DIR_RT, DIR_DN }},
	{ 7, 9, COLOR_MAGENTA, { DIR_DN, DIR_UP, DIR_UP, DIR_RT }},
/* shapes #11, #12, #13, #14:	'L' shape */
	{ 12, 14, COLOR_YELLOW, { DIR_RT, DIR_LT, DIR_LT, DIR_DN }},
	{ 13, 11, COLOR_YELLOW, { DIR_UP, DIR_DN, DIR_DN, DIR_RT }},
	{ 14, 12, COLOR_YELLOW, { DIR_LT, DIR_RT, DIR_RT, DIR_UP }},
	{ 11, 13, COLOR_YELLOW, { DIR_DN, DIR_UP, DIR_UP, DIR_LT }},
/* shapes #15, #16, #17, #18:	'T' shape */
	{ 16, 18, COLOR_WHITE, { DIR_UP, DIR_DN, DIR_LT, DIR_RT2 }},
	{ 17, 15, COLOR_WHITE, { DIR_LT, DIR_RT, DIR_UP, DIR_DN2 }},
	{ 18, 16, COLOR_WHITE, { DIR_DN, DIR_UP, DIR_RT, DIR_LT2 }},
	{ 15, 17, COLOR_WHITE, { DIR_RT, DIR_LT, DIR_DN, DIR_UP2 }}};

char Dirty[SCN_HT], Screen[SCN_WID][SCN_HT];
//////////////////////////////////////////////////////////////////////////////
//			ANSI GRAPHIC OUTPUT
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
	name:	refresh
	action:	updates display device based on contents of global
		char array Screen[][]. Updates only those rows
		marked Dirty[]
*****************************************************************************/
void refresh(void)
{
	unsigned XPos, YPos;
	qword fpos;
	word ch[2];
	wchar_t s[8] = L"\x1B[%dm  ";
	s[5] = s[6] = 0x2588;

	for(YPos=0; YPos < SCN_HT; YPos++)
	{
		if(!Dirty[YPos])
			continue;
/* gotoxy(0, YPos) */
		wprintf(L"\x1B[%d;1H", YPos + 1);
		fpos = YPos * 80;
		for(XPos=0; XPos < SCN_WID; XPos++)
		{
/* 0xDB is a solid rectangular block in the PC character set */
			//wprintf(L"\x1B[%dm\xDB\xDB", 30 + Screen[XPos][YPos]);

			/* U+2588 is the Unicode Full Block character */
			// xxx - gcc seems to break this
			wprintf(L"\x1B[%dm\x2588\x2588", 30 + Screen[XPos][YPos]);

			//wprintf(s, 30 + Screen[XPos][YPos]);

			//ch[0] = ch[1] = (Screen[XPos][YPos] << 8) | 0xDB;
			//devWriteSync(vga, (fpos + XPos * 2) * 2, ch, sizeof(ch));
		}
		Dirty[YPos]=0;
	}
/* reset foreground color to gray */
	wprintf(L"\x1B[37m");
	//fflush(stdout);
}
//////////////////////////////////////////////////////////////////////////////
//			GRAPHIC CHUNK DRAW & HIT DETECT
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
	name:	blockDraw
	action:	draws one graphic block in display buffer at
		position (XPos, YPos)
*****************************************************************************/
void blockDraw(unsigned XPos, unsigned YPos, char Color)
{	if(XPos >= SCN_WID)
		XPos=SCN_WID - 1;
	if(YPos >= SCN_HT)
		YPos=SCN_HT - 1;
	Color &= 7;

	Screen[XPos][YPos]=Color;
	Dirty[YPos]=1; }	/* this row has been modified */
/*****************************************************************************
	name:	blockHit
	action:	determines if coordinates (XPos, YPos) are already
		occupied by a graphic block
	returns:color of graphic block at (XPos, YPos) (zero if black/
		empty)
*****************************************************************************/
char blockHit(unsigned XPos, unsigned YPos)
{	return(Screen[XPos][YPos]); }
//////////////////////////////////////////////////////////////////////////////
//			SHAPE DRAW & HIT DETECT
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
	name:	shapeDraw
	action:	draws shape WhichShape in display buffer at
		position (XPos, YPos)
*****************************************************************************/
void shapeDraw(unsigned XPos, unsigned YPos, unsigned WhichShape)
{	unsigned Index;

	for(Index=0; Index < 4; Index++)
	{	blockDraw(XPos, YPos, Shapes[WhichShape].Color);
		XPos += Shapes[WhichShape].Dir[Index].DeltaX;
		YPos += Shapes[WhichShape].Dir[Index].DeltaY; }
	blockDraw(XPos, YPos, Shapes[WhichShape].Color); }
/*****************************************************************************
	name:	shapeErase
	action:	erases shape WhichShape in display buffer at
		position (XPos, YPos) by setting its color to zero
*****************************************************************************/
void shapeErase(unsigned XPos, unsigned YPos, unsigned WhichShape)
{	unsigned Index;

	for(Index=0; Index < 4; Index++)
	{	blockDraw(XPos, YPos, COLOR_BLACK);
		XPos += Shapes[WhichShape].Dir[Index].DeltaX;
		YPos += Shapes[WhichShape].Dir[Index].DeltaY; }
	blockDraw(XPos, YPos, COLOR_BLACK); }
/*****************************************************************************
	name:	shapeHit
	action:	determines if shape WhichShape would collide with
		something already drawn in display buffer if it
		were drawn at position (XPos, YPos)
	returns:nonzero if hit, zero if nothing there
*****************************************************************************/
char shapeHit(unsigned XPos, unsigned YPos, unsigned WhichShape)
{	unsigned Index;

	for(Index=0; Index < 4; Index++)
	{	if(blockHit(XPos, YPos))
			return(1);
		XPos += Shapes[WhichShape].Dir[Index].DeltaX;
		YPos += Shapes[WhichShape].Dir[Index].DeltaY; }
	if(blockHit(XPos, YPos))
		return(1);
	return(0); }
//////////////////////////////////////////////////////////////////////////////
//			MAIN ROUTINES
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
	name:	screenInit
	action:	clears display buffer, marks all rows dirty,
		set raw keyboard mode
*****************************************************************************/
void screenInit(void)
{	unsigned XPos, YPos;

	for(YPos=0; YPos < SCN_HT; YPos++)
	{	Dirty[YPos]=1;	/* force entire screen to be redrawn */
		for(XPos=1; XPos < (SCN_WID - 1); XPos++)
			Screen[XPos][YPos]=0;
		Screen[0][YPos]=Screen[SCN_WID - 1][YPos]=COLOR_BLUE; }
	for(XPos=0; XPos < SCN_WID; XPos++)
		Screen[XPos][0]=Screen[XPos][SCN_HT - 1]=COLOR_BLUE; }
/*****************************************************************************
*****************************************************************************/
void collapse(void)
{	char SolidRow[SCN_HT], SolidRows;
	int Row, Col, Temp;

/* determine which rows are solidly filled */
	SolidRows=0;
	for(Row=1; Row < SCN_HT - 1; Row++)
	{	Temp=0;
		for(Col=1; Col < SCN_WID - 1; Col++)
			if(Screen[Col][Row])
				Temp++;
		if(Temp == SCN_WID - 2)
		{	SolidRow[Row]=1;
			SolidRows++; }
		else SolidRow[Row]=0; }
	if(SolidRows == 0)
		return;
/* collapse them */
	for(Temp=Row=SCN_HT - 2; Row > 0; Row--, Temp--)
	{	while(SolidRow[Temp])
			Temp--;
		if(Temp < 1)
		{	for(Col=1; Col < SCN_WID - 1; Col++)
				Screen[Col][Row]=COLOR_BLACK; }
		else
		{	for(Col=1; Col < SCN_WID - 1; Col++)
				Screen[Col][Row]=Screen[Col][Temp]; }
		Dirty[Row]=1; }
	refresh(); }
/*****************************************************************************
*****************************************************************************/

wint_t getKey(void)
{
	dword end = sysUpTime() + 200;

	while (!_kbhit())
		if (sysUpTime() >= end)
			return 0;

	return _wgetch();
}

/*****************************************************************************
	name:	main
*****************************************************************************/
int main(void)
{
	char Fell, NewShape, NewX, NewY;
	unsigned Shape, X, Y, i;
	wint_t Key;

/* re-seed the random number generator */
	srand(sysUpTime());
/* turn off stdio.h buffering */
	//setbuf(stdout, NULL);
/* banner screen */
	wprintf(L"\x1B[2J"L"\x1B[1;%dH"L"TETRIS by Alexei Pazhitnov",
		SCN_WID * 2 + 2);
	wprintf(L"\x1B[2;%dH"L"Software by Chris Giese", SCN_WID * 2 + 2);
	wprintf(L"\x1B[4;%dH"L"'1' and '2' rotate shape", SCN_WID * 2 + 2);
	wprintf(L"\x1B[5;%dH"L"Arrow keys move shape", SCN_WID * 2 + 2);
	wprintf(L"\x1B[6;%dH"L"Esc or Q quits", SCN_WID * 2 + 2);

	/*wprintf(L"\x1B[2J"L"\x1B[1;%dH"L"TETRIS by Alexei Pazhitnov", 2);
	wprintf(L"\x1B[2;%dH"L"Software by Chris Giese", 2);
	wprintf(L"\x1B[4;%dH"L"'1' and '2' rotate shape", 2);
	wprintf(L"\x1B[5;%dH"L"Arrow keys move shape", 2);
	wprintf(L"\x1B[6;%dH"L"Esc or Q quits", 2);*/

NEW:
	wprintf(L"\x1B[9;%dH"L"Press any key to begin", SCN_WID * 2 + 2);
	//wprintf(L"\x1B[9;%dH"L"Press any key to begin", 2);
	//fflush(stdout);
	_wgetch();
	wprintf(L"\x1B[8;%dH"L"                      ", SCN_WID * 2 + 2);
	wprintf(L"\x1B[9;%dH"L"                      ", SCN_WID * 2 + 2);
	/*wprintf(L"\x1B[8;%dH"L"                      ", 2);
	wprintf(L"\x1B[9;%dH"L"                      ", 2);*/
	screenInit();
	goto FOO;

	while(1)
	{	Fell=0;
		NewShape=Shape;
		NewX=X;
		NewY=Y;
		Key=getKey();
		if(Key == 0)
		{	NewY++;
			Fell=1; }
		else
		{	if(Key == 'q' || Key == 'Q' || Key == 27)
				break;
				//goto FIN;
			if (Key == '1' || Key == KEY_UP)
				NewShape=Shapes[Shape].Plus90;
			else if(Key == '2')
				NewShape=Shapes[Shape].Minus90;
			else if(Key == KEY_LEFT)
			{
				if(X > 0)
					NewX=X - 1;
			}
			else if(Key == KEY_RIGHT)
			{
				if(X < SCN_WID - 1)
					NewX=X + 1;
			}
			else if(Key == KEY_DOWN)
			{
				if(Y < SCN_HT - 1)
					NewY=Y + 1;
			}

			Fell=0;
		}
/* if nothing has changed, skip the bottom half of this loop */
		if(NewX == X && NewY == Y && NewShape == Shape)
			continue;
/* otherwise, erase old shape from the old pos'n */
		shapeErase(X, Y, Shape);
/* hit anything? */
		if(shapeHit(NewX, NewY, NewShape) == 0)
/* no, update pos'n */
		{	X=NewX;
			Y=NewY;
			Shape=NewShape; }
/* yes -- did the piece hit something while falling on its own? */
		else if(Fell)
/* yes, draw it at the old pos'n... */
		{	shapeDraw(X, Y, Shape);
/* ... and spawn new shape */
FOO:			Y=3;
			X=SCN_WID / 2;
			Shape=rand() % 19;
			collapse();
/* if newly spawned shape hits something, game over */
			if(shapeHit(X, Y, Shape))
			{
				/*wprintf(L"\x1B[8;%dH"L"\x1B[37;40;1m"
					L"       GAME OVER"L"\x1B[0m",
					SCN_WID * 2 + 2);*/
				wprintf(L"\x1B[8;%dH"L"\x1B[37;40;1m"
					L"       GAME OVER"L"\x1B[0m",
					2);
				goto NEW; 
			}
		}
/* hit something because of user movement/rotate OR no hit: just redraw it */
		shapeDraw(X, Y, Shape);
		refresh();
	}
	
	wprintf(L"\x1B[2J");
	return 0;
}

//@}