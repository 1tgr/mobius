# Microsoft Developer Studio Project File - Name="libc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=libc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libc.mak" CFG="libc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libc - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "libc - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "libc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f libc.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "libc.exe"
# PROP BASE Bsc_Name "libc.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "NMAKE /f libc.mak"
# PROP Rebuild_Opt "/a"
# PROP Target_File "libc.exe"
# PROP Bsc_Name "libc.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "libc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f libc.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "libc.exe"
# PROP BASE Bsc_Name "libc.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "NMAKE /f libc.mak"
# PROP Rebuild_Opt "/a"
# PROP Target_File "libc.exe"
# PROP Bsc_Name "libc.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "libc - Win32 Release"
# Name "libc - Win32 Debug"

!IF  "$(CFG)" == "libc - Win32 Release"

!ELSEIF  "$(CFG)" == "libc - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "assert"

# PROP Default_Filter ""
# End Group
# Begin Group "ctype"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ctype\ct_flags.c
# End Source File
# Begin Source File

SOURCE=.\ctype\ct_lower.c
# End Source File
# Begin Source File

SOURCE=.\ctype\ct_upper.c
# End Source File
# Begin Source File

SOURCE=.\ctype\isalnum.c
# End Source File
# Begin Source File

SOURCE=.\ctype\isalpha.c
# End Source File
# Begin Source File

SOURCE=.\ctype\isascii.c
# End Source File
# Begin Source File

SOURCE=.\ctype\iscntrl.c
# End Source File
# Begin Source File

SOURCE=.\ctype\isdigit.c
# End Source File
# Begin Source File

SOURCE=.\ctype\isgraph.c
# End Source File
# Begin Source File

SOURCE=.\ctype\islower.c
# End Source File
# Begin Source File

SOURCE=.\ctype\isprint.c
# End Source File
# Begin Source File

SOURCE=.\ctype\ispunct.c
# End Source File
# Begin Source File

SOURCE=.\ctype\isspace.c
# End Source File
# Begin Source File

SOURCE=.\ctype\isupper.c
# End Source File
# Begin Source File

SOURCE=.\ctype\iswlower.c
# End Source File
# Begin Source File

SOURCE=.\ctype\iswupper.c
# End Source File
# Begin Source File

SOURCE=.\ctype\isxdigit.c
# End Source File
# Begin Source File

SOURCE=.\ctype\toascii.c
# End Source File
# Begin Source File

SOURCE=.\ctype\tolower.c
# End Source File
# Begin Source File

SOURCE=.\ctype\toupper.c
# End Source File
# Begin Source File

SOURCE=.\ctype\towupper.c
# End Source File
# End Group
# Begin Group "errno"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\errno\errno.c
# End Source File
# End Group
# Begin Group "locale"

# PROP Default_Filter ""
# End Group
# Begin Group "math"

# PROP Default_Filter ""
# End Group
# Begin Group "setjmp"

# PROP Default_Filter ""
# End Group
# Begin Group "stdarg"

# PROP Default_Filter ""
# End Group
# Begin Group "stdio"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\stdio\allocfil.c
# End Source File
# Begin Source File

SOURCE=.\stdio\clearerr.c
# End Source File
# Begin Source File

SOURCE=.\stdio\doprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\doscan.c
# End Source File
# Begin Source File

SOURCE=.\stdio\dowprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fclose.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fdopen.c
# End Source File
# Begin Source File

SOURCE=.\stdio\feof.c
# End Source File
# Begin Source File

SOURCE=.\stdio\ferror.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fflush.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fgetc.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fgetpos.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fgets.c
# End Source File
# Begin Source File

SOURCE=.\stdio\filbuf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\flsbuf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fopen.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fputc.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fputs.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fread.c
# End Source File
# Begin Source File

SOURCE=.\stdio\freopen.c
# End Source File
# Begin Source File

SOURCE=.\stdio\frlist.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fscanf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fseek.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fsetpos.c
# End Source File
# Begin Source File

SOURCE=.\stdio\ftell.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fwalk.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fwprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\fwrite.c
# End Source File
# Begin Source File

SOURCE=.\stdio\getc.c
# End Source File
# Begin Source File

SOURCE=.\stdio\getchar.c
# End Source File
# Begin Source File

SOURCE=.\stdio\gets.c
# End Source File
# Begin Source File

SOURCE=.\stdio\getw.c
# End Source File
# Begin Source File

SOURCE=.\stdio\perror.c
# End Source File
# Begin Source File

SOURCE=.\stdio\printf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\putc.c
# End Source File
# Begin Source File

SOURCE=.\stdio\putchar.c
# End Source File
# Begin Source File

SOURCE=.\stdio\puts.c
# End Source File
# Begin Source File

SOURCE=.\stdio\putw.c
# End Source File
# Begin Source File

SOURCE=.\stdio\pwerror.c
# End Source File
# Begin Source File

