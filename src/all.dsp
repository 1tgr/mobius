# Microsoft Developer Studio Project File - Name="all" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=all - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "all.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "all.mak" CFG="all - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "all - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "all - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "all - Win32 Release"

# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f all.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "all.exe"
# PROP BASE Bsc_Name "all.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "domake"
# PROP Rebuild_Opt "/a"
# PROP Target_File "all.exe"
# PROP Bsc_Name "all.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "all - Win32 Debug"

# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f all.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "all.exe"
# PROP BASE Bsc_Name "all.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "make"
# PROP Rebuild_Opt ""
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "all - Win32 Release"
# Name "all - Win32 Debug"

!IF  "$(CFG)" == "all - Win32 Release"

!ELSEIF  "$(CFG)" == "all - Win32 Debug"

!ENDIF 

# Begin Group "boot"

# PROP Default_Filter ".asm"
# Begin Source File

SOURCE=.\boot\bootsect.asm
# End Source File
# Begin Source File

SOURCE=.\boot\gdtnasm.inc
# End Source File
# Begin Source File

SOURCE=.\boot\listing.txt
# End Source File
# Begin Source File

SOURCE=.\boot\Makefile
# End Source File
# Begin Source File

SOURCE=.\boot\minifs.asm
# End Source File
# Begin Source File

SOURCE=.\boot\mobel.asm
# End Source File
# Begin Source File

SOURCE=.\boot\mobel_pe.asm
# End Source File
# End Group
# Begin Group "bin"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\bin\Makefile
# End Source File
# End Group
# Begin Source File

SOURCE=C:\BOCHS\bochs.out
# End Source File
# Begin Source File

SOURCE=..\bin\coffbase.txt
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# End Target
# End Project
