/*
 * $History: tetris.c $
 * 
 * *****************  Version 3  *****************
 * User: Tim          Date: 13/03/04   Time: 22:28
 * Updated in $/coreos/apps/tetris
 * Fixed write_status
 * Added a history block
 */

#include <stdlib.h> /* random() */
#include <stdio.h> /* printf() */
#include <stddef.h>
#include <errno.h>
#include <wchar.h>
#include <string.h>

#include <os/keyboard.h>
#include <os/syscall.h>
#include <os/defs.h>
#include <os/rtl.h>
#include <os/ioctl.h>
#include <os/framebuf.h>

/*!
 *    \ingroup    programs
 *    \defgroup    tetris    Tetris
 *    @{
 */

/* dimensions of playing area */
#define    SCN_WID            15
#define    SCN_HT             20
/* direction vectors */
#define    DIR_UP             { 0, -1 }
#define    DIR_DN             { 0, +1 }
#define    DIR_LT             { -1, 0 }
#define    DIR_RT             { +1, 0 }
#define    DIR_UP2            { 0, -2 }
#define    DIR_DN2            { 0, +2 }
#define    DIR_LT2            { -2, 0 }
#define    DIR_RT2            { +2, 0 }
/* Colours */
#define    COLOR_BLACK        0x000000
#define    COLOR_RED          0x800000
#define    COLOR_GREEN        0x008000
#define    COLOR_YELLOW       0x808000
#define    COLOR_BLUE         0x000080
#define    COLOR_MAGENTA      0x800080
#define    COLOR_CYAN         0x008080
#define    COLOR_WHITE        0x808080

typedef struct
{
    short int DeltaX, DeltaY;
} vector;

typedef struct
{
    char Plus90, Minus90;   /* pointer to shape rotated +/- 90 degrees */
    colour_t Color;         /* shape color */
    vector Dir[4];          /* drawing instructions for this shape */
} shape;

shape Shapes[]=
{
/* shape #0:                cube */
    { 0, 0, COLOR_BLUE,     { DIR_UP, DIR_RT, DIR_DN, DIR_LT }},
/* shapes #1 & #2:          bar */
    { 2, 2, COLOR_GREEN,    { DIR_LT, DIR_RT, DIR_RT, DIR_RT }},
    { 1, 1, COLOR_GREEN,    { DIR_UP, DIR_DN, DIR_DN, DIR_DN }},
/* shapes #3 & #4:          'Z' shape */
    { 4, 4, COLOR_CYAN,     { DIR_LT, DIR_RT, DIR_DN, DIR_RT }},
    { 3, 3, COLOR_CYAN,     { DIR_UP, DIR_DN, DIR_LT, DIR_DN }},
/* shapes #5 & #6:          'S' shape */
    { 6, 6, COLOR_RED,      { DIR_RT, DIR_LT, DIR_DN, DIR_LT }},
    { 5, 5, COLOR_RED,      { DIR_UP, DIR_DN, DIR_RT, DIR_DN }},
/* shapes #7, #8, #9, #10:  'J' shape */
    { 8, 10, COLOR_MAGENTA, { DIR_RT, DIR_LT, DIR_LT, DIR_UP }},
    { 9, 7, COLOR_MAGENTA,  { DIR_UP, DIR_DN, DIR_DN, DIR_LT }},
    { 10, 8, COLOR_MAGENTA, { DIR_LT, DIR_RT, DIR_RT, DIR_DN }},
    { 7, 9, COLOR_MAGENTA,  { DIR_DN, DIR_UP, DIR_UP, DIR_RT }},
/* shapes #11, #12, #13, #14: 'L' shape */
    { 12, 14, COLOR_YELLOW, { DIR_RT, DIR_LT, DIR_LT, DIR_DN }},
    { 13, 11, COLOR_YELLOW, { DIR_UP, DIR_DN, DIR_DN, DIR_RT }},
    { 14, 12, COLOR_YELLOW, { DIR_LT, DIR_RT, DIR_RT, DIR_UP }},
    { 11, 13, COLOR_YELLOW, { DIR_DN, DIR_UP, DIR_UP, DIR_LT }},
/* shapes #15, #16, #17, #18: 'T' shape */
    { 16, 18, COLOR_WHITE, { DIR_UP, DIR_DN, DIR_LT, DIR_RT2 }},
    { 17, 15, COLOR_WHITE, { DIR_LT, DIR_RT, DIR_UP, DIR_DN2 }},
    { 18, 16, COLOR_WHITE, { DIR_DN, DIR_UP, DIR_RT, DIR_LT2 }},
    { 15, 17, COLOR_WHITE, { DIR_RT, DIR_LT, DIR_DN, DIR_UP2 }}
};

