/* $Id$ */
#include <stdio.h>
#include <wchar.h>
#include <stddef.h>

#include "common.h"

void ShCmdHelp(const wchar_t *command, wchar_t *params)
{
    wchar_t text[1024];
    unsigned i, j;
    bool onlyOnce;

    if (*params == '\0')
    {
        onlyOnce = false;
        printf("Valid commands are:\n");
        for (i = 0; sh_commands[i].name != NULL; i++)
        {
            for (j = wcslen(sh_commands[i].name); j < 10; j++)
                printf(" ");
            _cputws(sh_commands[i].name, wcslen(sh_commands[i].name));
            //if (((i + 1) % 8) == 0)
                //printf("\n");
        }

        printf("\n\nValid file actions are:\n");
        for (i = 0; sh_actions[i].name != NULL; i++)
        {
            for (j = wcslen(sh_actions[i].name); j < 10; j++)
                printf(" ");
            _cputws(sh_actions[i].name, wcslen(sh_actions[i].name));
            //if (((i + 1) % 8) == 0)
                //printf("\n");
        }
    }
    else
        onlyOnce = true;

    while (1)
    {
        params = ShPrompt(L"\n Command? ", params);
        if (*params == '\0')
            break;

        for (i = 0; sh_commands[i].name != NULL; i++)
        {
            if (_wcsicmp(params, sh_commands[i].name) == 0)
            {
                if (!ResLoadString(NULL, sh_commands[i].help_id, text, _countof(text)) ||
                    text[0] == '\0')
                    swprintf(text, L"no string for ID %u", sh_commands[i].help_id);
                wprintf(L"%s: %s", sh_commands[i].name, text);
                break;
            }
        }

        if (sh_commands[i].name == NULL)
            printf("%S: unrecognized command\n", params);
        
        if (onlyOnce)
        {
            printf("\n");
            break;
        }

        params = L"";
    }
}