SOURCE=.\stdio\remove.c
# End Source File
# Begin Source File

SOURCE=.\stdio\rename.c
# End Source File
# Begin Source File

SOURCE=.\stdio\rewind.c
# End Source File
# Begin Source File

SOURCE=.\stdio\scanf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\setbuf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\setbuffe.c
# End Source File
# Begin Source File

SOURCE=.\stdio\setlineb.c
# End Source File
# Begin Source File

SOURCE=.\stdio\setvbuf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\sprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\sscanf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\stdaux.c
# End Source File
# Begin Source File

SOURCE=.\stdio\stderr.c
# End Source File
# Begin Source File

SOURCE=.\stdio\stdin.c
# End Source File
# Begin Source File

SOURCE=.\stdio\stdio.c
# End Source File
# Begin Source File

SOURCE=.\stdio\stdiohk.c
# End Source File
# Begin Source File

SOURCE=.\stdio\stdout.c
# End Source File
# Begin Source File

SOURCE=.\stdio\stdprn.c
# End Source File
# Begin Source File

SOURCE=.\stdio\swprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\tmpfile.c
# End Source File
# Begin Source File

SOURCE=.\stdio\tmpnam.c
# End Source File
# Begin Source File

SOURCE=.\stdio\ungetc.c
# End Source File
# Begin Source File

SOURCE=.\stdio\vfprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\vprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\vsprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\wfopen.c
# End Source File
# Begin Source File

SOURCE=.\stdio\wprintf.c
# End Source File
# End Group
# Begin Group "stdlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\stdlib\abort.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\abs.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\atexit.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\atof.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\atoi.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\atol.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\atold.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\bsdqsort.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\bsearch.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\cppsup.cpp
# End Source File
# Begin Source File

SOURCE=.\stdlib\div.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\exit.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\labs.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\ldiv.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\llabs.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\lldiv.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\qsort.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\rand.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\strtod.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\strtol.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\strtold.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\strtoll.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\strtoul.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\strtoull.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\wcstol.c
# End Source File
# End Group
# Begin Group "string"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\string\djmd.S
# End Source File
# Begin Source File

SOURCE=.\string\djmdr.S
# End Source File
# Begin Source File

SOURCE=.\string\mbstowcs.c
# End Source File
# Begin Source File

SOURCE=.\string\memchr.c
# End Source File
# Begin Source File

SOURCE=.\string\memcmp.c
# End Source File
# Begin Source File

SOURCE=.\string\memcpy.S
# End Source File
# Begin Source File

SOURCE=.\string\memmove.S
# End Source File
# Begin Source File

SOURCE=.\string\memset.S
# End Source File
# Begin Source File

SOURCE=.\string\strcat.c
# End Source File
# Begin Source File

SOURCE=.\string\strchr.c
# End Source File
# Begin Source File

SOURCE=.\string\strcmp.c
# End Source File
# Begin Source File

SOURCE=.\string\strcoll.c
# End Source File
# Begin Source File

SOURCE=.\string\strcpy.c
# End Source File
# Begin Source File

SOURCE=.\string\strcspn.c
# End Source File
# Begin Source File

SOURCE=.\string\strerror.c
# End Source File
# Begin Source File

SOURCE=.\string\stricmp.c
# End Source File
# Begin Source File

SOURCE=.\string\strlen.c
# End Source File
# Begin Source File

SOURCE=.\string\strncat.c
# End Source File
# Begin Source File

SOURCE=.\string\strncmp.c
# End Source File
# Begin Source File

SOURCE=.\string\strncpy.c
# End Source File
# Begin Source File

SOURCE=.\string\strpbrk.c
# End Source File
# Begin Source File

SOURCE=.\string\strrchr.c
# End Source File
# Begin Source File

SOURCE=.\string\strspn.c
# End Source File
# Begin Source File

SOURCE=.\string\strstr.c
# End Source File
# Begin Source File

SOURCE=.\string\strtok.c
# End Source File
# Begin Source File

SOURCE=.\string\strxfrm.c
# End Source File
# Begin Source File

SOURCE=.\string\syserr1.c
# End Source File
# Begin Source File

SOURCE=.\string\syserr1.h
# End Source File
# Begin Source File

SOURCE=.\string\syserr2.c
# End Source File
# Begin Source File

SOURCE=.\string\syserr3.c
# End Source File
# Begin Source File

SOURCE=.\string\wcscat.c
# End Source File
# Begin Source File

SOURCE=.\string\wcschr.c
# End Source File
# Begin Source File

SOURCE=.\string\wcscmp.c
# End Source File
# Begin Source File

SOURCE=.\string\wcscpy.c
# End Source File
# Begin Source File

SOURCE=.\string\wcsdup.c
# End Source File
# Begin Source File

SOURCE=.\string\wcsicmp.c
# End Source File
# Begin Source File

