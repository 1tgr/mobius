#ifndef __OS_BLKDEV_H
#define __OS_BLKDEV_H

#include <sys/types.h>

typedef struct block_size_t block_size_t;
struct block_size_t
{
	size_t block_size;
	size_t total_blocks;
};

#endif