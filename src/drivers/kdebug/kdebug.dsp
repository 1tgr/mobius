# Microsoft Developer Studio Project File - Name="kdebug" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=kdebug - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "kdebug.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "kdebug.mak" CFG="kdebug - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kdebug - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "kdebug - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "kdebug - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f kdebug.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "kdebug.exe"
# PROP BASE Bsc_Name "kdebug.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "make"
# PROP Rebuild_Opt "rebuild"
# PROP Target_File "..\..\..\bin\kdebug.dll"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "kdebug - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "kdebug___Win32_Debug"
# PROP BASE Intermediate_Dir "kdebug___Win32_Debug"
# PROP BASE Cmd_Line "NMAKE /f kdebug.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "kdebug.exe"
# PROP BASE Bsc_Name "kdebug.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "kdebug___Win32_Debug"
# PROP Intermediate_Dir "kdebug___Win32_Debug"
# PROP Cmd_Line "set"
# PROP Rebuild_Opt "rebuild"
# PROP Target_File "..\..\..\bin\kdebug.dll"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "kdebug - Win32 Release"
# Name "kdebug - Win32 Debug"

!IF  "$(CFG)" == "kdebug - Win32 Release"

!ELSEIF  "$(CFG)" == "kdebug - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\disasm.c
# End Source File
# Begin Source File

SOURCE=.\insnsd.c
# End Source File
# Begin Source File

SOURCE=.\kdebug.c
# End Source File
# Begin Source File

SOURCE=.\kdebug.def
# End Source File
# Begin Source File

SOURCE=.\sync.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\include\os\coff.h
# End Source File
# Begin Source File

SOURCE=.\disasm.h
# End Source File
# Begin Source File

SOURCE=.\insns.h
# End Source File
# Begin Source File

SOURCE=.\insnsi.h
# End Source File
# Begin Source File

SOURCE=.\nasm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\os\pe.h
# End Source File
# Begin Source File

SOURCE=.\sync.h
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
