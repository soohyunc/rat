# Microsoft Developer Studio Project File - Name="ratui" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=ratui - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ratui.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ratui.mak" CFG="ratui - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ratui - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "ratui - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ratui - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib uclmm.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib tcllib.lib tklib.lib advapi32.lib gdi32.lib comdlg32.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "ratui - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /D "_WINDOWS" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "DEBUG_MEM" /D "NEED_SNPRINTF" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 uclmm.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib  tklib.lib tcllib.lib kernel32.lib user32.lib advapi32.lib gdi32.lib comdlg32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:".\Debug" /libpath:"..\common\Debug" /libpath:"..\tcl-8.0\win\Debug" /libpath:"..\tk-8.0\win\Debug"

!ENDIF 

# Begin Target

# Name "ratui - Win32 Release"
# Name "ratui - Win32 Debug"
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\mbus_ui.h
# End Source File
# Begin Source File

SOURCE=..\tcltk.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Tcl/Tk Scripts"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\asfilebox.tcl
# End Source File
# Begin Source File

SOURCE=.\ui_audiotool.tcl

!IF  "$(CFG)" == "ratui - Win32 Release"

# Begin Custom Build - Building audiotool ui
InputDir=.
InputPath=.\ui_audiotool.tcl

"$(InputDir)\ui_audiotool.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy asfilebox.tcl + ui_audiotool.tcl ui_at.tcl 
	type $(InputDir)\ui_at.tcl | ..\tcl-8.0\win\tcl2c\tcl2c ui_audiotool >                    $(InputDir)\ui_audiotool.c 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ratui - Win32 Debug"

USERDEP__UI_AU="$(InputDir)\asfilebox.tcl"	
# Begin Custom Build - Building audiotool ui
InputDir=.
InputPath=.\ui_audiotool.tcl

"$(InputDir)\ui_audiotool.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy asfilebox.tcl + ui_audiotool.tcl ui_at.tcl 
	type $(InputDir)\ui_at.tcl | ..\tcl-8.0\win\tcl2c\tcl2c ui_audiotool >                    $(InputDir)\ui_audiotool.c 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ui_transcoder.tcl

!IF  "$(CFG)" == "ratui - Win32 Release"

# Begin Custom Build - Building transcoder ui
InputDir=.
InputPath=.\ui_transcoder.tcl

"$(InputDir)\ui_transcoder.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type $(InputDir)\ui_transcoder.tcl | ..\tcl-8.0\win\tcl2c\tcl2c ui_transcoder > $(InputDir)\ui_transcoder.c

# End Custom Build

!ELSEIF  "$(CFG)" == "ratui - Win32 Debug"

# Begin Custom Build - Building transcoder ui
InputDir=.
InputPath=.\ui_transcoder.tcl

"$(InputDir)\ui_transcoder.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type $(InputDir)\ui_transcoder.tcl | ..\tcl-8.0\win\tcl2c\tcl2c ui_transcoder > $(InputDir)\ui_transcoder.c

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\main_ui.c
# End Source File
# Begin Source File

SOURCE=.\mbus_ui.c
# End Source File
# Begin Source File

SOURCE=.\tcltk.c
# End Source File
# Begin Source File

SOURCE=.\ui_audiotool.c
# End Source File
# Begin Source File

SOURCE=.\ui_transcoder.c
# End Source File
# Begin Source File

SOURCE=.\win32.c
# End Source File
# End Group
# Begin Source File

SOURCE=".\..\tk-8.0\win\tk.res"
# End Source File
# Begin Source File

SOURCE=.\VERSION

!IF  "$(CFG)" == "ratui - Win32 Release"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\VERSION

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "ratui - Win32 Debug"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\VERSION

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project