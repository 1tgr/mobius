/* $Id: blkdev.h,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */
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