/*
 * Demo of fdc functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <bios.h>   /* for biostime() */
#include <crt0.h>
#include "fdc.h"

#define FORMAT

/* ensure all mem is locked */
int _crt0_startup_flags = _CRT0_FLAG_LOCK_MEMORY;

/* test functions */
int main(void)
{
   int block,i,c,track;
   BYTE trackbuff[512];
   DrvGeom geometry;

   for (i = 0;i < 512;i++) trackbuff[i] = 0;
   
   init();
   atexit(deinit);
   
   puts("insert a HD stiffy that has nothing of value in it and press enter");
   getchar();
   
#ifdef FORMAT

   log_disk(NULL);
   
   geometry.heads = DG168_HEADS;
   geometry.tracks = DG168_TRACKS;
   geometry.spt = DG168_SPT;
   
   /* format disk */
   for (i = 0;i < geometry.tracks;i++) {
      if (!format_track(i,&geometry)) {
	 if (diskchange())
	   printf("diskchange - abort!\n");
	 else
	   printf("\nerror!\n");
	 return 1;
      }
      fprintf(stderr,"formatted track %d\r",i);
   }
   
#endif
   
   if (!log_disk(&geometry)) {
      printf("cannot read geometry!\n");
      exit(1);
   }

   if (geometry.spt == DG144_SPT)
     printf("1.44M format\n");
   else
     printf("1.68M format\n");
   
   /* write block */
   for (block = 0;block < geometry.spt;block++) {
      sprintf(trackbuff,"block number %d",block);

      if (!write_block(block,trackbuff)) {
	 if (diskchange())
	   printf("diskchange - abort!\n");
	 else
	   printf("error writing!\n");
	 return 1;
      }
   }
   
   /* read block */
   for (block = 0;block < geometry.spt;block++) {
      strcpy(trackbuff,"************");
      if (!read_block(block,trackbuff)) {
	 if (diskchange())
	   printf("diskchange - abort!\n");
	 else
	   printf("error reading!\n");
	 return 1;
      }

      /* display block (1st 16 bytes) */
      for (i = 0;i < 16;i++)
	printf("%02x ",trackbuff[i]);
      
      printf(": ");
      
      for (i = 0;i < 16;i++) {
	 c = trackbuff[i];
	 printf("%c",isprint(c) ? c : '.');
      }
      
      printf("\n");
   }

   srand(biostime(0,0));
   
   /* seek a few times */
   for (i = 0;i < 10;i++) {
      track = rand() % 80;
      printf("seeking to %d: ",track);
      if (seek(track))
	printf("OK\n");
      else
	printf("error!\n");
   }

   printf("All done - press enter to finish\n");
   getchar();
   
   return 0;
}
