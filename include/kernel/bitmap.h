/* $Header: /include/kernel/bitmap.h 1     7/03/04 17:07 Tim $ */
#ifndef __KERNEL_BITMAP_H
#define __KERNEL_BITMAP_H

#ifdef __cplusplus
extern "C"
{
#endif

void BitmapSetBit(void *bitmap, unsigned index, bool value);
int BitmapFindClearRange(const void *bitmap, unsigned length, unsigned range_length);

#ifdef __cplusplus
}
#endif

#endif
