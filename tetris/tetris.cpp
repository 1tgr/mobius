/* $Id: tetris.cpp,v 1.1 2002/12/21 09:51:11 pavlovskii Exp $ */

#include <gui/application.h>
#include <gui/frame.h>
#include <gui/view.h>
#include <gui/messagebox.h>

#include <os/keyboard.h>
#include <os/rtl.h>
#include <os/syscall.h>

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
    char Plus90, Minus90;    /* pointer to shape rotated +/- 90 degrees */
    MGLcolour Color;        /* shape color */
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

class TetrisView : public os::View
{
public:
    char m_Dirty[SCN_HT];
    MGLcolour m_Screen[SCN_WID][SCN_HT];

    TetrisView(os::Container *parent) :
        os::View(parent, os::alClient, 0)
    {
    }

    void OnPaint(mgl::Rc *rc)
    {
        MGLrect pos, r;
        unsigned XPos, YPos;

        GetPosition(&pos);

        for (YPos = 0; YPos < SCN_HT; YPos++)
        {
            if (!m_Dirty[YPos])
                continue;

            for (XPos = 0; XPos < SCN_WID; XPos++)
            {
                rc->SetFillColour(m_Screen[XPos][YPos]);
                r = MGLrect(XPos * 40, YPos * 40, (XPos + 1) * 40, (YPos + 1) * 40);
                r.Offset(pos.left, pos.top);
                rc->FillRect(r);

                if (m_Screen[XPos][YPos] != 0)
                    rc->Bevel(r, 2, 0x40, true);
            }

            m_Dirty[YPos] = 0;
        }
    }
};

class TetrisFrame : public os::Frame
{
protected:
    TetrisView m_view;
    unsigned m_X, m_Y;
    unsigned m_Shape;
    bool m_key_pressed;

public:
    TetrisFrame() : 
        os::Frame(L"Tetris", MGLrect(200, 100, 200 + 40 * SCN_WID + 16, 100 + 40 * SCN_HT + 46)), 
        m_view(this)
    {
        ScreenInit();
        ShapeSpawn();
        ThrCreateThread(FallerThread, this, 16);
    }

    ~TetrisFrame()
    {
    }

    void Collapse()
    {
        char SolidRow[SCN_HT], SolidRows;
        int Row, Col, Temp;

        /* determine which rows are solidly filled */
        SolidRows=0;
        for (Row=1; Row < SCN_HT - 1; Row++)
        {
            Temp=0;
            for(Col=1; Col < SCN_WID - 1; Col++)
                if(m_view.m_Screen[Col][Row])
                    Temp++;

            if(Temp == SCN_WID - 2)
            {
                SolidRow[Row]=1;
                SolidRows++;
            }
            else
                SolidRow[Row]=0;
        }

        if(SolidRows == 0)
            return;

        /* collapse them */
        for(Temp=Row=SCN_HT - 2; Row > 0; Row--, Temp--)
        {
            while(SolidRow[Temp])
                Temp--;
            if(Temp < 1)
            {
                for(Col=1; Col < SCN_WID - 1; Col++)
                    m_view.m_Screen[Col][Row]=COLOR_BLACK;
            }
            else
            {
                for(Col=1; Col < SCN_WID - 1; Col++)
                    m_view.m_Screen[Col][Row]=m_view.m_Screen[Col][Temp];
            }
            m_view.m_Dirty[Row]=true; 
        }

        m_view.Invalidate();
    }

    void ScreenInit()
    {
        unsigned XPos, YPos;

        for(YPos=0; YPos < SCN_HT; YPos++)
        {
            m_view.m_Dirty[YPos]=1;    /* force entire m_view.m_Screen to be redrawn */
            for(XPos=1; XPos < (SCN_WID - 1); XPos++)
                m_view.m_Screen[XPos][YPos]=0;
            m_view.m_Screen[0][YPos]=m_view.m_Screen[SCN_WID - 1][YPos]=COLOR_BLUE;
        }

        for(XPos=0; XPos < SCN_WID; XPos++)
            m_view.m_Screen[XPos][0]=m_view.m_Screen[XPos][SCN_HT - 1]=COLOR_BLUE;
    }

    bool ShapeHit(unsigned XPos, unsigned YPos, unsigned WhichShape)
    {
        unsigned Index;

        for(Index=0; Index < 4; Index++)
        {
            if(BlockHit(XPos, YPos))
                return true;
            XPos += Shapes[WhichShape].Dir[Index].DeltaX;
            YPos += Shapes[WhichShape].Dir[Index].DeltaY;
        }

        if(BlockHit(XPos, YPos))
            return true;
        return false;
    }

