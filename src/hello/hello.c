#include <stdio.h>
#include <os/os.h>
#include <stdlib.h>

int main()
{
	wchar_t str[50], *whom;

	whom = procCmdLine();
	if (*whom == '\0')
		whom = L"world";

	if (resLoadString(0x40000000, 1, str, countof(str)))
		wprintf(str, whom);
	
	return 0;
}