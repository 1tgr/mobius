# Microsoft Developer Studio Project File - Name="winmgr" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=winmgr - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "winmgr.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "winmgr.mak" CFG="winmgr - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "winmgr - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "winmgr - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "winmgr - Win32 Release"

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
# ADD CPP /nologo /MT /W3 /O2 /D "NDEBUG" /D "__MOBIUS__" /D "KERNEL" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel.lib ..\crt0dll.obj /nologo /base:"@..\..\bin\coffbase.txt,winmgr" /subsystem:windows /dll /machine:I386 /nodefaultlib /implib:"..\..\lib/winmgr.lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "winmgr - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /ZI /Od /D "_DEBUG" /D "__MOBIUS__" /D "KERNEL" /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel.lib ..\crt0dll.obj /nologo /base:"@..\..\bin\coffbase.txt,winmgr" /subsystem:windows /dll /incremental:no /debug /debugtype:coff /machine:I386 /nodefaultlib /implib:"..\..\lib/winmgr.lib" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "winmgr - Win32 Release"
# Name "winmgr - Win32 Debug"
# Begin Group "Source"

# PROP Default_Filter ".c;.cpp;.rc"
# Begin Source File

SOURCE=.\ClipSurface.cpp
# End Source File
# Begin Source File

SOURCE=.\common.c
# End Source File
# Begin Source File

SOURCE=.\console.cpp
# End Source File
# Begin Source File

SOURCE=.\DesktopWnd.cpp
# End Source File
# Begin Source File

SOURCE=.\font.cpp
# End Source File
# Begin Source File

SOURCE=.\font1.c
# End Source File
# Begin Source File

SOURCE=.\font2.c
# End Source File
# Begin Source File

SOURCE=.\fontbfd.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\gfxcons.cpp
# End Source File
# Begin Source File

SOURCE=.\main.cpp
# End Source File
# Begin Source File

SOURCE=.\MsgQueue.cpp
# End Source File
# Begin Source File

SOURCE=.\ResFont.cpp
# End Source File
# Begin Source File

SOURCE=.\S3Graphics.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\surface.cpp
# End Source File
# Begin Source File

SOURCE=.\trio.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\trio2.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\vga8.cpp
# End Source File
# Begin Source File

SOURCE=.\Window.cpp
# End Source File
# Begin Source File

SOURCE=.\WindowServer.cpp
# End Source File
# Begin Source File

SOURCE=.\winframe.cpp
# End Source File
# Begin Source File

SOURCE=.\winmgr.rc
# End Source File
# End Group
# Begin Group "Headers"

# PROP Default_Filter ".h"
# Begin Group "gui"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\gui\font.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gui\surface.h
# End Source File
# Begin Source File

SOURCE=..\..\include\gui\winserve.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\ClipSurface.h
# End Source File
# Begin Source File

SOURCE=.\console.h
# End Source File
# Begin Source File

SOURCE=.\DesktopWnd.h
# End Source File
# Begin Source File

SOURCE=.\gfxcons.h
# End Source File
# Begin Source File

SOURCE=.\MsgQueue.h
# End Source File
# Begin Source File

SOURCE=.\palette.h
# End Source File
# Begin Source File

SOURCE=.\ResFont.h
# End Source File
# Begin Source File

SOURCE=.\S3Graphics.h
# End Source File
# Begin Source File

SOURCE=.\surface.h
# End Source File
# Begin Source File

SOURCE=.\Window.h
# End Source File
# Begin Source File

SOURCE=.\WindowServer.h
# End Source File
# Begin Source File

SOURCE=.\winframe.h
# End Source File
# End Group
# Begin Group "Resources"

# PROP Default_Filter ".fnt"
# Begin Source File

SOURCE=.\res\84.fnt
# End Source File
# Begin Source File

SOURCE=.\res\85.fnt
# End Source File
# Begin Source File

SOURCE=.\res\86.fnt
# End Source File
# Begin Source File

SOURCE=.\res\87.fnt
# End Source File
# Begin Source File

SOURCE=.\res\88.fnt
# End Source File
# Begin Source File

SOURCE=.\res\89.fnt
# End Source File
# End Group
# End Target
# End Project
