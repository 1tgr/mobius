/* $Id: sound.h,v 1.3 2002/08/17 22:52:06 pavlovskii Exp $ */

#ifndef SOUND_H__
#define SOUND_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include <kernel/memory.h>

typedef struct sound_t sound_t;

typedef struct sound_vtbl_t sound_vtbl_t;
struct sound_vtbl_t
{
    bool (*SndOpen)(sound_t *snd);
    void (*SndClose)(sound_t *snd);
    bool (*SndIsr)(sound_t *snd, uint8_t irq);
    status_t (*SndStartBuffer)(sound_t *snd, page_array_t *pages, 
        size_t length, bool is_recording);
};

struct sound_t
{
    const sound_vtbl_t *vtbl;
};

void    SndFinishedBuffer(device_t *dev);

#ifdef __cplusplus
}
#endif

#endif
