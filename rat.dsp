# Microsoft Developer Studio Project File - Name="rat" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=rat - Win32 Debug IPv6
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "rat.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "rat.mak" CFG="rat - Win32 Debug IPv6"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rat - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "rat - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "rat - Win32 Debug IPv6" (based on "Win32 (x86) Application")
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
# ADD CPP /nologo /W3 /GX /O2 /I "\src\common" /I "\src\tcl-8.0\generic" /I "\src\tk-8.0\generic" /I "\src\tk-8.0\xlib" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "SASR" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 tcllib.lib tklib.lib uclmm.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386 /libpath:"\src\common\Release" /libpath:"\src\tcl-8.0\win\Release" /libpath:"\src\tk-8.0\win\Release"

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
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "\src\common" /I "\src\tcl-8.0\generic" /I "\src\tk-8.0\generic" /I "\src\tk-8.0\xlib" /I "\DDK\inc" /D "_WINDOWS" /D "DEBUG" /D "SASR" /D "WIN32" /D "_DEBUG" /D "DEBUG_MEM" /Fr /YX /FD /I ../common /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 tcllib.lib tklib.lib uclmm.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /libpath:"\src\common\Debug" /libpath:"\src\tcl-8.0\win\Debug" /libpath:"\src\tk-8.0\win\Debug"
# SUBTRACT LINK32 /pdb:none

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "c:\Program Files\Tcl\include" /I "c:\Program Files\Tcl\include\xlib" /I "c:\DDK\inc" /I "c:\src\msripv6\inc" /D "_WINDOWS" /D "DEBUG" /D "SASR" /D "WIN32" /D "_DEBUG" /D "HAVE_IPv6" /FR /YX /FD /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "\src\common" /I "\src\tcl-8.0\generic" /I "\src\tk-8.0\generic" /I "\src\tk-8.0\xlib" /I "\src\tk-8.0\xlib\X11" /I "\DDK\inc" /I "\src\msripv6-1.2\inc" /D "_WINDOWS" /D "DEBUG" /D "SASR" /D "WIN32" /D "_DEBUG" /D "HAVE_IPv6" /FR /YX /FD /I ../common /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 winmm.lib Ws2_32.lib msacm32.lib tcl80vc.lib tk80vc.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wship6.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:"c:\src\rat32\rat" /libpath:"c:\Program Files\Tcl\lib" /libpath:".\win32" /libpath:"c:\src\msripv6\wship6\obj\i386\free"
# ADD LINK32 tklib.lib tcllib.lib wship6.lib uclmm.lib winmm.lib wsock32.lib Ws2_32.lib msacm32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept /libpath:"\src\tcl-8.0\win\Debug_IPv6" /libpath:"\src\tk-8.0\win\Debug_IPv6" /libpath:"\src\msripv6-1.2\wship6\obj\i386\free" /libpath:"\src\common\Debug"
# SUBTRACT LINK32 /pdb:none /incremental:no /map /force

!ENDIF 

# Begin Target

# Name "rat - Win32 Release"
# Name "rat - Win32 Debug"
# Name "rat - Win32 Debug IPv6"
# Begin Group "C Source Files"

# PROP Default_Filter ".c"
# Begin Source File

SOURCE=.\auddev.c
# End Source File
# Begin Source File

SOURCE=.\auddev_null.c
# End Source File
# Begin Source File

SOURCE=.\auddev_win32.c
# End Source File
# Begin Source File

SOURCE=.\audio.c
# End Source File
# Begin Source File

SOURCE=.\audio_fmt.c
# End Source File
# Begin Source File

SOURCE=.\audio_util.c
# End Source File
# Begin Source File

SOURCE=.\cc_rdncy.c
# End Source File
# Begin Source File

SOURCE=.\cc_vanilla.c
# End Source File
# Begin Source File

SOURCE=.\channel.c
# End Source File
# Begin Source File

SOURCE=.\channel_types.c
# End Source File
# Begin Source File

SOURCE=.\codec.c
# End Source File
# Begin Source File

SOURCE=.\codec_dvi.c
# End Source File
# Begin Source File

SOURCE=.\codec_g711.c
# End Source File
# Begin Source File

SOURCE=.\codec_g726.c
# End Source File
# Begin Source File

SOURCE=.\codec_gsm.c
# End Source File
# Begin Source File

SOURCE=.\codec_l16.c
# End Source File
# Begin Source File

SOURCE=.\codec_lpc.c
# End Source File
# Begin Source File

SOURCE=.\codec_state.c
# End Source File
# Begin Source File

SOURCE=.\codec_types.c
# End Source File
# Begin Source File

SOURCE=.\codec_vdvi.c
# End Source File
# Begin Source File

SOURCE=.\codec_wbs.c
# End Source File
# Begin Source File

SOURCE=.\convert.c
# End Source File
# Begin Source File

SOURCE=.\crypt.c
# End Source File
# Begin Source File

SOURCE=.\cushion.c
# End Source File
# Begin Source File

SOURCE=.\cx_dvi.c
# End Source File
# Begin Source File

SOURCE=.\cx_g726.c
# End Source File
# Begin Source File

