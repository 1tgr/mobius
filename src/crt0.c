/* $Id: crt0.c,v 1.2 2001/11/05 18:45:23 pavlovskii Exp $ */

void exit(int code);
int main(void);

void mainCRTStartup(void)
{
	exit(main());
}