char Dirty[SCN_HT];
colour_t Screen[SCN_WID][SCN_HT];
unsigned block_size;

surface_t surf;
void *surf_base;
handle_t surf_vid;
videomode_t mode;
font_t *font;


//////////////////////////////////////////////////////////////////////////////
//            ANSI GRAPHIC OUTPUT
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
    name:    refresh
    action:    updates display device based on contents of global
        char array Screen[][]. Updates only those rows
        marked Dirty[]
*****************************************************************************/
void refresh(void)
{
    unsigned XPos, YPos;
    
    for(YPos=0; YPos < SCN_HT; YPos++)
    {
        if(!Dirty[YPos])
            continue;

        for(XPos=0; XPos < SCN_WID; XPos++)
        {
            surf.vtbl->SurfFillRect(&surf,
                XPos * block_size, YPos * block_size, 
                (XPos + 1) * block_size, (YPos + 1) * block_size,
                Screen[XPos][YPos]);

            if (Screen[XPos][YPos] != 0)
            {
                int r, g, b;

                r = min(COLOUR_RED(Screen[XPos][YPos]) + 0x40, 255);
                g = min(COLOUR_GREEN(Screen[XPos][YPos]) + 0x40, 255);
                b = min(COLOUR_BLUE(Screen[XPos][YPos]) + 0x40, 255);

                surf.vtbl->SurfLine(&surf,
                    (XPos + 1) * block_size, YPos * block_size,
                    XPos * block_size, YPos * block_size,
                    MAKE_COLOUR(r, g, b));
                surf.vtbl->SurfLine(&surf,
                    XPos * block_size, YPos * block_size,
                    XPos * block_size, (YPos + 1) * block_size,
                    MAKE_COLOUR(r, g, b));
            }
        }

        Dirty[YPos]=0;
    }

    surf.vtbl->SurfFlush(&surf);
}
//////////////////////////////////////////////////////////////////////////////
//            GRAPHIC CHUNK DRAW & HIT DETECT
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
    name:    blockDraw
    action:    draws one graphic block in display buffer at
        position (XPos, YPos)
*****************************************************************************/
void blockDraw(unsigned XPos, unsigned YPos, colour_t Color)
{
    if(XPos >= SCN_WID)
        XPos=SCN_WID - 1;
    if(YPos >= SCN_HT)
        YPos=SCN_HT - 1;
    
    Screen[XPos][YPos]=Color;
    Dirty[YPos] = 1;    /* this row has been modified */
}
/*****************************************************************************
    name:    blockHit
    action:    determines if coordinates (XPos, YPos) are already
        occupied by a graphic block
    returns:color of graphic block at (XPos, YPos) (zero if black/
        empty)
*****************************************************************************/
bool blockHit(unsigned XPos, unsigned YPos)
{
    return Screen[XPos][YPos] != 0;
}

//////////////////////////////////////////////////////////////////////////////
//            SHAPE DRAW & HIT DETECT
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
    name:    shapeDraw
    action:    draws shape WhichShape in display buffer at
        position (XPos, YPos)
*****************************************************************************/
void shapeDraw(unsigned XPos, unsigned YPos, unsigned WhichShape)
{
    unsigned Index;

    for(Index=0; Index < 4; Index++)
    {    blockDraw(XPos, YPos, Shapes[WhichShape].Color);
        XPos += Shapes[WhichShape].Dir[Index].DeltaX;
        YPos += Shapes[WhichShape].Dir[Index].DeltaY;
    }
    blockDraw(XPos, YPos, Shapes[WhichShape].Color);
}
/*****************************************************************************
    name:    shapeErase
    action:    erases shape WhichShape in display buffer at
        position (XPos, YPos) by setting its color to zero
*****************************************************************************/
void shapeErase(unsigned XPos, unsigned YPos, unsigned WhichShape)
{
    unsigned Index;

    for(Index=0; Index < 4; Index++)
    {
        blockDraw(XPos, YPos, COLOR_BLACK);
        XPos += Shapes[WhichShape].Dir[Index].DeltaX;
        YPos += Shapes[WhichShape].Dir[Index].DeltaY;
    }
    blockDraw(XPos, YPos, COLOR_BLACK);
}
/*****************************************************************************
    name:    shapeHit
    action:    determines if shape WhichShape would collide with
        something already drawn in display buffer if it
        were drawn at position (XPos, YPos)
    returns:nonzero if hit, zero if nothing there
*****************************************************************************/
char shapeHit(unsigned XPos, unsigned YPos, unsigned WhichShape)
{    unsigned Index;

    for(Index=0; Index < 4; Index++)
    {    if(blockHit(XPos, YPos))
            return(1);
        XPos += Shapes[WhichShape].Dir[Index].DeltaX;
        YPos += Shapes[WhichShape].Dir[Index].DeltaY; }
    if(blockHit(XPos, YPos))
        return(1);
    return(0); }
