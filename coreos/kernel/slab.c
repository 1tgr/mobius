/*
 * $History: slab.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 8/03/04    Time: 20:33
 * Updated in $/coreos/kernel
 * Added slab allocator
 * 
 * *****************  Version 1  *****************
 * User: Tim          Date: 7/03/04    Time: 15:30
 * Created in $/coreos/kernel
 */

#include <kernel/kernel.h>
#include <kernel/slab.h>
#include <kernel/bitmap.h>
#include <kernel/memory.h>

#include <string.h>

#define OBJECTS_PER_CHUNK(q)    (SLAB_CHUNK_SIZE / q)

static spinlock_t spin_slab;
slab_t *slab_first, *slab_last;

static slab_chunk_t *SlabCreateChunk(slab_t *slab)
{
    slab_chunk_t *chunk;
    unsigned num_bits, num_objects, i;
    uint8_t *ptr;
    addr_t phys;

    num_objects = OBJECTS_PER_CHUNK(slab->quantum);
    num_bits = (num_objects + 7) & -8;
    chunk = malloc(sizeof(*chunk) - 1 + (num_bits / 8));
    if (chunk == NULL)
        goto err0;

    chunk->next = NULL;
    chunk->num_free_objects = num_objects;
    memset(chunk->allocation_bitmap, 0, num_bits / 8);

    chunk->data = sbrk_virtual(SLAB_CHUNK_SIZE);
    if (chunk->data == NULL)
        goto err1;

    for (ptr = chunk->data;
        ptr < chunk->data + SLAB_CHUNK_SIZE;
        ptr += PAGE_SIZE)
    {
        phys = MemAlloc();
        MemMapRange(ptr, phys, ptr + PAGE_SIZE, PRIV_RD | PRIV_WR | PRIV_KERN | PRIV_PRES);
    }

    for (i = 0; i < num_objects; i++)
        if (slab->ctor == NULL)
            memset(chunk->data + i * slab->quantum, 0, slab->quantum);
        else
            slab->ctor(chunk->data + i * slab->quantum);

    return chunk;

err1:
    free(chunk);
err0:
    return NULL;
}


static void SlabDeleteChunk(slab_t *slab, slab_chunk_t *chunk)
{
    unsigned num_objects, i;

    num_objects = OBJECTS_PER_CHUNK(slab->quantum);
    for (i = 0; i < num_objects; i++)
        if (slab->dtor != NULL)
            slab->dtor(chunk->data + i * slab->quantum);

    /* free_virtual(chunk->data); */
    free(chunk);
}


static void SlabInsertChunk(slab_t *slab, slab_chunk_t *chunk)
{
    chunk->next = slab->chunk_first;
    slab->chunk_first = chunk;
}


static bool SlabFindObject(slab_t *slab, void *ptr, slab_chunk_t **chunk, unsigned *index)
{
    slab_chunk_t *the_chunk;

    for (the_chunk = slab->chunk_first; the_chunk != NULL; the_chunk = the_chunk->next)
    {
        if ((uint8_t*) ptr >= the_chunk->data &&
            (uint8_t*) ptr < the_chunk->data + SLAB_CHUNK_SIZE)
        {
            *chunk = the_chunk;
            *index = ((uint8_t*) ptr - the_chunk->data) / slab->quantum;
            return true;
        }
    }

    return false;
}


void SlabInit(slab_t *slab, size_t quantum, slab_ctor_t ctor, slab_dtor_t dtor)
{
    SpinInit(&slab->spin);

    slab->chunk_first = NULL;
    slab->quantum = quantum;
    slab->ctor = ctor;
    slab->dtor = dtor;

    SpinAcquire(&spin_slab);
    LIST_ADD(slab, slab);
    SpinRelease(&spin_slab);
}


void SlabDelete(slab_t *slab)
{
    slab_chunk_t *chunk, *next;

    for (chunk = slab->chunk_first; chunk != NULL; chunk = next)
    {
        next = chunk->next;
        SlabDeleteChunk(slab, chunk);
    }

    SpinAcquire(&spin_slab);
    LIST_REMOVE(slab, slab);
    SpinRelease(&spin_slab);
}


void *SlabAlloc(slab_t *slab)
{
    slab_chunk_t *chunk;
    int index;

    SpinAcquire(&slab->spin);
    for (chunk = slab->chunk_first; chunk != NULL; chunk = chunk->next)
        if (chunk->num_free_objects > 0)
        {
            index = BitmapFindClearRange(chunk->allocation_bitmap, 
                OBJECTS_PER_CHUNK(slab->quantum), 
                1);
            assert(index > -1);
            break;
        }

    if (chunk == NULL)
    {
        chunk = SlabCreateChunk(slab);
        if (chunk == NULL)
            return NULL;
        SlabInsertChunk(slab, chunk);
        index = 0;
    }

    BitmapSetBit(chunk->allocation_bitmap, index, true);
    chunk->num_free_objects--;
    SpinRelease(&slab->spin);

    return chunk->data + slab->quantum * index;
}


void SlabFree(slab_t *slab, void *ptr)
{
    slab_chunk_t *chunk;
    unsigned index;
    bool found;

    SpinAcquire(&slab->spin);

    found = SlabFindObject(slab, ptr, &chunk, &index);
    assert(found);
    assert(chunk != NULL);
    assert(index < OBJECTS_PER_CHUNK(slab->quantum));

    BitmapSetBit(chunk->allocation_bitmap, index, false);
    chunk->num_free_objects++;

    SpinRelease(&slab->spin);
}


#include <wchar.h>

typedef struct slab_test_t slab_test_t;
struct slab_test_t
{
    wchar_t sig[10];
    int number;
    wchar_t a_bunch_of_stuff[123];
};


static void SlabTestInit(void *ptr)
{
    slab_test_t *t;
    t = ptr;
    wcscpy(t->sig, L"Slab Test");
    t->number = 0;
}


static void SlabTestDelete(void *ptr)
{
    slab_test_t *t;
    t = ptr;
    assert(wcscmp(t->sig, L"Slab Test") == 0);
    memset(t, 0xcd, sizeof(*t));
}


void test_slab(void)
{
    slab_t slab;
    slab_test_t *objects[16];
    unsigned i;

    wprintf(L"---- slab_test ----\n");
    wprintf(L"Calling SlabInit, %u objects per chunk...", 
        OBJECTS_PER_CHUNK(sizeof(slab_test_t)));
    SlabInit(&slab, sizeof(slab_test_t), SlabTestInit, SlabTestDelete);
    wprintf(L"done\n");

    for (i = 0; i < _countof(objects); i++)
    {
        wprintf(L"Allocating object %u\n", i);
        objects[i] = SlabAlloc(&slab);
        wprintf(L"\t%p\n", objects[i]);
        objects[i]->number = i;
    }

    for (i = 5; i < _countof(objects); i++)
    {
        wprintf(L"Freeing object %u\n", i);
        SlabFree(&slab, objects[i]);
    }

    i = 5;
    while (i > 0)
    {
        i--;
        wprintf(L"Freeing object %u\n", i);
        SlabFree(&slab, objects[i]);
    }

    for (i = 0; i < 2; i++)
    {
        wprintf(L"Allocating object\n");
        objects[0] = SlabAlloc(&slab);
        wprintf(L"\t%p\n", objects[0]);
        wprintf(L"Freeing object\n");
        SlabFree(&slab, objects[0]);
    }

    wprintf(L"Calling SlabDelete...");
    SlabDelete(&slab);

    __asm__("int3");
}
