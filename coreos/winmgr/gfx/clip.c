/* $Id: clip.c,v 1.3 2002/08/14 16:23:59 pavlovskii Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "internal.h"

int ClipIntersect(const rect_t* pos, const rect_t *Clip)
{
    int ThisX2, ThisY2;
    int X2, Y2;
    int RetVal;

    ThisY2 = pos->bottom;
    Y2 = Clip->bottom;
    if (Clip->top < pos->top)        /* T// */
    {
        if (Y2 < pos->top)
            RetVal=0;                /* TB// */
        else if (Y2 < ThisY2)
            RetVal=0x10;             /* T/B/ */
        else
            RetVal=0x20;             /* T//B */
    }
    else if (Clip->top < ThisY2)     /* /T/ */
    {
        if (Y2 < ThisY2)             /* /TB/ */
            RetVal=0x30;
        else
            RetVal=0x40;             /* /T/B */
    }
    else
        RetVal=0x50;                 /* //TB */

    ThisX2=pos->right;
    X2=Clip->right;
    if (Clip->left < pos->left)      /* L// */
    {
        if (X2 < pos->left)
            RetVal |= 0;             /* LR// */
        else if (X2 < ThisX2)
            RetVal |= 1;             /* L/R/ */
        else
            RetVal |= 2;             /* L//R */
    }

    else if (Clip->left < ThisX2)    /* /L/ */
    {
        if (X2 < ThisX2)
            RetVal |= 3;             /* /LR/ */
        else
            RetVal |= 4;             /* /L/R */
    }
    else
        RetVal |= 5;                 /* //LR */

    return(RetVal);
}

static bool clipNew(clip_t *clip, const rect_t *OldClip, int DeltaX, int DeltaY)
{
    rect_t *NewClip;

    /* add another clip to *Newrects */
    NewClip = realloc(clip->rects, sizeof(rect_t) * 
        (clip->num_rects + 1));
    if (NewClip == NULL)
        return false;
    clip->rects = NewClip;
    NewClip += clip->num_rects;
    clip->num_rects++;

    /* simple clipping (clip on the right): reduce width */
    if (DeltaX >= 0)
    {
        NewClip[0].left = OldClip->left;
        NewClip[0].right = OldClip->right - DeltaX;
    }
    /* clip on the left: reduce width and also advance SrcX and DstX */
    else if (DeltaX < 0)
    {
        NewClip[0].left = -DeltaX + OldClip->left;
        NewClip[0].right = OldClip->right;
    }
    
    /* clip on the bottom: reduce height */
    if (DeltaY >= 0)
    {
        NewClip[0].top = OldClip->top;
        NewClip[0].bottom = OldClip->bottom - DeltaY;
    }
    /* clip on the top: reduce height and advance SrcY and DstY */
    else if (DeltaY < 0)
    {
        NewClip[0].top = -DeltaY + OldClip->top;
        NewClip[0].bottom = OldClip->bottom;
    }
    
    return true;
}

/*void wndiGetOutline(window_t* wnd, rect_t** rects, int* num_rects)
{
    event_t evt;
    eventInit(&evt);
    eventSetInfo(&evt, eventOutlineNumRects, 0);
    eventSetInfo(&evt, eventOutlineRects, (dword) NULL);
    eventSend(wndiMarshal(wnd), msgGetOutline, &evt);
    *num_rects = eventGetInfo(&evt, eventOutlineNumRects);
    *rects = (rect_t*) eventGetInfo(&evt, eventOutlineRects);
    eventDelete(&evt);
}*/

