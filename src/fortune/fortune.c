#define INITGUID
#include <os/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

int main(int argc, char** argv)
{
	IStream* file = (IStream*) fsOpen(L"/boot/fortunes", &IID_IStream);
	folderitem_t item;
	dword location, pos;
	char buf[1024], *ch, ch1;

	if (!file)
	{
		wprintf(L"Failed to open boot/fortunes\n");
		return 1;
	}

	item.size = sizeof(item);
	item.name = NULL;
	item.name_max = 0;
	IStream_Stat(file, &item);

	srand(sysUpTime());
	location = rand() % item.length;
	//wprintf(L"File is %u bytes long; location = %u\n", item.length, location);

	for (pos = 0; pos < location; )
	{
		ch = buf;

		while (IStream_Read(file, &ch1, sizeof(ch1)) &&
			ch1 != '%' &&
			ch - buf < countof(buf))
		{
			*ch = ch1;
			ch++;
		}

		*ch = '\0';
		pos += strlen(buf);
	}

	wprintf(L"%S", buf);
	IUnknown_Release(file);
	return 0;
}