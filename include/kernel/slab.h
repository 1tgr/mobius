/* $Header: /include/kernel/slab.h 3     9/03/04 21:47 Tim $ */
#ifndef __KERNEL_SLAB_H
#define __KERNEL_SLAB_H

typedef void (*slab_ctor_t)(void*);
typedef void (*slab_dtor_t)(void*);

#define SLAB_CHUNK_SIZE             PAGE_SIZE
#define SLAB_OBJECTS_PER_CHUNK(q)   (SLAB_CHUNK_SIZE / q)

typedef struct slab_chunk_t slab_chunk_t;
struct slab_chunk_t
{
    slab_chunk_t *next;
    uint8_t *data;
    unsigned num_free_objects;
    uint8_t allocation_bitmap[1];
};

typedef struct slab_t slab_t;
struct slab_t
{
    slab_t *prev, *next;
    spinlock_t spin;
    slab_chunk_t *chunk_first;
    size_t quantum;
    slab_ctor_t ctor;
    slab_dtor_t dtor;
};

void SlabInit(slab_t *slab, size_t quantum, slab_ctor_t ctor, slab_dtor_t dtor);
void SlabDelete(slab_t *slab);
void *SlabAlloc(slab_t *slab);
void SlabFree(slab_t *slab, void *ptr);

#endif
