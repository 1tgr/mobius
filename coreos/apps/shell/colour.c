/* $Id$ */
#include <stdio.h>
#include <wchar.h>
#include <stddef.h>

#include "common.h"

void ShCmdColour(const wchar_t *cmd, wchar_t *params)
{
	static const struct
	{
		const wchar_t *name;
		unsigned value;
	} colours[] = 
	{
		{ L"fgblack",	30 },
		{ L"fgred",		31 },
		{ L"fggreen",	32 },
		{ L"fgyellow",	33 },
		{ L"fgblue",	34 },
		{ L"fgmagenta",	35 },
		{ L"fgcyan",	36 },
		{ L"fgwhite",	37 },

		{ L"bgblack",	40 },
		{ L"bgred",		41 },
		{ L"bggreen",	42 },
		{ L"bgyellow",	43 },
		{ L"bgblue",	44 },
		{ L"bgmagenta",	45 },
		{ L"bgcyan",	46 },
		{ L"bgwhite",	47 },

		{ L"reset",		0 },
		{ L"bold",		1 },
		{ L"blink",		5 },
		{ L"reverse",	7 },
		{ L"conceal",	8 },
	};

	wchar_t *token;
	unsigned i;

	if (*params == '\0')
	{
		printf("Possible colours are:\n");
		for (i = 0; i < _countof(colours); i++)
			printf("%10S", colours[i].name);

		params = ShPrompt(L"\n Colours? ", params);
		if (*params == '\0')
			return;
	}

	token = _wcstok(params, L" ");
	while (token != NULL)
	{
		for (i = 0; i < _countof(colours); i++)
			if (_wcsicmp(token, colours[i].name) == 0)
			{
				printf("\x1b[%um", colours[i].value);
				break;
			}

		token = _wcstok(NULL, L" ");
	}

	fflush(stdout);
}
