/* $Id: file.c,v 1.1 2002/08/17 22:52:14 pavlovskii Exp $ */

#include "common.h"

void EdClearFile(void)
{
    unsigned i;

    for (i = 0; i < ed_num_lines; i++)
        free(ed_lines[i]);
    
    free(ed_lines);
    ed_num_lines = 0;
}

void EdNewFile(void)
{
    EdClearFile();
    ed_num_lines = 1;
    ed_lines = malloc(sizeof(ed_line_t*));
    ed_lines[0] = malloc(sizeof(ed_line_t) + 15);
    ed_lines[0]->length = 5;
    ed_lines[0]->length_alloc = 16;
    strcpy(ed_lines[0]->text, "Hello");
    ed_row = ed_col = 0;
}

bool EdLoadFile(const wchar_t *name)
{
    FILE *file;
    char buffer[1025], *newline, *start_of_line;
    size_t bytes;
    ed_line_t *line;

    file = _wfopen(name, L"rt");
    if (file == NULL)
    {
        _pwerror(name);
        return false;
    }

    EdClearFile();

    while ((bytes = fread(buffer, 1, sizeof(buffer) - 1, file)) != 0)
    {
        buffer[bytes] = '\0';
        start_of_line = buffer;

        while (start_of_line < buffer + bytes)
        {
            newline = strchr(start_of_line, '\n');
            if (newline == NULL)
                newline = buffer + bytes;
            *newline = '\0';
            line = malloc(sizeof(ed_line_t*) + newline - start_of_line + 1);
            line->length = newline - start_of_line;
            line->length_alloc = line->length + 1;
            strcpy(line->text, start_of_line);
            ed_lines = realloc(ed_lines, sizeof(ed_line_t*) * (ed_num_lines + 1));
            ed_lines[ed_num_lines++] = line;
            start_of_line = newline + 1;
        }
    }

    ed_row = ed_col = 0;
    fclose(file);
    return true;
}