//////////////////////////////////////////////////////////////////////////////
//            MAIN ROUTINES
//////////////////////////////////////////////////////////////////////////////
/*****************************************************************************
    name:    screenInit
    action:    clears display buffer, marks all rows dirty,
        set raw keyboard mode
*****************************************************************************/
void screenInit(void)
{    unsigned XPos, YPos;

    for(YPos=0; YPos < SCN_HT; YPos++)
    {    Dirty[YPos]=1;    /* force entire screen to be redrawn */
        for(XPos=1; XPos < (SCN_WID - 1); XPos++)
            Screen[XPos][YPos]=0;
        Screen[0][YPos]=Screen[SCN_WID - 1][YPos]=COLOR_BLUE; }
    for(XPos=0; XPos < SCN_WID; XPos++)
        Screen[XPos][0]=Screen[XPos][SCN_HT - 1]=COLOR_BLUE; }
/*****************************************************************************
*****************************************************************************/
void collapse(void)
{    char SolidRow[SCN_HT], SolidRows;
    int Row, Col, Temp;

/* determine which rows are solidly filled */
    SolidRows=0;
    for(Row=1; Row < SCN_HT - 1; Row++)
    {    Temp=0;
        for(Col=1; Col < SCN_WID - 1; Col++)
            if(Screen[Col][Row])
                Temp++;
        if(Temp == SCN_WID - 2)
        {    SolidRow[Row]=1;
            SolidRows++; }
        else SolidRow[Row]=0; }
    if(SolidRows == 0)
        return;
/* collapse them */
    for(Temp=Row=SCN_HT - 2; Row > 0; Row--, Temp--)
    {    while(SolidRow[Temp])
            Temp--;
        if(Temp < 1)
        {    for(Col=1; Col < SCN_WID - 1; Col++)
                Screen[Col][Row]=COLOR_BLACK; }
        else
        {    for(Col=1; Col < SCN_WID - 1; Col++)
                Screen[Col][Row]=Screen[Col][Temp]; }
        Dirty[Row]=1; }
    refresh(); }
/*****************************************************************************
*****************************************************************************/

bool _kbhit(void)
{
    static handle_t event;
    fileop_t op;
    size_t bytes;

    if (event == 0)
        event = EvtCreate(false);

    bytes = 0;
    op.event = event;
    if (FsIoCtl(ProcGetProcessInfo()->std_in, 
        IOCTL_BYTES_AVAILABLE, 
        &bytes, 
        sizeof(bytes), 
        &op))
        return bytes > 0;
    else
    {
        errno = op.result;
        return false;
    }

    return false;
}

wint_t getKey(void)
{
    uint32_t end = SysUpTime() + 200;
    uint32_t key;

    while (!_kbhit())
        if (SysUpTime() >= end)
            return 0;

    if (!FsRead(ProcGetProcessInfo()->std_in, &key, 0, sizeof(key), NULL))
        return 0;

    return key;
}

void write_status(font_t *font, const wchar_t *text)
{
    rect_t dims;
    const wchar_t *sol, *eol;
    unsigned unused, line_height;

    dims.left = SCN_WID * block_size;
    dims.top = 0;
    dims.right = mode.width;
    dims.bottom = mode.height;
    surf.vtbl->SurfFillRect(&surf, 
        dims.left, dims.top,
        dims.right, dims.bottom,
        0);

    FontGetMaxSize(font, &unused, &line_height);

    eol = sol = text;
    while (*eol != '\0')
    {
        eol = wcschr(sol, '\n');
        if (eol == NULL)
            eol = sol + wcslen(sol);

        FontDrawText(font, &surf, dims.left, dims.top, sol, eol - sol, 0xffffff, 0);
        dims.top += line_height;
        sol = eol + 1;
    }
}

