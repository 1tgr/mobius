# Microsoft Developer Studio Project File - Name="kdll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=kdll - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "kdll.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "kdll.mak" CFG="kdll - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kdll - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "kdll - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "kdll - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "kdll___Win32_Release"
# PROP BASE Intermediate_Dir "kdll___Win32_Release"
# PROP BASE Cmd_Line "NMAKE /f kdll.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "kdll.exe"
# PROP BASE Bsc_Name "kdll.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "kdll___Win32_Release"
# PROP Intermediate_Dir "kdll___Win32_Release"
# PROP Cmd_Line "nmake /f "kdll.mak""
# PROP Rebuild_Opt "/a"
# PROP Target_File "kdll.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "kdll - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "kdll___Win32_Debug"
# PROP BASE Intermediate_Dir "kdll___Win32_Debug"
# PROP BASE Cmd_Line "NMAKE /f kdll.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "kdll.exe"
# PROP BASE Bsc_Name "kdll.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "kdll___Win32_Debug"
# PROP Intermediate_Dir "kdll___Win32_Debug"
# PROP Cmd_Line "set"
# PROP Rebuild_Opt "rebuild"
# PROP Target_File "..\..\bin\kdll.dll"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "kdll - Win32 Release"
# Name "kdll - Win32 Debug"

!IF  "$(CFG)" == "kdll - Win32 Release"

!ELSEIF  "$(CFG)" == "kdll - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\common.c
# End Source File
# Begin Source File

SOURCE=.\config.cpp
# End Source File
# Begin Source File

SOURCE=.\console.cpp
# End Source File
# Begin Source File

SOURCE=.\crt0.c
# End Source File
# Begin Source File

SOURCE=.\fat16.cpp
# End Source File
# Begin Source File

SOURCE=.\fat16.tmp.cpp
# End Source File
# Begin Source File

SOURCE=.\folder.cpp
# End Source File
# Begin Source File

SOURCE=.\kdll.rc
# End Source File
# Begin Source File

SOURCE=.\main.cpp
# End Source File
# Begin Source File

SOURCE=.\Ne2000.cpp
# End Source File
# Begin Source File

SOURCE=.\pci.c
# End Source File
# Begin Source File

SOURCE=.\ramdisk.cpp
# End Source File
# Begin Source File

SOURCE=.\serial.cpp
# End Source File
# Begin Source File

SOURCE=.\textcons.cpp
# End Source File
# Begin Source File

SOURCE=.\wildcard.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\8390.h
# End Source File
# Begin Source File

SOURCE=.\british.h
# End Source File
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=.\console.h
# End Source File
# Begin Source File

SOURCE=.\fat.h
# End Source File
# Begin Source File

SOURCE=.\fb.h
# End Source File
# Begin Source File

SOURCE=.\folder.h
# End Source File
# Begin Source File

SOURCE=.\Ne2000.h
# End Source File
# Begin Source File

SOURCE=.\pci.h
# End Source File
# Begin Source File

SOURCE=.\ramdisk.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\textcons.h
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
