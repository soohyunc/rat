# Microsoft Developer Studio Project File - Name="rat" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=rat - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "rat.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "rat.mak" CFG="rat - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rat - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "rat - Win32 Debug IPv6" (based on "Win32 (x86) Application")
!MESSAGE "rat - Win32 Debug IPv6 Musica" (based on "Win32 (x86) Application")
!MESSAGE "rat - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "rat - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\common" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "SASR" /D "NEED_SNPRINTF" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 uclmm.lib winmm.lib wsock32.lib Ws2_32.lib kernel32.lib user32.lib advapi32.lib /nologo /version:1984.0 /subsystem:windows /machine:I386 /libpath:"..\common\Release" /libpath:"..\tcl-8.0\win\Release" /libpath:"..\tk-8.0\win\Release"

!ELSEIF  "$(CFG)" == "rat - Win32 Debug IPv6"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "rat___Win32_Debug_IPv6"
# PROP BASE Intermediate_Dir "rat___Win32_Debug_IPv6"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_IPv6"
# PROP Intermediate_Dir "Debug_IPv6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "c:\Program Files\Tcl\include" /I "c:\Program Files\Tcl\include\xlib" /I "c:\DDK\inc" /I "c:..\msripv6\inc" /D "_WINDOWS" /D "DEBUG" /D "SASR" /D "WIN32" /D "_DEBUG" /D "HAVE_IPv6" /FR /YX /FD /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\common" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "..\tk-8.0\xlib\X11" /I "\DDK\inc" /I "..\ipv6kit\inc" /D "_WINDOWS" /D "DEBUG" /D "SASR" /D "WIN32" /D "_DEBUG" /D "HAVE_IPv6" /D "NEED_SNPRINTF" /FR /YX /FD /I ../common /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winmm.lib Ws2_32.lib msacm32.lib tcl80vc.lib tk80vc.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wship6.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:"c:..\rat32\rat" /libpath:"c:\Program Files\Tcl\lib" /libpath:".\win32" /libpath:"c:..\msripv6\wship6\obj\i386\free"
# ADD LINK32 tklib.lib tcllib.lib uclmm.lib wship6.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /version:1984.0 /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:"..\tcl-8.0\win\Debug_IPv6" /libpath:"..\tk-8.0\win\Debug_IPv6" /libpath:"..\IPv6kit\lib" /libpath:"..\common\Debug_IPv6"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "rat - Win32 Debug IPv6 Musica"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "rat___Win32_Debug_IPv6_Musica"
# PROP BASE Intermediate_Dir "rat___Win32_Debug_IPv6_Musica"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_IPv6_Musica"
# PROP Intermediate_Dir "Debug_IPv6_Musica"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\common" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "..\tk-8.0\xlib\X11" /I "\DDK\inc" /I "..\MSR_IPv6_1.3\inc" /D "_WINDOWS" /D "DEBUG" /D "SASR" /D "WIN32" /D "_DEBUG" /D "HAVE_IPv6" /FR /YX /FD /I ../common /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\common" /I "..\tk-8.0\xlib\X11" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /I "..\MUSICA\WINSOCK6" /D "_WINDOWS" /D "SASR" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "HAVE_IPv6" /D "MUSICA_IPV6" /D "_WINNT" /D "_POSIX" /D "NEED_SNPRINTF" /FR /YX /FD /I ../common /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 tklib.lib tcllib.lib wship6.lib uclmm.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:"..\tcl-8.0\win\Debug_IPv6" /libpath:"..\tk-8.0\win\Debug_IPv6" /libpath:"..\MSR_IPv6_1.3\wship6\obj\i386\free" /libpath:"..\common\Debug"
# SUBTRACT BASE LINK32 /pdb:none /incremental:no /map /force
# ADD LINK32 tklib.lib tcllib.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib uclmm.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib advapi32.lib lib44bsd.lib Resolv.lib /nologo /version:1984.0 /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:"..\tcl-8.0\win\Debug_IPv6" /libpath:"..\tk-8.0\win\Debug_IPv6" /libpath:"..\MUSICA\WINSOCK6" /libpath:"..\common\Debug"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "rat - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\common" /I "..\tcl-8.0\generic" /I "..\tk-8.0\generic" /I "..\tk-8.0\xlib" /I "\DDK\inc" /D "_WINDOWS" /D "DEBUG" /D "WIN32" /D "_DEBUG" /D "DEBUG_MEM" /D "NEED_SNPRINTF" /D "SASR" /Fr /YX /FD /I ../common /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 uclmm.lib winmm.lib wsock32.lib Ws2_32.lib kernel32.lib user32.lib advapi32.lib /nologo /version:1984.0 /subsystem:windows /debug /machine:I386 /libpath:".\Debug" /libpath:"..\common\Debug" /libpath:"..\tcl-8.0\win\Debug" /libpath:"..\tk-8.0\win\Debug"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "rat - Win32 Release"
# Name "rat - Win32 Debug IPv6"
# Name "rat - Win32 Debug IPv6 Musica"
# Name "rat - Win32 Debug"
# Begin Group "C Source Files"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\codec_compat.c
# End Source File
# Begin Source File

SOURCE=.\fatal_error.c
# End Source File
# Begin Source File

SOURCE=.\main_control.c
# End Source File
# Begin Source File

SOURCE=.\mbus_control.c
# End Source File
# Begin Source File

SOURCE=.\win32.c

!IF  "$(CFG)" == "rat - Win32 Release"

!ELSEIF  "$(CFG)" == "rat - Win32 Debug IPv6"

!ELSEIF  "$(CFG)" == "rat - Win32 Debug IPv6 Musica"

!ELSEIF  "$(CFG)" == "rat - Win32 Debug"

# ADD CPP /FR

!ENDIF 

# End Source File
# End Group
# Begin Group "C Header Files"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\codec_compat.h
# End Source File
# Begin Source File

SOURCE=.\config_win32.h
# End Source File
# Begin Source File

SOURCE=.\mbus_control.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Version

!IF  "$(CFG)" == "rat - Win32 Release"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\Version

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "rat - Win32 Debug IPv6"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\Version

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "rat - Win32 Debug IPv6 Musica"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\Version

"version.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy win32\set.txt + VERSION win32\vergen.bat 
	copy win32\vergen.bat + win32\null.txt win32\vergen.bat 
	copy win32\vergen.bat + win32\echo.txt win32\vergen.bat 
	win32\vergen.bat 
	move win32\version.h version.h 
	erase win32\version.h 
	erase win32\vergen.bat 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "rat - Win32 Debug"

USERDEP__VERSI="win32\echo.txt"	"win32\set.txt"	"win32\null.txt"	
# Begin Custom Build - Generating "version.h".
InputPath=.\Version

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