/*****************************************************************************
    name:    main
*****************************************************************************/
int main(void)
{
    params_vid_t params;
    videomode_t old_mode;
    char Fell, NewShape, NewX, NewY;
    unsigned Shape, X, Y;
    wint_t Key;

/* re-seed the random number generator */
    srand(SysUpTime());

    font = FontLoad(L"/Mobius/vera.ttf", 14 * 64, 0);
    if (font == NULL)
    {
        printf("Failed to load font file\n");
        return EXIT_FAILURE;
    }

    surf_vid = FsOpen(SYS_DEVICES L"/Classes/video0", 0);
    if (surf_vid == 0)
    {
        _pwerror(SYS_DEVICES L"/Classes/video0");
        goto error0;
    }

    memset(&params.vid_getmode, 0, sizeof(params.vid_getmode));
    FsRequestSync(surf_vid, VID_GETMODE, &params, sizeof(params), NULL);
    old_mode = params.vid_getmode;

    memset(&params.vid_setmode, 0, sizeof(params.vid_setmode));
    params.vid_setmode.bitsPerPixel = 8;
    if (!FsRequestSync(surf_vid, VID_SETMODE, &params, sizeof(params), NULL))
    {
        _pwerror(L"mode");
        goto error1;
    }

    mode = params.vid_setmode;
    if (mode.framebuffer[0] != '\0')
    {
        handle_t base_area;

        base_area = HndOpen(mode.framebuffer);
        if (base_area == 0)
        {
            _pwerror(mode.framebuffer);
            goto error2;
        }

        surf_base = VmmMapSharedArea(base_area, 0, VM_MEM_READ | VM_MEM_WRITE | VM_MEM_USER);
        if (surf_base == NULL)
        {
            _pwerror(L"map");
            HndClose(base_area);
            goto error2;
        }

        if (!FramebufCreateSurface(&mode, surf_base, &surf))
        {
            _pwerror(L"framebuffer");
            HndClose(base_area);
            goto error3;
        }

        HndClose(base_area);
    }
    else
    {
        surf_base = NULL;
        if (!AccelCreateSurface(&mode, surf_vid, &surf))
        {
            _pwerror(L"accel");
            goto error3;
        }
    }

    surf.vtbl->SurfFillRect(&surf,
        0, 0, 
        mode.width, mode.height,
        0);
    block_size = mode.height / SCN_HT;

NEW:
    screenInit();
    write_status(font,
        L"TETRIS by Alexei\n"
        L" Pazhitnov\n"
        L"Software by\n"
        L" Chris Giese\n"
        L"\x2018" L"1" L"\x2019" L" and " L"\x2018" L"2" L"\x2019" L" rotate\n"
        L"Arrow keys move\n"
        L"Esc or Q quits");
    goto FOO;

    while(1)
    {
        Fell = 0;
        NewShape = Shape;
        NewX = X;
        NewY = Y;
        Key = getKey();
        if (Key == 0)
        {
            NewY++;
            Fell = 1;
        }
        else
        {
            if(Key == 'q' || Key == 'Q' || Key == 27)
                break;
            if (Key == '1' || Key == KEY_UP)
                NewShape = Shapes[Shape].Plus90;
            else if (Key == '2')
                NewShape = Shapes[Shape].Minus90;
            else if (Key == KEY_LEFT)
            {
                if (X > 0)
                    NewX = X - 1;
            }
            else if (Key == KEY_RIGHT)
            {
                if (X < SCN_WID - 1)
                    NewX = X + 1;
            }
            else if (Key == KEY_DOWN)
            {
                if (Y < SCN_HT - 1)
                    NewY = Y + 1;
            }

            Fell = 0;
        }
        /* if nothing has changed, skip the bottom half of this loop */
        if(NewX == X && NewY == Y && NewShape == Shape)
            continue;
        /* otherwise, erase old shape from the old pos'n */
        shapeErase(X, Y, Shape);

        /* hit anything? */
        if(shapeHit(NewX, NewY, NewShape) == 0)
        {
            /* no, update pos'n */
            X = NewX;
            Y = NewY;
            Shape=NewShape; 
        }
        /* yes -- did the piece hit something while falling on its own? */
        else if(Fell)
        {
            /* yes, draw it at the old pos'n... */
            shapeDraw(X, Y, Shape);

            /* ... and spawn new shape */
FOO:        Y = 3;
            X = SCN_WID / 2;
            Shape = rand() % _countof(Shapes);
            collapse();

            /* if newly spawned shape hits something, game over */
            if (shapeHit(X, Y, Shape))
            {
                uint32_t key;
                write_status(font, L"GAME OVER");
                FsRead(ProcGetProcessInfo()->std_in, &key, 0, sizeof(key), NULL);
                goto NEW; 
            }
        }

        /* hit something because of user movement/rotate OR no hit: just redraw it */
        shapeDraw(X, Y, Shape);
        refresh();
    }

    surf.vtbl->SurfDeleteCookie(&surf);
    if (surf_base != NULL)
        VmmFree(surf_base);
    params.vid_setmode = old_mode;
    FsRequestSync(surf_vid, VID_SETMODE, &params, sizeof(params), NULL);
    HndClose(surf_vid);
    printf("\x1b[2J");
    fflush(stdout);
    return EXIT_SUCCESS;

error3:
    if (surf_base != NULL)
        VmmFree(surf_base);
error2:
    params.vid_setmode = old_mode;
    FsRequestSync(surf_vid, VID_SETMODE, &params, sizeof(params), NULL);
error1:
    HndClose(surf_vid);
error0:
    return EXIT_FAILURE;
}

//@}

