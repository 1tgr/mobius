# Microsoft Developer Studio Project File - Name="kernelu" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=kernelu - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "kernelu.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "kernelu.mak" CFG="kernelu - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kernelu - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "kernelu - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "kernelu - Win32 Release"

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
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "__MOBIUS__" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 /nologo /base:"@..\..\bin\coffbase.txt,kernelu" /subsystem:windows /dll /machine:I386 /nodefaultlib /def:".\kernelu.def" /implib:"..\..\lib/kernelu.lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "kernelu - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Ox /Ot /Oi /Ob2 /D "_DEBUG" /D "__MOBIUS__" /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /base:"@..\..\bin\coffbase.txt,kernelu" /subsystem:windows /dll /incremental:no /debug /debugtype:coff /machine:I386 /nodefaultlib /def:".\kernelu.def" /implib:"..\..\lib/kernelu.lib" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "kernelu - Win32 Release"
# Name "kernelu - Win32 Debug"
# Begin Group "Source"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\debug.c
# End Source File
# Begin Source File

SOURCE=.\device.c
# End Source File
# Begin Source File

SOURCE=.\except.c
# End Source File
# Begin Source File

SOURCE=.\fs.c
# End Source File
# Begin Source File

SOURCE=.\fullpath.c
# End Source File
# Begin Source File

SOURCE=.\main.c
# End Source File
# Begin Source File

SOURCE=.\obj.c
# End Source File
# Begin Source File

SOURCE=.\port.c
# End Source File
# Begin Source File

SOURCE=.\proc.c
# End Source File
# Begin Source File

SOURCE=.\resource.c
# End Source File
# Begin Source File

SOURCE=.\sys.c
# End Source File
# Begin Source File

SOURCE=.\thread.c
# End Source File
# Begin Source File

SOURCE=.\vmm.c
# End Source File
# End Group
# Begin Group "Headers"

# PROP Default_Filter ".h"
# Begin Group "os"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\os\coff.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\com.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\device.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\devreq.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\filesys.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\fs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\guids.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\keyboard.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\mouse.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\os.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\pe.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\serial.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\stream.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\include\os\port.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\kernelu.def
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\kernelu.rc
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# Begin Source File

SOURCE=..\crt0dll.obj
# End Source File
# End Target
# End Project
