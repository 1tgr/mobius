#define INITGUID
#include <os/os.h>
#include <stdio.h>

int main(int argc, char** argv)
{
	IStream* file;
	char buf[1024];
	size_t read, i;
	bool is_tty;
	folderitem_t stat;

	file = (IStream*) fsOpen(procCmdLine(), &IID_IStream);
	if (!file)
	{
		wprintf(L"%s: failed to open\n", procCmdLine());
		return 1;
	}
	
	stat.size = sizeof(stat);
	is_tty = FAILED(IStream_Stat(file, &stat)) ||
		(stat.attributes & ATTR_DEVICE);
	if (is_tty)
		_cputws(L"It's a character device\n");
	else
		_cputws(L"It's a file\n");
		
	do
	{
		read = IStream_Read(file, buf, is_tty ? 1 : sizeof(buf));
		for (i = 0; i < read; i++)
			putwchar(buf[i]);
	} while (read);

	putwchar('\n');
	IUnknown_Release(file);
	return 0;
}