/* $Id$ */
#include <wchar.h>

#include "common.h"

void ShCmdDismount(const wchar_t *cmd, wchar_t *params)
{
	params = ShPrompt(L" Path? ", params);
    if (*params == '\0')
        return;

	if (!FsDismount(params))
		_pwerror(params);
}
