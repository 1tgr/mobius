/* $Id: clip.c,v 1.2 2002/04/10 12:32:38 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/winmgr.h>
#include <stdio.h>
#include <stdlib.h>

int ClipIntersect(const MGLrect* pos, const MGLrect *Clip)
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

static bool clipNew(MGLclip *clip, const MGLrect *OldClip, int DeltaX, int DeltaY)
{
    MGLrect *NewClip;

    /* add another clip to *Newrects */
    NewClip = realloc(clip->rects, sizeof(MGLrect) * 
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

/*void wndiGetOutline(window_t* wnd, MGLrect** rects, int* num_rects)
{
    event_t evt;
    eventInit(&evt);
    eventSetInfo(&evt, eventOutlineNumRects, 0);
    eventSetInfo(&evt, eventOutlineRects, (dword) NULL);
    eventSend(wndiMarshal(wnd), msgGetOutline, &evt);
    *num_rects = eventGetInfo(&evt, eventOutlineNumRects);
    *rects = (MGLrect*) eventGetInfo(&evt, eventOutlineRects);
    eventDelete(&evt);
}*/

static void clipDoClip(window_t* wnd, window_t* Next)
{
    MGLclip new_rects;
    int i, j, DeltaX, DeltaY, num_rects;
    MGLrect *OldClip;
    MGLrect *rects, temp2;
    MGLrect temp;

    /*if (!Next->visible)
        return;*/

    /* is the clip in the current window obscured by the closer window? */
    /*wndiGetOutline(Next, &rects, &num_rects);*/
    num_rects = 1;
    WndiGetPosition(Next, &temp);
    temp2.left = temp.left;
    temp2.top = temp.top;
    temp2.right = temp.right;
    temp2.bottom = temp.bottom;
    rects = &temp2;

    for (j = 0; j < num_rects; j++)
    {
        OldClip = wnd->clip.rects;
        /* for each clip in the current window */
        new_rects.rects = 0;
        new_rects.num_rects = 0;
        for (i = 0; i < wnd->clip.num_rects; i++)
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

        if (wnd->clip.rects)
            free(wnd->clip.rects);

        wnd->clip = new_rects;
    }

BAIL:
    free(rects);
}

static void clipAddSiblingClips(window_t* addto, window_t* wnd)
{
    window_t *sib;

    for (sib = wnd->next; sib; sib = sib->next)
        //if (sib != addto)
            clipDoClip(addto, sib);
    
    if (wnd->parent)
        clipAddSiblingClips(addto, wnd->parent);
}

void WndiUpdateClip(window_t* wnd)
{
    window_t *sib;

    /* clobber the old rects */
    if (wnd->clip.rects != NULL)
    {
        free(wnd->clip.rects);
        wnd->clip.rects = NULL;
    }

    wnd->clip.num_rects = 0;
    /* create one clip equal to entire window */
    wnd->clip.rects = malloc(sizeof(MGLrect) * 1);
    /* xxx - check if out of memory */

    if (wnd->invalid_rect.right - wnd->invalid_rect.left != 0 &&
        wnd->invalid_rect.bottom - wnd->invalid_rect.top != 0)
        *wnd->clip.rects = wnd->invalid_rect;
    else
        WndiGetPosition(wnd, wnd->clip.rects);

    //printf("win::clip (1)\n");
    /* clip it against the screen */
    /*if (!surfClip(&wnd->clip.rects->SrcX, &wnd->clip.rects->SrcY,
        &wnd->clip.rects->DstX, &wnd->clip.rects->DstY,
        &wnd->clip.rects->Wd, &wnd->clip.rects->Ht, _ScnWd, _ScnHt))
    {
        free(wnd->clip.rects);
        wnd->clip.rects = NULL;
        return;
    }*/

    wnd->clip.num_rects = 1;

    for (sib = wnd->child_first; sib; sib = sib->next)
        clipDoClip(wnd, sib);
    
    clipAddSiblingClips(wnd, wnd);
}