    void ShapeErase(unsigned XPos, unsigned YPos, unsigned WhichShape)
    {
        unsigned Index;

        for(Index=0; Index < 4; Index++)
        {
            BlockDraw(XPos, YPos, COLOR_BLACK);
            XPos += Shapes[WhichShape].Dir[Index].DeltaX;
            YPos += Shapes[WhichShape].Dir[Index].DeltaY;
        }
        BlockDraw(XPos, YPos, COLOR_BLACK);
    }

    void ShapeDraw(unsigned XPos, unsigned YPos, unsigned WhichShape)
    {
        unsigned Index;

        for(Index=0; Index < 4; Index++)
        {
            BlockDraw(XPos, YPos, Shapes[WhichShape].Color);
            XPos += Shapes[WhichShape].Dir[Index].DeltaX;
            YPos += Shapes[WhichShape].Dir[Index].DeltaY;
        }
        BlockDraw(XPos, YPos, Shapes[WhichShape].Color);
    }

    bool BlockHit(unsigned XPos, unsigned YPos)
    {
        return m_view.m_Screen[XPos][YPos] != 0;
    }

    void BlockDraw(unsigned XPos, unsigned YPos, MGLcolour Color)
    {
        if(XPos >= SCN_WID)
            XPos=SCN_WID - 1;
        if(YPos >= SCN_HT)
            YPos=SCN_HT - 1;
    
        m_view.m_Screen[XPos][YPos]=Color;
        m_view.m_Dirty[YPos] = 1;    /* this row has been modified */
    }

    void ShapeSpawn()
    {
        m_Y = 3;
        m_X = SCN_WID / 2;
        m_Shape = rand() % _countof(Shapes);
        Collapse();

        /* if newly spawned shape hits something, game over */
        if (ShapeHit(m_X, m_Y, m_Shape))
        {
            os::MessageBox msg(L"Tetris", L"GAME OVER");
            msg.DoModal();
            ScreenInit();
        }
    }

    void MovePiece(unsigned NewX, unsigned NewY, unsigned NewShape, bool Fell)
    {
        ShapeErase(m_X, m_Y, m_Shape);

        /* hit anything? */
        if (ShapeHit(NewX, NewY, NewShape) == 0)
        {
            /* no, update pos'n */
            m_X = NewX;
            m_Y = NewY;
            m_Shape = NewShape; 
        }
        else if (Fell)
        {
            /* yes, draw it at the old pos'n... */
            ShapeDraw(m_X, m_Y, m_Shape);
            ShapeSpawn();
        }
    }

    static void FallerThread()
    {
        TetrisFrame *frame;
        uint32_t end;
        bool key_was_pressed;

        frame = (TetrisFrame*) ThrGetThreadInfo()->param;
        while (true)
        {
            end = SysUpTime() + 200;
            frame->m_key_pressed = false;
            while (SysUpTime() < end)
            {
                if (frame->m_key_pressed)
                {
                    key_was_pressed = true;
                    break;
                }
            }

            if (!key_was_pressed)
            {
                frame->MovePiece(frame->m_X, frame->m_Y + 1, frame->m_Shape, true);
                frame->ShapeDraw(frame->m_X, frame->m_Y, frame->m_Shape);
                frame->m_view.Invalidate();
            }
        }
    }

    void OnKeyDown(uint32_t key)
    {
        unsigned NewShape, NewX, NewY;

        NewShape = m_Shape;
        NewX = m_X;
        NewY = m_Y;
        switch (key)
        {
        case 27:
            PostMessage(MSG_CLOSE);
            PostMessage(MSG_QUIT);
            break;

        case KEY_LEFT:
            if (m_X > 0)
                NewX = m_X - 1;
            break;

        case KEY_RIGHT:
            if (m_X < SCN_WID - 1)
                NewX = m_X + 1;
            break;

        case KEY_UP:
            NewShape = Shapes[m_Shape].Plus90;
            break;

        case '2':
            NewShape = Shapes[m_Shape].Minus90;
            break;

        case KEY_DOWN:
            if (m_Y < SCN_HT - 1)
                NewY = m_Y + 1;
            break;
        }

        if (NewX != m_X || NewY != m_Y || NewShape != m_Shape)
            MovePiece(NewX, NewY, NewShape, false);

        ShapeDraw(m_X, m_Y, m_Shape);
        m_view.Invalidate();
    }

    void OnClose()
    {
        delete this;
    }
};

class TetrisApp : public os::Application
{
public:
    TetrisApp(const wchar_t *name) : os::Application(name)
    {
        IpcConnect(SYS_PORTS L"/winmgr");
        __stub_WndCreate(NULL, NULL, 0);
        msg_t msg;
        __stub_WndGetMessage(&msg);

        new TetrisFrame;
    }
};

int main()
{
    TetrisApp app(L"application/x-vnd.tetris");
    return app.Run();
}
