#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int main( int argc, char **argv ) {

  FILE *boot;
  unsigned int i,j = 0;
  unsigned char *buffer;
  long len;
  char *name, *dot;


  if( argc != 2 ) {
    fprintf(stderr,"Boot2h by EKS - Dave Poirier\nUsage: boot2h /path/to/boot/record > boot.h\n");
    return 0;
  }

  boot = fopen(argv[1],"rb");
  if( !boot ) {
    fprintf(stderr,"Unable to open specified boot record: %s\n",argv[1]);
    return 0;
  }

  fseek(boot, 0, SEEK_END);
  len = ftell(boot);
  fseek(boot, 0, SEEK_SET);

  buffer = malloc(len);
  if (buffer == NULL)
    return 1;

  if( fread(buffer,len,1,boot) != 1 ) {
    fprintf(stderr,"Warning: reading %u bytes failed!", len);
  }
  fclose(boot);

  if( buffer[510] != 0x55 || buffer[511] != 0xAA ) {
    fprintf(stderr,"Warning: boot signature not found!");
  }

  name = strrchr(argv[1], '\\');
  if (name == NULL)
	  name = argv[1];
  else
	  name++;

  dot = strrchr(name, '.');
  if (dot != NULL)
	  *dot = '\0';

  printf("/* Generated using boot2h */\nunsigned char %s[%u] = {\n", name, len);
  for(i=0; i < len - 1; i++)
  {
    printf("0x%02X,",buffer[i]);
    j++;
    if( j == 10 ) {
      j = 0; printf("\n");
    }
  }
  printf("0x%02X };\n",buffer[len - 1]);
  free(buffer);
  return 0;
}