static void clipDoClip(gfx_t *gfx, wnd_t* Next)
{
    clip_t new_rects;
    unsigned i, j, num_rects;
    int DeltaX, DeltaY;
    rect_t *OldClip;
    rect_t *rects, temp2;
    rect_t temp;

    /*if (!Next->visible)
        return;*/

    /* is the clip in the current window obscured by the closer window? */
    /*wndiGetOutline(Next, &rects, &num_rects);*/
    num_rects = 1;
    WmgrGetPosition(Next, &temp);
    temp2.left = temp.left;
    temp2.top = temp.top;
    temp2.right = temp.right;
    temp2.bottom = temp.bottom;
    rects = &temp2;

    for (j = 0; j < num_rects; j++)
    {
        OldClip = gfx->clip.rects;
        /* for each clip in the current window */
        new_rects.rects = 0;
        new_rects.num_rects = 0;
        for (i = 0; i < gfx->clip.num_rects; i++)
        {
            int ISect;

            ISect = ClipIntersect(rects + j, OldClip);
            switch(ISect)
            //
            // simple overlap: the clip is reduced in size along one side;
            // no additional rects are created
            // xxx - guidoc "obscure2.htm" says these values are for intrusions
            //
            {
            case 0x31:
                DeltaX = OldClip->right - rects[j].left;
                if (!clipNew(&new_rects, OldClip, DeltaX, 0))
                    goto BAIL;
                break;
            case 0x34:
                DeltaX = rects[j].right - OldClip->left;
                if (!clipNew(&new_rects, OldClip, -DeltaX, 0))
                    goto BAIL;
                break;
            case 0x13:
                DeltaY=OldClip->bottom - rects[j].top;
                if (!clipNew(&new_rects, OldClip, 0, DeltaY))
                    goto BAIL;
                break;
            case 0x43:
                DeltaY=rects[j].bottom - OldClip->top;
                if (!clipNew(&new_rects, OldClip, 0, -DeltaY))
                    goto BAIL;
                break;
            //
            // corner overlap results in the creation of ONE additional clip
            //
            case 0x11:
                DeltaX=0;
                DeltaY=OldClip->bottom - rects[j].top;
                if (!clipNew(&new_rects, OldClip, DeltaX, DeltaY))
                    goto BAIL;
                DeltaX = OldClip->right - rects[j].left;
                DeltaY = OldClip->bottom - OldClip->top - DeltaY;
                if (!clipNew(&new_rects, OldClip, DeltaX, -DeltaY))
                    goto BAIL;
                break;
            case 0x44:
                DeltaX=0;
                DeltaY=rects[j].bottom - OldClip->top;
                if (!clipNew(&new_rects, OldClip, 0, -DeltaY))
                    goto BAIL;
                DeltaX=rects[j].right - OldClip->left;
                DeltaY=OldClip->bottom - OldClip->top - DeltaY;
                if (!clipNew(&new_rects, OldClip, -DeltaX, DeltaY))
                    goto BAIL;
                break;
            case 0x14:
                DeltaX=rects[j].right - OldClip->left;
                DeltaY=OldClip->bottom - rects[j].top;
                if (!clipNew(&new_rects, OldClip, 0, DeltaY))
                    goto BAIL;
                DeltaY=OldClip->bottom - OldClip->top - DeltaY;
                if (!clipNew(&new_rects, OldClip, -DeltaX, -DeltaY))
                    goto BAIL;
                break;
            case 0x41:
                DeltaX=OldClip->right - rects[j].left;
                DeltaY=rects[j].bottom - OldClip->top;
                if (!clipNew(&new_rects, OldClip, 0, -DeltaY))
                    goto BAIL;
                DeltaY=OldClip->bottom - OldClip->top - DeltaY;
                if (!clipNew(&new_rects, OldClip, DeltaX, DeltaY))
                    goto BAIL;
                break;
            //
            // "band" or "belt" overlap results in the creation of ONE additional clip
            //
            case 0x23:
                DeltaY=OldClip->bottom - rects[j].top;
                if (!clipNew(&new_rects, OldClip, 0, DeltaY))
                    goto BAIL;
                DeltaY=rects[j].bottom - OldClip->top;
                if (!clipNew(&new_rects, OldClip, 0, -DeltaY))
                    goto BAIL;
                break;
            case 0x32:
                DeltaX=OldClip->right - rects[j].left;
                if (!clipNew(&new_rects, OldClip, DeltaX, 0))
                    goto BAIL;
                DeltaX=rects[j].right - OldClip->left;
                if (!clipNew(&new_rects, OldClip, -DeltaX, 0))
                    goto BAIL;
                break;
            //
            // overlap is a "tongue" or intrusion on one of the four sides;
            // and results in the creation of TWO additional rects
            //
            case 0x12:
                DeltaY=OldClip->bottom - rects[j].top;
                if (!clipNew(&new_rects, OldClip, 0, DeltaY))
                    goto BAIL;
                DeltaX=OldClip->right - rects[j].left;
                if (!clipNew(&new_rects, OldClip, DeltaX, 0))
                    goto BAIL;
                DeltaX=rects[j].right - OldClip->left;
                if (!clipNew(&new_rects, OldClip, -DeltaX, 0))
                    goto BAIL;
                break;
            case 0x42:
                DeltaY=rects[j].bottom - OldClip->top;
                if (!clipNew(&new_rects, OldClip, 0, -DeltaY))
                    goto BAIL;
                DeltaX=OldClip->right - rects[j].left;
                if (!clipNew(&new_rects, OldClip, DeltaX, 0))
                    goto BAIL;
                DeltaX=rects[j].right - OldClip->left;
                if (!clipNew(&new_rects, OldClip, -DeltaX, 0))
                    goto BAIL;
                break;
            case 0x21:
                DeltaY=OldClip->bottom - rects[j].top;
                if (!clipNew(&new_rects, OldClip, 0, DeltaY))
                    goto BAIL;
                DeltaX=OldClip->right - rects[j].left;
                DeltaY=OldClip->bottom - OldClip->top - DeltaY;
                if (!clipNew(&new_rects, OldClip, DeltaX, -DeltaY))
                    goto BAIL;
                /* this clip is clipped on 3 sides
                win::makeNewClip() can't handle that, so finish the clipping here */
                new_rects.rects[new_rects.num_rects - 1].bottom = rects[j].bottom;

                DeltaY=rects[j].bottom - OldClip->top;
                if (!clipNew(&new_rects, OldClip, 0, -DeltaY))
                    goto BAIL;
                break;
            case 0x24:
                DeltaY=OldClip->bottom - rects[j].top;
                if (!clipNew(&new_rects, OldClip, 0, DeltaY))
                    goto BAIL;
                DeltaX=rects[j].right - OldClip->left;
                DeltaY=OldClip->bottom - OldClip->top - DeltaY;
                if (!clipNew(&new_rects, OldClip, -DeltaX, -DeltaY))
                    goto BAIL;
                /* this clip is clipped on 3 sides
                win::makeNewClip() can't handle that, so finish the clipping here */
                new_rects.rects[new_rects.num_rects - 1].bottom=rects[j].bottom;

                DeltaY=rects[j].bottom - OldClip->top;
                if (!clipNew(&new_rects, OldClip, 0, -DeltaY))
                    goto BAIL;
                break;
            //
            // the closer window sits entirely "within" the clip;
            // THREE additional rects are created
            // xxx - guidoc "obscure2.htm" says 0x33 for this case
            //
            case 0x22:
                DeltaY=OldClip->bottom + - rects[j].top;
                if (!clipNew(&new_rects, OldClip, 0, DeltaY))
                    goto BAIL;

                DeltaY=rects[j].bottom - OldClip->top;
                if (!clipNew(&new_rects, OldClip, 0, -DeltaY))
                    goto BAIL;

                DeltaX=OldClip->right - rects[j].left;
                DeltaY=rects[j].top - OldClip->top;
                if (!clipNew(&new_rects, OldClip, DeltaX, -DeltaY))
                    goto BAIL;
                /* this clip is clipped on 3 sides
                win::makeNewClip() can't handle that, so finish the clipping here */
                new_rects.rects[new_rects.num_rects - 1].bottom = rects[j].bottom;

                DeltaX=rects[j].right - OldClip->left;
                DeltaY=rects[j].top - OldClip->top;
                if (!clipNew(&new_rects, OldClip, -DeltaX, -DeltaY))
                    goto BAIL;
                /* this clip is clipped on 3 sides
                win::makeNewClip() can't handle that, so finish the clipping here */
                new_rects.rects[new_rects.num_rects - 1].bottom = rects[j].bottom;
                break;
            //
            // the closer window overlaps the clip completely
            // xxx - guidoc "obscure2.htm" says 0x22 for this case
            //
            case 0x33:
                break;
            //
            // no overlap: the clip in the current window is unobscured
            //
            default:
                if (!clipNew(&new_rects, OldClip, 0, 0))
                    goto BAIL;
                break;
            }

            OldClip++;
        }

        if (gfx->clip.rects)
            free(gfx->clip.rects);

        gfx->clip = new_rects;
    }

BAIL:
    ;
    //if (rects != &temp2)
        //free(rects);
}

