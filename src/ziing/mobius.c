/* $Id: mobius.c,v 1.2 2002/12/18 22:55:25 pavlovskii Exp $ */

#include "common.h"
#include <os/queue.h>
#include <os/keyboard.h>
#include <os/syscall.h>
#include <os/rtl.h>

handle_t ed_event_waiting;
queue_t ed_event_queue;

void gotoxy(unsigned col, unsigned row)
{
    printf("\x1b[%u;%uH", row, col);
    fflush(stdout);
}

void clreol(void)
{
    printf("\x1b[K");
    fflush(stdout);
}

void clrscr(void)
{
    printf("\x1b[2J");
    fflush(stdout);
}

bool EdGetEvent(ed_event_t *evt)
{
    do
    {
        ThrWaitHandle(ed_event_waiting);
        //printf("end\n");
    } while (!QueuePopLast(&ed_event_queue, evt, sizeof(*evt)));

    //printf("EdGetEvent: %u\n", evt->type);
    return evt->type != evtQuit;
}

void EdPostEvent(unsigned type, uint32_t param1, uint32_t param2)
{
    ed_event_t evt;
    evt.type = type;
    evt.param1 = param1;
    evt.param2 = param2;
    QueueAppend(&ed_event_queue, &evt, sizeof(evt));
    EvtSignal(ed_event_waiting);
}

static void EdKeyboardThread(void)
{
    handle_t keyb;
    uint32_t key;

    keyb = ProcGetProcessInfo()->std_in;
    if (keyb == NULL)
    {
        perror("keyboard");
        ThrExitThread(errno);
    }

    while (true)
    {
        FsReadSync(keyb, &key, sizeof(key), NULL);
        if (key & KBD_BUCKY_RELEASE)
            EdPostEvent(evtKeyUp, key & ~KBD_BUCKY_RELEASE, 0);
        else
            EdPostEvent(evtKeyDown, key, 0);
    }

    ThrExitThread(0);
}

void EdInitEvents(void)
{
    ed_event_waiting = EvtCreate();
    ThrCreateThread(EdKeyboardThread, NULL, 16);
    QueueInit(&ed_event_queue);
}
