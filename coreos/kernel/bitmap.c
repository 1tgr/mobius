/*
 * $History: bitmap.c $
 * 
 * *****************  Version 2  *****************
 * User: Tim          Date: 8/03/04    Time: 20:28
 * Updated in $/coreos/kernel
 * Fixed off-by-one error when allocating last bit
 */

#include <kernel/kernel.h>
#include <kernel/bitmap.h>

void BitmapSetBit(void *bitmap, 
                  unsigned index, 
                  bool value)
{
    static const uint8_t masks[2][8] =
    {
        { 0xfe, 0xfd, 0xfb, 0xf7, 0xef, 0xdf, 0xbf, 0x7f },
        { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 }
    };

    unsigned offset;

    offset = index / 8;
    if (value)
        ((uint8_t*) bitmap)[offset] |= masks[value][index % 8];
    else
        ((uint8_t*) bitmap)[offset] &= masks[value][index % 8];
}


int BitmapFindClearRange(const void *bitmap, 
                         unsigned length, 
                         unsigned range_length)
{
    const uint8_t *bitmap_ptr;
    uint8_t mask;
    unsigned range_start;

    range_start = 0;
    mask = 1;
    bitmap_ptr = (const uint8_t*) bitmap;
    while (range_start <= length - range_length)
    {
        if ((*bitmap_ptr & mask) == 0)
        {
            unsigned length_clear, index;

            length_clear = 0;
            index = range_start;
            while (length_clear < range_length &&
                (*bitmap_ptr & mask) == 0)
            {
                length_clear++;
                mask <<= 1;
                index++;
                if (mask == 0)
                {
                    mask = 1;
                    bitmap_ptr++;
                }
            }

            if (length_clear >= range_length)
                return range_start;

            range_start = index;
        }
        else
        {
            mask <<= 1;
            range_start++;
            if (mask == 0)
            {
                mask = 1;
                bitmap_ptr++;
            }
        }
    }

    return -1;
}
