# Microsoft Developer Studio Project File - Name="libc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

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
!MESSAGE "libc - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libc - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\bin"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /X /I "d:\projects\mobius\include" /D "NDEBUG" /D "__MOBIUS__" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 /nologo /base:"@..\..\bin\coffbase.txt,libc" /subsystem:windows /dll /machine:I386 /nodefaultlib /def:".\libc.def" /implib:"..\..\lib/libc.lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "libc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\bin"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "_DEBUG" /D "__MOBIUS__" /YX /FD /c
# SUBTRACT CPP /X
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /base:"@..\..\bin\coffbase.txt,libc" /subsystem:windows /dll /incremental:no /debug /debugtype:coff /machine:I386 /nodefaultlib /def:".\libc.def" /implib:"..\..\lib/libc.lib" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "libc - Win32 Release"
# Name "libc - Win32 Debug"
# Begin Group "Source"

# PROP Default_Filter ".c"
# Begin Group "stdio"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\stdio\dowprintf.c
# End Source File
# Begin Source File

SOURCE=.\stdio\wprintf.c
# End Source File
# End Group
# Begin Group "string"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\string\iswlower.c
# End Source File
# Begin Source File

SOURCE=.\string\iswupper.c
# End Source File
# Begin Source File

SOURCE=.\string\match.c
# End Source File
# Begin Source File

SOURCE=.\string\mbstowcs.c
# End Source File
# Begin Source File

SOURCE=.\string\memcpy.c
# End Source File
# Begin Source File

SOURCE=.\string\memmove.c
# End Source File
# Begin Source File

SOURCE=.\string\memset.c
# End Source File
# Begin Source File

SOURCE=.\string\strcmp.c
# End Source File
# Begin Source File

SOURCE=.\string\stricmp.c
# End Source File
# Begin Source File

SOURCE=.\string\strlen.c
# End Source File
# Begin Source File

SOURCE=.\string\strncmp.c
# End Source File
# Begin Source File

SOURCE=.\string\strpbrk.c
# End Source File
# Begin Source File

SOURCE=.\stdio\swprintf.c
# End Source File
# Begin Source File

SOURCE=.\string\towlower.c
# End Source File
# Begin Source File

SOURCE=.\string\towupper.c
# End Source File
# Begin Source File

SOURCE=.\string\unicode.c
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

SOURCE=.\string\wcsncpy.c
# End Source File
# Begin Source File

SOURCE=.\string\wcspbrk.c
# End Source File
# Begin Source File

SOURCE=.\string\wcsrchr.c
# End Source File
# End Group
# Begin Group "malloc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\malloc\free.c
# End Source File
# Begin Source File

SOURCE=.\malloc\malloc.c
# End Source File
# Begin Source File

SOURCE=.\malloc\msize.c
# End Source File
# Begin Source File

SOURCE=.\malloc\realloc.c
# End Source File
# End Group
# Begin Group "sys"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\sys\atexit.c
# End Source File
# Begin Source File

SOURCE=.\sys\core.c
# End Source File
# Begin Source File

SOURCE=.\sys\cputws.c
# End Source File
# End Group
# Begin Group "conio"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\conio\kbhit.c
# End Source File
# Begin Source File

SOURCE=.\conio\wgetch.c
# End Source File
# Begin Source File

SOURCE=.\conio\wgetche.c
# End Source File
# End Group
# Begin Group "stdlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\stdlib\rand.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\srand.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\strtol.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\crt0\crt0pp.cpp
# End Source File
# Begin Source File

SOURCE=.\dllmain.c
# End Source File
# Begin Source File

SOURCE=.\stdlib\errno.c
# End Source File
# Begin Source File

SOURCE=.\ftol.asm

!IF  "$(CFG)" == "libc - Win32 Release"

# Begin Custom Build
InputPath=.\ftol.asm

"ftol.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	f:\drived\nasm\nasm ftol.asm -o ftol.obj -f win32

# End Custom Build

!ELSEIF  "$(CFG)" == "libc - Win32 Debug"

# Begin Custom Build
InputPath=.\ftol.asm

"ftol.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	f:\drived\nasm\nasm ftol.asm -o ftol.obj -f win32

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\init.c
# End Source File
# Begin Source File

SOURCE=.\libc.rc
# End Source File
# Begin Source File

SOURCE=.\stdio\pwerror.c
# End Source File
# Begin Source File

SOURCE=.\string\wcserror.c
# End Source File
# End Group
# Begin Group "Headers"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=..\..\include\conio.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ctype.h
# End Source File
# Begin Source File

SOURCE=..\..\include\errno.h
# End Source File
# Begin Source File

SOURCE=..\..\include\malloc.h
# End Source File
# Begin Source File

SOURCE=..\..\include\printf.h
# End Source File
# Begin Source File

SOURCE=..\..\include\stdarg.h
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

SOURCE=..\..\include\types.h
# End Source File
# Begin Source File

SOURCE=..\..\include\wchar.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\libc.def
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\listing.txt
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# Begin Source File

SOURCE=..\crt0dll.obj
# End Source File
# End Target
# End Project
