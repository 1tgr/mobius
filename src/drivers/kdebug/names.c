/* names.c   included source file defining instruction and register
 *           names for the Netwide [Dis]Assembler
 *
 * The Netwide Assembler is copyright (C) 1996 Simon Tatham and
 * Julian Hall. All rights reserved. The software is
 * redistributable under the licence given in the file "Licence"
 * distributed in the NASM archive.
 */

static wchar_t *reg_names[] = {	       /* register names, as strings */
    L"ah", L"al", L"ax", L"bh", L"bl", L"bp", L"bx", L"ch", L"cl",
    L"cr0", L"cr2", L"cr3", L"cr4", L"cs", L"cx", L"dh", L"di", L"dl", L"dr0",
    L"dr1", L"dr2", L"dr3", L"dr6", L"dr7", L"ds", L"dx", L"eax", L"ebp",
    L"ebx", L"ecx", L"edi", L"edx", L"es", L"esi", L"esp", L"fs", L"gs",
    L"mm0", L"mm1", L"mm2", L"mm3", L"mm4", L"mm5", L"mm6", L"mm7", L"si",
    L"sp", L"ss", L"st0", L"st1", L"st2", L"st3", L"st4", L"st5", L"st6",
    L"st7", L"tr3", L"tr4", L"tr5", L"tr6", L"tr7",
    L"xmm0", L"xmm1", L"xmm2", L"xmm3", L"xmm4", L"xmm5", L"xmm6", L"xmm7"
};

static wchar_t *conditions[] = {	       /* condition code names */
    L"a", L"ae", L"b", L"be", L"c", L"e", L"g", L"ge", L"l", L"le", L"na", L"nae",
    L"nb", L"nbe", L"nc", L"ne", L"ng", L"nge", L"nl", L"nle", L"no", L"np",
    L"ns", L"nz", L"o", L"p", L"pe", L"po", L"s", L"z"
};

/* Instruction names automatically generated from insns.dat */
#include "insnsn.c"
