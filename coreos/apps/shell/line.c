/* $Id: line.c,v 1.1.1.1 2002/12/21 09:51:09 pavlovskii Exp $ */

#include <os/syscall.h>
#include <os/rtl.h>
#include <os/keyboard.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>

#include "common.h"

shell_line_t *ShReadLine(void)
{
	handle_t hnd;
    uint32_t ch;
    size_t read, allocd;
    shell_line_t *line;
    wchar_t wc[2];
    char mb[5];
    int len;
    
    /*line = *ptr;
    if (line == NULL)*/
    {
        line = malloc(sizeof(shell_line_t));
        line->next = NULL;
        line->prev = sh_line_last;
        line->text = _wcsdup(L"");
        line->widths = _strdup("");
        
        if (sh_line_first == NULL)
        {
            line->num = 0;
            sh_line_first = line;
        }
        else
            line->num = sh_line_last->num + 1;

        if (sh_line_last != NULL)
            sh_line_last->next = line;

        sh_line_last = line;
    }
    
    read = wcslen(line->text);
    allocd = (read + 15) & -16;
    fflush(stdout);
	hnd = ProcGetProcessInfo()->std_in;

    while (true)
    {
        if (!FsRead(hnd, &ch, 0, sizeof(ch), NULL))
		{
			_wdprintf(L"ShReadLine: FsRead failed, %s\n", _wcserror(errno));
            break;
		}
        else if (ch & KBD_BUCKY_RELEASE)
            continue;

        switch (ch)
        {
        case KEY_UP:
            if (line->prev != NULL)
            {
                printf("\x1b[%uD\x1b[K", read);
                line = line->prev;
                read = wcslen(line->text);
                allocd = (read + 15) & -16;
                _cputws(line->text, read);
            }
            break;

        case KEY_DOWN:
            if (line->next != NULL)
            {
                printf("\x1b[%uD\x1b[K", read);
                line = line->next;
                read = wcslen(line->text);
                allocd = (read + 15) & -16;
                _cputws(line->text, read);
            }
            break;

        case '\n':
            line->text[read] = '\0';
            line->widths[read] = 0;
            
            if (line != sh_line_last)
            {
                free(sh_line_last->text);
                sh_line_last->text = _wcsdup(line->text);
                free(sh_line_last->widths);
                sh_line_last->widths = _strdup(line->widths);
                line = sh_line_last;
            }

            putchar('\n');
            fflush(stdout);
            return line;
            
        case '\b':
            if (read > 0)
            {
                read--;
                len = line->widths[read];
                printf("\x1b[%uD%*s\x1b[%uD", 
                    len, len, "", len);
            }
            break;

        default:
            if ((wchar_t) ch != 0)
            {
                if (ch & KBD_BUCKY_CTRL)
                {
                    ch = towupper((wchar_t) ch);
                    len = sprintf(mb, "^%c", (char) ch);
                    ch = ch - 'A' + 1;
                }
                else if (ch == '\t')
                {
                    strcpy(mb, "    ");
                    len = 4;
                }
                else
                {
                    wc[0] = ch;
                    wc[1] = '\0';
                    len = wcstombs(mb, wc, _countof(mb));
                    if (len == -1)
                        mb[0] = '\0';
                    else
                        mb[len] = '\0';
                    len = 1;
                }

                if (mb[0] != '\0')
                {
                    fwrite(mb, 1, strlen(mb), stdout);

                    if (read + 1 >= allocd)
                    {
                        allocd = (read + 16) & -16;
                        line->text = realloc(line->text, sizeof(wchar_t) * (allocd + 1));
                        line->widths = realloc(line->widths, allocd + 1);
                    }

                    line->widths[read] = len;
                    line->text[read] = ch;
                    read++;
                }
            }
            break;
        }

        fflush(stdout);
    }

    free(line->text);
    free(line);
    return NULL;
}

wchar_t *ShPrompt(const wchar_t *prompt, wchar_t *params)
{
    shell_line_t *line;
    if (*params != '\0')
        return params;
    else
    {
        _cputws(prompt, wcslen(prompt));
        fflush(stdout);
        line = ShReadLine();
        return line->text;
    }
}
