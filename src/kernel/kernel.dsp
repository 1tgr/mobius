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

# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f kernel.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "kernel.exe"
# PROP BASE Bsc_Name "kernel.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "..\domake"
# PROP Rebuild_Opt "rebuild"
# PROP Target_File "kernel.bin"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "kernel - Win32 Debug"

# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f kernel.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "kernel.exe"
# PROP BASE Bsc_Name "kernel.bsc"
# PROP BASE Target_Dir ""
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "..\domake"
# PROP Rebuild_Opt "rebuild"
# PROP Target_File "kernel.bin"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "kernel - Win32 Release"
# Name "kernel - Win32 Debug"

!IF  "$(CFG)" == "kernel - Win32 Release"

!ELSEIF  "$(CFG)" == "kernel - Win32 Debug"

!ENDIF 

# Begin Group "C Source"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\cache.c
# End Source File
# Begin Source File

SOURCE=.\config.c
# End Source File
# Begin Source File

SOURCE=.\cupertino.c
# End Source File
# Begin Source File

SOURCE=.\device.c
# End Source File
# Begin Source File

SOURCE=.\dowprintf.c
# End Source File
# Begin Source File

SOURCE=.\font8x16.c
# End Source File
# Begin Source File

SOURCE=.\font8x8.c
# End Source File
# Begin Source File

SOURCE=.\fs.c
# End Source File
# Begin Source File

SOURCE=.\handle.c
# End Source File
# Begin Source File

SOURCE=.\hash.c
# End Source File
# Begin Source File

SOURCE=.\i386.c
# End Source File
# Begin Source File

SOURCE=.\kstart.c
# End Source File
# Begin Source File

SOURCE=.\main.c
# End Source File
# Begin Source File

SOURCE=.\mbstowcs.c
# End Source File
# Begin Source File

SOURCE=.\memory.c
# End Source File
# Begin Source File

SOURCE=.\mod_pe.c
# End Source File
# Begin Source File

SOURCE=.\module.c
# End Source File
# Begin Source File

SOURCE=.\obj.c
# End Source File
# Begin Source File

SOURCE=.\port.c
# End Source File
# Begin Source File

SOURCE=.\ramdisk.c
# End Source File
# Begin Source File

SOURCE=.\root.c
# End Source File
# Begin Source File

SOURCE=.\serial.c
# End Source File
# Begin Source File

SOURCE=.\strcmp.c
# End Source File
# Begin Source File

SOURCE=.\stricmp.c
# End Source File
# Begin Source File

SOURCE=.\strtok.c
# End Source File
# Begin Source File

SOURCE=.\strtol.c
# End Source File
# Begin Source File

SOURCE=.\sys.c
# End Source File
# Begin Source File

SOURCE=.\syscall.c
# End Source File
# Begin Source File

SOURCE=.\t.c
# End Source File
# Begin Source File

SOURCE=.\text.c
# End Source File
# Begin Source File

SOURCE=.\thread.c
# End Source File
# Begin Source File

SOURCE=.\vga.c
# End Source File
# Begin Source File

SOURCE=.\vm86.c
# End Source File
# Begin Source File

SOURCE=.\vmm.c
# End Source File
# Begin Source File

SOURCE=.\wcstombs.c
# End Source File
# Begin Source File

SOURCE=.\wprintf.c
# End Source File
# End Group
# Begin Group "Assembler Source"

# PROP Default_Filter ".asm"
# Begin Source File

SOURCE=.\isr.asm
# End Source File
# Begin Source File

SOURCE=.\start.asm
# End Source File
# End Group
# Begin Group "Headers"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=..\..\include\kernel\cache.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\config.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\console.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\debug.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\devreq.h
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

SOURCE=..\..\include\kernel\hash.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\i386.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\kernel.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\memory.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\obj.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\pe.h
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

SOURCE=..\..\include\kernel\serial.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\sys.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\thread.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\vmm.h
# End Source File
# Begin Source File

SOURCE=..\..\include\kernel\wrappers.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\kernel.def
# End Source File
# Begin Source File

SOURCE=.\kernel.ld
# End Source File
# Begin Source File

SOURCE=.\kernel.map
# End Source File
# Begin Source File

SOURCE=.\kernel.rc
# End Source File
# Begin Source File

SOURCE=.\kernelrc
# End Source File
# Begin Source File

SOURCE=.\listing.txt
# End Source File
# Begin Source File

SOURCE=.\logo.bmp
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# Begin Source File

SOURCE=.\symbols.txt
# End Source File
# End Target
# End Project
