#include <bios.h>
#include "mobel.h"

#define	MAX_CACHE	32	/* 32 sectors == 16K */
#define	BPS			512	/* bytes per sector */

typedef struct
{
	sector_t sector;
	unsigned char blk[BPS];
} cache_t;
/*****************************************************************************
*****************************************************************************/
int read_sector(dev_t *dev, sector_t sector, unsigned char **blk)
{
	static unsigned char init, evict;
	static cache_t cache[MAX_CACHE];
/* */
	unsigned short c, h, s, temp;
	unsigned char tries;

	if(!init)
	{
		init = 1;
		for(temp = 0; temp < MAX_CACHE; temp++)
			cache[temp].sector = -1uL;
	}
/* see if this sector is cached */
	for(temp = 0; temp < MAX_CACHE; temp++)
	{
		if(cache[temp].sector == sector)
		{
			(*blk) = cache[temp].blk;
			return 0;
		}
	}
/* not cached, find a free buffer for it */
	for(temp = 0; temp < MAX_CACHE; temp++)
	{
		if(cache[temp].sector == -1uL)
			break;
	}
/* no free buffer, kick out someone else */
	if(temp >= MAX_CACHE)
	{
		temp = evict;
		evict++;
		if(evict >= MAX_CACHE)
			evict = 0;
	}
/* load it */
	cache[temp].sector = sector;
	(*blk) = cache[temp].blk;
/* we can load sector 0 even if we don't know the disk geometry
(which is good, because FAT uses sector 0 to store floppy geometry info) */
	if(sector == 0)
	{
		s = 1;
		h = 0;
		c = 0;
	}
	else
	{
		s = sector % dev->sects + 1;
		h = (sector / dev->sects) % dev->heads;
		c = (sector / dev->sects) / dev->heads;
	}
/* make 3 attempts */
	for(tries = 3; tries != 0; tries--)
	{
		temp = biosdisk(/* _DISK_READ */2, dev->int13_dev,
			h, c, s, 1, *blk);
/* biosdisk() does not return what the Turbo C online help says.
It returns the AH value from INT 13h, not AX
		temp >>= 8; */
		if(temp == 0 || temp == 0x11)
			return 0;
/* reset FDC if error */
		(void)biosdisk(/* _DISK_RESET */0, dev->int13_dev,
			0, 0, 0, 0, 0);
	}
	cprintf("\n\rread_sector: INT 13h disk error 0x%02X, CHS=%u:%u:%u, "
		"dev=0x%02X, *blk=%p\n\r", temp, c, h, s,
		dev->int13_dev, *blk);
	return ERR_IO;
}
