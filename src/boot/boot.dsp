# Microsoft Developer Studio Project File - Name="boot" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=boot - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "boot.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "boot.mak" CFG="boot - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "boot - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "boot - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "boot - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f boot.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "boot.exe"
# PROP BASE Bsc_Name "boot.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "nmake /f "boot.mak""
# PROP Rebuild_Opt "/a"
# PROP Target_File "boot.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "boot - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f boot.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "boot.exe"
# PROP BASE Bsc_Name "boot.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "nmake /f "boot.mak""
# PROP Rebuild_Opt "/a"
# PROP Target_File "boot.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "boot - Win32 Release"
# Name "boot - Win32 Debug"

!IF  "$(CFG)" == "boot - Win32 Release"

!ELSEIF  "$(CFG)" == "boot - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\fat.c
# End Source File
# Begin Source File

SOURCE=.\io.c
# End Source File
# Begin Source File

SOURCE=.\loaderb.asm
# End Source File
# Begin Source File

SOURCE=.\main.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\mobel_pe.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Assembly Files"

# PROP Default_Filter "asm inc"
# Begin Source File

SOURCE=.\bootsect.asm
# End Source File
# Begin Source File

SOURCE=.\gdtnasm.inc
# End Source File
# Begin Source File

SOURCE=.\minifs.asm
# End Source File
# Begin Source File

SOURCE=.\mobel.asm
# End Source File
# Begin Source File

SOURCE=.\mobel_pe.asm
# End Source File
# Begin Source File

SOURCE=.\startup.asm
# End Source File
# End Group
# Begin Source File

SOURCE=C:\BOCHS\bochs.out
# End Source File
# Begin Source File

SOURCE=.\listing.txt
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# End Target
# End Project
