#include <os/os.h>
#include <errno.h>

int *__errno()
{
	return &thrGetInfo()->last_error;
}