SOURCE=.\cx_g726_16.c
# End Source File
# Begin Source File

SOURCE=.\cx_g726_24.c
# End Source File
# Begin Source File

SOURCE=.\cx_g726_32.c
# End Source File
# Begin Source File

SOURCE=.\cx_g726_40.c
# End Source File
# Begin Source File

SOURCE=.\cx_gsm.c
# End Source File
# Begin Source File

SOURCE=.\cx_lpc.c
# End Source File
# Begin Source File

SOURCE=.\cx_vdvi.c
# End Source File
# Begin Source File

SOURCE=.\cx_wbs.c
# End Source File
# Begin Source File

SOURCE=.\main.c
# End Source File
# Begin Source File

SOURCE=.\mbus_engine.c

!IF  "$(CFG)" == "rat - Win32 Release"

!ELSEIF  "$(CFG)" == "rat - Win32 Debug"

# ADD CPP /W3

!ELSEIF  "$(CFG)" == "rat - Win32 Debug IPv6"

# ADD BASE CPP /W3
# ADD CPP /W3

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\mbus_ui.c
# End Source File
# Begin Source File

SOURCE=.\mix.c
# End Source File
# Begin Source File

SOURCE=.\net.c
# End Source File
# Begin Source File

SOURCE=.\parameters.c
# End Source File
# Begin Source File

SOURCE=.\pckt_queue.c
# End Source File
# Begin Source File

SOURCE=.\playout.c
# End Source File
# Begin Source File

SOURCE=.\render_3D.c
# End Source File
# Begin Source File

SOURCE=.\repair.c
# End Source File
# Begin Source File

SOURCE=.\rtcp.c
# End Source File
# Begin Source File

SOURCE=.\rtcp_db.c
# End Source File
# Begin Source File

SOURCE=.\rtcp_pckt.c
# End Source File
# Begin Source File

SOURCE=.\session.c
# End Source File
# Begin Source File

SOURCE=.\sndfile.c
# End Source File
# Begin Source File

SOURCE=.\source.c
# End Source File
# Begin Source File

SOURCE=.\statistics.c
# End Source File
# Begin Source File

SOURCE=.\tcltk.c
# End Source File
# Begin Source File

SOURCE=.\timers.c
# End Source File
# Begin Source File

SOURCE=.\transcoder.c
# End Source File
# Begin Source File

SOURCE=.\transmit.c
# End Source File
# Begin Source File

SOURCE=.\ts.c
# End Source File
# Begin Source File

SOURCE=.\ui.c
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
# Begin Group "C Header Files"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\auddev.h
# End Source File
# Begin Source File

SOURCE=.\auddev_luigi.h
# End Source File
# Begin Source File

SOURCE=.\auddev_null.h
# End Source File
# Begin Source File

SOURCE=.\auddev_osprey.h
# End Source File
# Begin Source File

SOURCE=.\auddev_oss.h
# End Source File
# Begin Source File

SOURCE=.\auddev_pca.h
# End Source File
# Begin Source File

SOURCE=.\auddev_sgi.h
# End Source File
# Begin Source File

SOURCE=.\auddev_sparc.h
# End Source File
# Begin Source File

SOURCE=.\auddev_win32.h
# End Source File
# Begin Source File

SOURCE=.\audio.h
# End Source File
# Begin Source File

SOURCE=.\audio_fmt.h
# End Source File
# Begin Source File

SOURCE=.\audio_types.h
# End Source File
# Begin Source File

SOURCE=.\audio_util.h
# End Source File
# Begin Source File

SOURCE=.\cc_rdncy.h
# End Source File
# Begin Source File

SOURCE=.\cc_vanilla.h
# End Source File
# Begin Source File

SOURCE=.\channel.h
# End Source File
# Begin Source File

SOURCE=.\channel_types.h
# End Source File
# Begin Source File

SOURCE=.\codec.h
# End Source File
# Begin Source File

SOURCE=.\codec_acm.h
# End Source File
# Begin Source File

SOURCE=.\codec_dvi.h
# End Source File
# Begin Source File

SOURCE=.\codec_g711.h
# End Source File
# Begin Source File

SOURCE=.\codec_g726.h
# End Source File
# Begin Source File

SOURCE=.\codec_gsm.h
# End Source File
# Begin Source File

SOURCE=.\codec_l16.h
# End Source File
# Begin Source File

SOURCE=.\codec_lpc.h
# End Source File
# Begin Source File

SOURCE=.\codec_state.h
# End Source File
# Begin Source File

SOURCE=.\codec_types.h
# End Source File
# Begin Source File

SOURCE=.\codec_vdvi.h
# End Source File
# Begin Source File

SOURCE=.\codec_wbs.h
# End Source File
# Begin Source File

SOURCE=.\config_unix.h
# End Source File
# Begin Source File

SOURCE=.\config_win32.h
# End Source File
# Begin Source File

SOURCE=.\convert.h
# End Source File
# Begin Source File

SOURCE=.\crypt.h
# End Source File
# Begin Source File

SOURCE=.\crypt_global.h
# End Source File
# Begin Source File

SOURCE=.\crypt_random.h
# End Source File
# Begin Source File

