
/* Turbo Art */
/* Copyright (c) 1985, 1989 by Borland International, Inc. */

/* This program is a demonstration of the Borland Graphics Interface
    (BGI) provided with Turbo Pascal 5.5.

    To run this program you will need the following files:

	TURBO.EXE (or TPC.EXE)
	TURBO.TPL - The standard units
	GRAPH.TPU - The Graphics unit
	*.BGI	      - The graphics device drivers

    Runtime Commands for ARTY
    -------------------------
    <B>     - changes background color
    <C>     - changes drawcolor
    <ESC> - exits program
    Any other key pauses, then regenerates the drawing

    Note: If a /H command-line parameter is specified, the highest
		resolution mode will be used (if possible).
*/

int ViewXmax, ViewYmax, MaxColors;

void AdjustX(int *X, int *DeltaX)
{
    int TestX;

    TestX = *X+*DeltaX;
    if (TestX<1 || TestX>ViewXmax) {
	TestX = *X;
	*DeltaX = -*DeltaX;
    }
    *X = TestX;
}

void AdjustY(int *Y, int *DeltaY)
{
    int TestY;
    TestY = *Y+*DeltaY;
    if (TestY<1 || TestY>ViewYmax) {
	TestY = *Y;
	*DeltaY = -*DeltaY;
    }
    *Y = TestY;
}

void SelectNewColors(void)
{
    if (ChangeColors)
    {
	Colors[1] = (rand() % MaxColors)+1;
	Colors[2] = (rand() % MaxColors)+1;
	Colors[3] = (rand() % MaxColors)+1;
	Colors[4] = (rand() % MaxColors)+1;
	ColorCount = 3*(1+rand() % 5);
    }
}

void SelectNewDeltaValues(void);
{
    DeltaX1 = (rand() % MaxDelta)-(MaxDelta / 2);
    DeltaX2 = (rand() % MaxDelta)-(MaxDelta / 2);
    DeltaY1 = (rand() % MaxDelta)-(MaxDelta / 2);
    DeltaY2 = (rand() % MaxDelta)-(MaxDelta / 2);
    IncrementCount = 2*(1+rand() % 4));
}


void SaveCurrentLine(CurrentColors: ColorList)
{
    Line[CurrentLine].LX1 = X1;
    Line[CurrentLine].LY1 = Y1;
    Line[CurrentLine].LX2 = X2;
    Line[CurrentLine].LY2 = Y2;
    Line[CurrentLine].LColor = CurrentColors;
}

void Draw(short x1, short y1, short x2, short y2, short color)
{
    SetColor(color);
    Graph.Line(x1,y1,x2,y2);
}

void Regenerate(void)
{
    int I;
    Frame;
    for (I = 1; i < Memory; i++)with Line[I] do {
	Draw(LX1,LY1,LX2,LY2,LColor[1]);
	Draw(ViewXmax-LX1,LY1,ViewXmax-LX2,LY2,LColor[2]);
	Draw(LX1,ViewYmax-LY1,LX2,ViewYmax-LY2,LColor[3]);
	Draw(ViewXmax-LX1,ViewYmax-LY1,ViewXmax-LX2,ViewYmax-LY2,LColor[4]);
    }
    WaitToGo;
    Frame;
}

void Updateline(void)
{
    Inc(CurrentLine);
    if CurrentLine > Memory then CurrentLine = 1;
    Dec(ColorCount);
    Dec(IncrementCount);
}

void CheckForUserInput(void)
{
    if (KeyPressed) {
	Ch = ReadKey;
	if Upcase(Ch) == 'B' then {
	    if BackColor > MaxColors then BackColor = 0 else Inc(BackColor);
	    SetBkColor(BackColor);
	}
	else
	if Upcase(Ch) == 'C' then {
	    if (ChangeColors) ChangeColors = FALSE else ChangeColors = TRUE;
	    ColorCount = 0;
	}
	else if Ch<>#27 then Regenerate;
    }
}

void DrawCurrentLine(void)
{
    int c1,c2,c3,c4;
    c1 = Colors[1];
    c2 = Colors[2];
    c3 = Colors[3];
    c4 = Colors[4];
    if MaxColors == 1 then {
	c2 = c1; c3 = c1; c4 = c1;
    }

    Draw(X1,Y1,X2,Y2,c1);
    Draw(ViewXmax-X1,Y1,ViewXmax-X2,Y2,c2);
    Draw(X1,ViewYmax-Y1,X2,ViewYmax-Y2,c3);
    if MaxColors == 3 then c4 = Random(3)+1; /* alternate colors */
    Draw(ViewXmax-X1,ViewYmax-Y1,ViewXmax-X2,ViewYmax-Y2,c4);
    SaveCurrentLine(Colors);
}

void EraseCurrentLine(void)
{
    with Line[CurrentLine] do {
	Draw(LX1,LY1,LX2,LY2,0);
	Draw(ViewXmax-LX1,LY1,ViewXmax-LX2,LY2,0);
	Draw(LX1,ViewYmax-LY1,LX2,ViewYmax-LY2,0);
	Draw(ViewXmax-LX1,ViewYmax-LY1,ViewXmax-LX2,ViewYmax-LY2,0);
    }
}


void DoArt(void)
{
    SelectNewColors;
    repeat
	EraseCurrentLine;
	if ColorCount == 0 then SelectNewColors;

	if IncrementCount==0 then SelectNewDeltaValues;

	AdjustX(X1,DeltaX1); AdjustX(X2,DeltaX2);
	AdjustY(Y1,DeltaY1); AdjustY(Y2,DeltaY2);

	if Random(5)==3 then {
	    x1 = (x1+x2) / 2; /* shorten the lines */
	    y2 = (y1+y2) / 2;
	}

	DrawCurrentLine;
	Updateline;
	CheckForUserInput;
    until Ch==#27;
}

{
     Init();
     Frame();
     MessageFrame("Press a key to stop action, Esc quits.");
     DoArt();
     CloseGraph();
     RestoreCrtMode();
     printf("The end.\n");
}.
