# Microsoft Developer Studio Project File - Name="kernel" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=kernel - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "kernel.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "kernel.mak" CFG="kernel - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "kernel - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "kernel - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "kernel - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f kernel.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "kernel.exe"
# PROP BASE Bsc_Name "kernel.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "NMAKE /f kernel.mak"
# PROP Rebuild_Opt "/a"
# PROP Target_File "kernel.exe"
# PROP Bsc_Name "kernel.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "kernel - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f kernel.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "kernel.exe"
# PROP BASE Bsc_Name "kernel.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "NMAKE /f kernel.mak"
# PROP Rebuild_Opt "/a"
# PROP Target_File "kernel.exe"
# PROP Bsc_Name "kernel.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "kernel - Win32 Release"
# Name "kernel - Win32 Debug"

!IF  "$(CFG)" == "kernel - Win32 Release"

!ELSEIF  "$(CFG)" == "kernel - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "i386"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\i386\arch.c
# End Source File
# Begin Source File

SOURCE=".\i386\gdb-stub.c"
# End Source File
# Begin Source File

SOURCE=.\i386\gdb.c
# End Source File
# Begin Source File

SOURCE=.\i386\i386.c
# End Source File
# Begin Source File

SOURCE=.\i386\isr.asm
# End Source File
# Begin Source File

SOURCE=.\i386\Makefile
# End Source File
# Begin Source File

SOURCE=.\i386\memory.c
# End Source File
# Begin Source File

SOURCE=.\i386\scdsptch.c
# End Source File
# Begin Source File

SOURCE=.\start.asm
# End Source File
# End Group
# Begin Source File

SOURCE=.\cache.c
# End Source File
# Begin Source File

SOURCE=.\clip.c
# End Source File
# Begin Source File

SOURCE=.\cppsup.cpp
# End Source File
# Begin Source File

SOURCE=.\debug.c
# End Source File
# Begin Source File

SOURCE=.\device.c
# End Source File
# Begin Source File

SOURCE=.\fs.c
# End Source File
# Begin Source File

SOURCE=.\handle.c
# End Source File
# Begin Source File

SOURCE=.\io.c
# End Source File
# Begin Source File

SOURCE=.\main.c
# End Source File
# Begin Source File

SOURCE=.\mod_pe.c
# End Source File
# Begin Source File

SOURCE=.\port.c
# End Source File
# Begin Source File

SOURCE=.\proc.c
# End Source File
# Begin Source File

SOURCE=.\profile.c
# End Source File
# Begin Source File

SOURCE=.\queue.c
# End Source File
# Begin Source File

SOURCE=.\ramdisk_mb.c
# End Source File
# Begin Source File

SOURCE=.\rtlsup.c
# End Source File
# Begin Source File

SOURCE=.\syscall.c
# End Source File
# Begin Source File

SOURCE=.\thread.c
# End Source File
# Begin Source File

SOURCE=.\vfs.c
# End Source File
# Begin Source File

SOURCE=.\vmm.c
# End Source File
# Begin Source File

SOURCE=.\winmgr.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "kernel"

# PROP Default_Filter "*.h"
# Begin Source File

SOURCE=..\..\include\kernel\arch.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\cache.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\debug.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\device
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\driver.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\fs.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\handle.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\init.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\io.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\kernel.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\memory.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\multiboot.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\port.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\proc.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\ramdisk.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\sched.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\thread.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\vmm.h
# End Source File
# End Group
# Begin Group "i386 (h)"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\kernel\i386.h
# End Source File
# Begin Source File

SOURCE=.\i386\wrappers.h
# End Source File
# End Group
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=C:\BOCHS\bochs.out
# End Source File
# Begin Source File

SOURCE=.\kernel.def
# End Source File
# Begin Source File

SOURCE=.\kernel.ld
# End Source File
# Begin Source File

SOURCE=.\listing.txt
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# End Target
# End Project
