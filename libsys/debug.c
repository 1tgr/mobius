/* $Id: debug.c,v 1.1 2002/12/21 09:50:26 pavlovskii Exp $ */

#include <string.h>

#include <os/rtl.h>
#include <os/defs.h>
#include <os/pe.h>

module_info_t *DbgFindModule(addr_t addr)
{
    process_info_t *proc_info;
    module_info_t *mod_info;

    proc_info = ProcGetProcessInfo();
    for (mod_info = proc_info->module_first; mod_info != NULL; mod_info = mod_info->next)
        if (mod_info->base <= addr &&
            mod_info->base + mod_info->length > addr)
            return mod_info;

    return NULL;
}

IMAGE_PE_HEADERS *DbgGetPeHeaders(addr_t base)
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_PE_HEADERS *pe;

    dos = (IMAGE_DOS_HEADER*) base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return NULL;

    pe = (IMAGE_PE_HEADERS*) (base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE)
        return NULL;

    return pe;
}

IMAGE_SECTION_HEADER *DbgFindSectionByName(addr_t base, const char *name)
{
    IMAGE_PE_HEADERS *pe;
    IMAGE_SECTION_HEADER *section;
    unsigned i;

    pe = DbgGetPeHeaders(base);
    if (pe == NULL)
        return NULL;

    section = IMAGE_FIRST_SECTION(pe);

    for (i = 0; pe->FileHeader.NumberOfSections; i++)
        if (strncmp(name, section[i].Name, sizeof(section[i].Name)) == 0)
            return section + i;

    return NULL;
}

typedef struct internal_nlist_t internal_nlist_t;
struct internal_nlist_t
{
    unsigned long n_strx;         /* index into string table of name */
    unsigned char n_type;         /* type of symbol */
    unsigned char n_other;        /* misc info (usually empty) */
    unsigned short n_desc;        /* description field */
    uint32_t n_value;             /* value of symbol */
};

bool DbgLookupLineNumber(addr_t addr, 
                         char **path, 
                         char **file, 
                         unsigned *line)
{
    IMAGE_PE_HEADERS *pe;
    IMAGE_SECTION_HEADER *stab, *stabstr;
    module_info_t *info;
    internal_nlist_t *nlist;
    char *str, *temp_path, *temp_file, *the_path, *the_file;
    unsigned num_stabs, i, the_index, diff;
    uint32_t func_start, the_addr;

    info = DbgFindModule(addr);
    if (info == NULL)
        return false;

    pe = DbgGetPeHeaders(info->base);
    if (pe == NULL)
        return false;

    stab = DbgFindSectionByName(info->base, ".stab");
    stabstr = DbgFindSectionByName(info->base, ".stabstr");
    if (stab == NULL || stabstr == NULL)
        return false;

    num_stabs = stab->SizeOfRawData / sizeof(internal_nlist_t);
    nlist = (internal_nlist_t*) (info->base + stab->VirtualAddress);
    str = (char*) (info->base + stabstr->VirtualAddress);

    the_path = the_file = temp_path = temp_file = NULL;
    the_index = 0;
    diff = (unsigned) -1;
    for (i = 0; i < num_stabs; i++)
        if (nlist[i].n_type == 0x64)
        {
            /* Directory or file name */

            if (str[nlist[i].n_strx] == '\0')
                temp_path = temp_file = NULL;
            else if (temp_path == NULL)
            {
                temp_path = str + nlist[i].n_strx;
                func_start = 0;
            }
            else
            {
                temp_file = str + nlist[i].n_strx;
                /*func_start = nlist[i].n_value - 
                    pe->OptionalHeader.ImageBase + 
                    info->base;*/
            }
        }
        else if (nlist[i].n_type == 0x44)
        {
            /* Line number */
            if (temp_path != NULL && temp_file != NULL)
            {
                if (func_start + nlist[i].n_value <= addr &&
                    addr - (func_start + nlist[i].n_value) < diff)
                {
                    the_addr = func_start + nlist[i].n_value;
                    diff = addr - the_addr;
                    the_file = temp_file;
                    the_path = temp_path;
                    the_index = i;
                }
            }
        }
        else if (nlist[i].n_type == 0x24)
        {
            /* Funtion name */
            func_start = nlist[i].n_value - 
                pe->OptionalHeader.ImageBase + 
                info->base;
        }

    if (the_file != NULL && the_path != NULL && the_index > 0)
    {
        *path = the_path;
        *file = the_file;
        *line = nlist[the_index].n_desc;
        return true;
    }
    else
        return false;
}