SOURCE=.\cushion.h
# End Source File
# Begin Source File

SOURCE=.\cx_dvi.h
# End Source File
# Begin Source File

SOURCE=.\cx_g726.h
# End Source File
# Begin Source File

SOURCE=.\cx_gsm.h
# End Source File
# Begin Source File

SOURCE=.\cx_lpc.h
# End Source File
# Begin Source File

SOURCE=.\cx_vdvi.h
# End Source File
# Begin Source File

SOURCE=.\cx_wbs.h
# End Source File
# Begin Source File

SOURCE=.\gsm.h
# End Source File
# Begin Source File

SOURCE=.\mbus_engine.h
# End Source File
# Begin Source File

SOURCE=.\mbus_ui.h
# End Source File
# Begin Source File

SOURCE=.\mix.h
# End Source File
# Begin Source File

SOURCE=.\net.h
# End Source File
# Begin Source File

SOURCE=.\parameters.h
# End Source File
# Begin Source File

SOURCE=.\pckt_queue.h
# End Source File
# Begin Source File

SOURCE=.\playout.h
# End Source File
# Begin Source File

SOURCE=.\render_3D.h
# End Source File
# Begin Source File

SOURCE=.\repair.h
# End Source File
# Begin Source File

SOURCE=.\rtcp.h
# End Source File
# Begin Source File

SOURCE=.\rtcp_db.h
# End Source File
# Begin Source File

SOURCE=.\rtcp_pckt.h
# End Source File
# Begin Source File

SOURCE=.\session.h
# End Source File
# Begin Source File

SOURCE=.\sndfile.h
# End Source File
# Begin Source File

SOURCE=.\source.h
# End Source File
# Begin Source File

SOURCE=.\statistics.h
# End Source File
# Begin Source File

SOURCE=.\tcltk.h
# End Source File
# Begin Source File

SOURCE=.\timers.h
# End Source File
# Begin Source File

SOURCE=.\transcoder.h
# End Source File
# Begin Source File

SOURCE=.\transmit.h
# End Source File
# Begin Source File

SOURCE=.\ts.h
# End Source File
# Begin Source File

SOURCE=.\ui.h
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# End Group
# Begin Group "Tcl/Tk Scripts"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\asfilebox.tcl
# End Source File
# Begin Source File

SOURCE=.\ui_audiotool.tcl

!IF  "$(CFG)" == "rat - Win32 Release"

# Begin Custom Build - Building audiotool ui
InputDir=.
InputPath=.\ui_audiotool.tcl

"$(InputDir)\ui_audiotool.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type $(InputDir)\ui_audiotool.tcl | tcl2c\tcl2c ui_audiotool >                    $(InputDir)\ui_audiotool.c

# End Custom Build

!ELSEIF  "$(CFG)" == "rat - Win32 Debug"

USERDEP__UI_AU="$(InputDir)\asfilebox.tcl"	
# Begin Custom Build - Building audiotool ui
InputDir=.
InputPath=.\ui_audiotool.tcl

"$(InputDir)\ui_audiotool.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy asfilebox.tcl + ui_audiotool.tcl ui_at.tcl 
	type $(InputDir)\ui_at.tcl | tcl2c\tcl2c ui_audiotool >                    $(InputDir)\ui_audiotool.c 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "rat - Win32 Debug IPv6"

USERDEP__UI_AU="$(InputDir)\asfilebox.tcl"	
# Begin Custom Build - Building audiotool ui
InputDir=.
InputPath=.\ui_audiotool.tcl

"$(InputDir)\ui_audiotool.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy asfilebox.tcl + ui_audiotool.tcl ui_at.tcl 
	type $(InputDir)\ui_at.tcl | tcl2c\tcl2c ui_audiotool >                    $(InputDir)\ui_audiotool.c 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ui_transcoder.tcl

!IF  "$(CFG)" == "rat - Win32 Release"

# Begin Custom Build - Building transcoder ui
InputDir=.
InputPath=.\ui_transcoder.tcl

"$(InputDir)\ui_transcoder.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type $(InputDir)\ui_transcoder.tcl | tcl2c\tcl2c ui_transcoder > $(InputDir)\ui_transcoder.c

# End Custom Build

!ELSEIF  "$(CFG)" == "rat - Win32 Debug"

# Begin Custom Build - Building transcoder ui
InputDir=.
InputPath=.\ui_transcoder.tcl

"$(InputDir)\ui_transcoder.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type $(InputDir)\ui_transcoder.tcl | tcl2c\tcl2c ui_transcoder > $(InputDir)\ui_transcoder.c

# End Custom Build

!ELSEIF  "$(CFG)" == "rat - Win32 Debug IPv6"

# Begin Custom Build - Building transcoder ui
InputDir=.
InputPath=.\ui_transcoder.tcl

"$(InputDir)\ui_transcoder.c" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	type $(InputDir)\ui_transcoder.tcl | tcl2c\tcl2c ui_transcoder > $(InputDir)\ui_transcoder.c

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE="..\tk-8.0\win\tk.res"
# End Source File
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

!ENDIF 

# End Source File
# End Target
# End Project
