# Microsoft Developer Studio Project File - Name="libsys" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=libsys - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libsys.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libsys.mak" CFG="libsys - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libsys - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "libsys - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "libsys - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f libsys.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "libsys.exe"
# PROP BASE Bsc_Name "libsys.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "NMAKE /f libsys.mak"
# PROP Rebuild_Opt "/a"
# PROP Target_File "libsys.exe"
# PROP Bsc_Name "libsys.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "libsys - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f libsys.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "libsys.exe"
# PROP BASE Bsc_Name "libsys.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "NMAKE /f libsys.mak"
# PROP Rebuild_Opt "/a"
# PROP Target_File "libsys.exe"
# PROP Bsc_Name "libsys.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "libsys - Win32 Release"
# Name "libsys - Win32 Debug"

!IF  "$(CFG)" == "libsys - Win32 Release"

!ELSEIF  "$(CFG)" == "libsys - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\dllmain.c
# End Source File
# Begin Source File

SOURCE=.\libsys.def
# End Source File
# Begin Source File

SOURCE=.\misc.c
# End Source File
# Begin Source File

SOURCE=.\port.c
# End Source File
# Begin Source File

SOURCE=.\syscall.S
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "os"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\os\defs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\device.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\pe.h
# End Source File
# Begin Source File

SOURCE=..\..\..\include\os\port.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\rtl.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\syscall.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\sysdef.h
# End Source File
# End Group
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\Makefile
# End Source File
# End Target
# End Project