static void clipAddSiblingClips(gfx_t* addto, wnd_t* wnd)
{
    wnd_t *sib;

    for (sib = wnd->next; sib; sib = sib->next)
        //if (sib != addto)
            clipDoClip(addto, sib);
    
    if (wnd->parent)
    {
        sib = WmgrLockWindow(wnd->parent);
        clipAddSiblingClips(addto, sib);
        WmgrUnlockWindow(sib);
    }
}

void WmgrUpdateClip(gfx_t *gfx, wnd_t* wnd)
{
    wnd_t *sib;

    /* clobber the old rects */
    if (gfx->clip.rects != NULL)
    {
        free(gfx->clip.rects);
        gfx->clip.rects = NULL;
        gfx->clip.num_rects = 0;
    }

    /* create one clip equal to entire window */
    gfx->clip.rects = malloc(sizeof(rect_t) * 1);
    /* xxx - check if out of memory */

    if (wnd->invalid.right - wnd->invalid.left != 0 &&
        wnd->invalid.bottom - wnd->invalid.top != 0)
        *gfx->clip.rects = wnd->invalid;
    else
        WmgrGetPosition(wnd, gfx->clip.rects);

    //printf("win::clip (1)\n");
    /* clip it against the screen */
    /*if (!surfClip(&gfx->clip.rects->SrcX, &gfx->clip.rects->SrcY,
        &gfx->clip.rects->DstX, &gfx->clip.rects->DstY,
        &gfx->clip.rects->Wd, &gfx->clip.rects->Ht, _ScnWd, _ScnHt))
    {
        free(gfx->clip.rects);
        gfx->clip.rects = NULL;
        return;
    }*/

    gfx->clip.num_rects = 1;

    for (sib = wnd->child_first; sib; sib = sib->next)
        clipDoClip(gfx, sib);
    
    clipAddSiblingClips(gfx, wnd);
}
