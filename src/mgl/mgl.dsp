# Microsoft Developer Studio Project File - Name="mgl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=mgl - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mgl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mgl.mak" CFG="mgl - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mgl - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "mgl - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "mgl - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f mgl.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "mgl.exe"
# PROP BASE Bsc_Name "mgl.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "nmake /f "mgl.mak""
# PROP Rebuild_Opt "/a"
# PROP Target_File "mgl.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "mgl - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f mgl.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "mgl.exe"
# PROP BASE Bsc_Name "mgl.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "nmake /f "mgl.mak""
# PROP Rebuild_Opt "/a"
# PROP Target_File "mgl.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "mgl - Win32 Release"
# Name "mgl - Win32 Debug"

!IF  "$(CFG)" == "mgl - Win32 Release"

!ELSEIF  "$(CFG)" == "mgl - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "test"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\test\mgltest.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\init.c
# End Source File
# Begin Source File

SOURCE=.\mgl.def
# End Source File
# Begin Source File

SOURCE=.\mgl.rc
# End Source File
# Begin Source File

SOURCE=.\rc.c
# End Source File
# Begin Source File

SOURCE=.\rect.c
# End Source File
# Begin Source File

SOURCE=.\render.c
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "gl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\gl\mgl.h
# End Source File
# End Group
# Begin Group "os"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\include\os\device.h
# End Source File
# Begin Source File

SOURCE=..\..\include\os\video.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\mgl.h
# End Source File
# Begin Source File

SOURCE=.\render.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Makefile
# End Source File
# End Target
# End Project