SOURCE=.\string\wcslen.c
# End Source File
# Begin Source File

SOURCE=.\string\wcsmatch.c
# End Source File
# Begin Source File

SOURCE=.\string\wcsncmp.c
# End Source File
# Begin Source File

SOURCE=.\string\wcsncpy.c
# End Source File
# Begin Source File

SOURCE=.\string\wcsnicmp.c
# End Source File
# Begin Source File

SOURCE=.\string\wcspbrk.c
# End Source File
# Begin Source File

SOURCE=.\string\wcsrchr.c
# End Source File
# Begin Source File

SOURCE=.\string\wcstombs.c
# End Source File
# End Group
# Begin Group "time"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\time\localtime.c
# End Source File
# Begin Source File

SOURCE=.\time\time.c
# End Source File
# End Group
# Begin Group "sys"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\sys\access.c
# End Source File
# Begin Source File

SOURCE=.\sys\close.c
# End Source File
# Begin Source File

SOURCE=.\sys\cputs.c
# End Source File
# Begin Source File

SOURCE=.\sys\cputws.c
# End Source File
# Begin Source File

SOURCE=.\sys\creatnew.c
# End Source File
# Begin Source File

SOURCE=.\sys\fmode.c
# End Source File
# Begin Source File

SOURCE=.\sys\getenv.c
# End Source File
# Begin Source File

SOURCE=.\sys\int64.c
# End Source File
# Begin Source File

SOURCE=.\sys\isatty.c
# End Source File
# Begin Source File

SOURCE=.\sys\lseek.c
# End Source File
# Begin Source File

SOURCE=.\sys\main.c
# End Source File
# Begin Source File

SOURCE=.\sys\morecore.c
# End Source File
# Begin Source File

SOURCE=.\sys\open.c
# End Source File
# Begin Source File

SOURCE=.\sys\read.c
# End Source File
# Begin Source File

SOURCE=.\sys\sbrk.c
# End Source File
# Begin Source File

SOURCE=.\sys\towc.c
# End Source File
# Begin Source File

SOURCE=.\sys\ucfile.c
# End Source File
# Begin Source File

SOURCE=.\sys\unicode.c
# End Source File
# Begin Source File

SOURCE=.\sys\write.c
# End Source File
# End Group
# Begin Group "malloc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=".\malloc-1.18\_emalloc.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\_malloc.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\_memalign.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\_strdup.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\_strsave.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\align.h"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\assert.h"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\botch.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\defs.h"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\dumpheap.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\emalloc.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\externs.h"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\getmem.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\globals.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\globals.h"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\globrename.h"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\leak.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\malloc.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\malloc.h"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\memalign.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\setopts.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\stats.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\strdup.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\strsave.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\trace.h"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\verify.c"
# End Source File
# Begin Source File

SOURCE=".\malloc-1.18\version.c"
# End Source File
# End Group
# Begin Source File

SOURCE=.\libc.def
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "sys (h)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\sys\stat.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sys\types.h
# End Source File
# Begin Source File

SOURCE=..\..\include\sys\version.h
# End Source File
# End Group
# Begin Group "libc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\libc\file.h
# End Source File
# Begin Source File

SOURCE=..\..\include\libc\local.h
# End Source File
# Begin Source File

SOURCE=..\..\include\libc\stdiohk.h
# End Source File
# Begin Source File

SOURCE=..\..\include\libc\stubs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\libc\unconst.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\include\assert.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ctype.h
# End Source File
# Begin Source File

SOURCE=..\..\include\errno.h
# End Source File
# Begin Source File

SOURCE=..\..\include\fcntl.h
# End Source File
# Begin Source File

SOURCE=..\..\include\float.h
# End Source File
# Begin Source File

SOURCE=..\..\include\io.h
# End Source File
# Begin Source File

SOURCE=..\..\include\limits.h
# End Source File
# Begin Source File

SOURCE=..\..\include\locale.h
# End Source File
# Begin Source File

SOURCE=..\..\include\math.h
# End Source File
# Begin Source File

SOURCE=..\..\include\printf.h
# End Source File
# Begin Source File

SOURCE=..\..\include\signal.h
# End Source File
# Begin Source File

SOURCE=..\..\include\stdarg.h
# End Source File
# Begin Source File

SOURCE=..\..\include\stddef.h
# End Source File
# Begin Source File

SOURCE=..\..\include\stdio.h
# End Source File
# Begin Source File

SOURCE=..\..\include\stdlib.h
# End Source File
# Begin Source File

SOURCE=..\..\include\string.h
# End Source File
# Begin Source File

SOURCE=..\..\include\time.h
# End Source File
# Begin Source File

SOURCE=..\..\include\unistd.h
# End Source File
# Begin Source File

SOURCE=..\..\include\wchar.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\Makefile
# End Source File
# End Target
# End Project
