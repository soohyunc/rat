char ui_relate[] = "\
#\n\
# Copyright (c) 1997 University College London\n\
# All rights reserved.\n\
# \n\
# Redistribution and use in source and binary forms, with or without\n\
# modification, is permitted, for non-commercial use only, provided \n\
# that the following conditions are met:\n\
# 1. Redistributions of source code must retain the above copyright\n\
#    notice, this list of conditions and the following disclaimer.\n\
# 2. Redistributions in binary form must reproduce the above copyright\n\
#    notice, this list of conditions and the following disclaimer in the\n\
#    documentation and/or other materials provided with the distribution.\n\
# 3. All advertising materials mentioning features or use of this software\n\
#    must display the following acknowledgement:\n\
#      This product includes software developed by the Computer Science\n\
#      Department at University College London\n\
# 4. Neither the name of the University nor of the Department may be used\n\
#    to endorse or promote products derived from this software without\n\
#    specific prior written permission.\n\
# Use of this software for commercial purposes is explicitly forbidden\n\
# unless prior written permission is obtained from the authors. \n\
# \n\
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND\n\
# ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n\
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE\n\
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n\
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n\
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n\
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n\
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n\
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n\
# SUCH DAMAGE.\n\
#\n\
\n\
option add *background 			gray80 		widgetDefault\n\
option add *foreground 			black 		widgetDefault\n\
option add *activeBackground 		gray85 		widgetDefault\n\
option add *selectBackground 		gray85 		widgetDefault\n\
option add *Scale.sliderForeground 	gray80 		widgetDefault\n\
option add *Scale.activeForeground 	gray85 		widgetDefault\n\
option add *Scale.background 		gray80		widgetDefault\n\
option add *Entry.background 		gray70 		widgetDefault\n\
option add *Menu*selectColor 		forestgreen 	widgetDefault\n\
option add *Radiobutton*selectColor 	forestgreen 	widgetDefault\n\
option add *Checkbutton*selectColor 	forestgreen 	widgetDefault\n\
option add *borderWidth 		1 \n\
option add *font			 -*-courier-medium-o-normal--8-*-m-*-iso8859-1\n\
\n\
set RAT_ADDR \"NONE\"\n\
set  UI_ADDR \"NONE\"\n\
\n\
#power meters\n\
proc bargraphCreate {bgraph} {\n\
	global oh$bgraph\n\
\n\
	frame $bgraph -bg black\n\
	frame $bgraph.inner0 -width 8 -height 8 -bg green\n\
	pack $bgraph.inner0 -side left -padx 1 -fill both -expand true\n\
	for {set i 1} {$i < 16} {incr i} {\n\
		frame $bgraph.inner$i -width 8 -height 8 -bg black\n\
		pack $bgraph.inner$i -side left -padx 1 -fill both -expand true\n\
	}\n\
	set oh$bgraph 0\n\
}\n\
\n\
proc bargraphSetHeight {bgraph height} {\n\
	upvar #0 oh$bgraph oh\n\
\n\
	if {$oh > $height} {\n\
		for {set i [expr $height + 1]} {$i <= $oh} {incr i} {\n\
			$bgraph.inner$i config -bg black\n\
		}\n\
	} else {\n\
		for {set i [expr $oh + 1]} {$i <= $height} {incr i} {\n\
			if {$i > 12} {\n\
				$bgraph.inner$i config -bg red\n\
			} else {\n\
				$bgraph.inner$i config -bg green\n\
			}\n\
		}\n\
	}\n\
	set oh $height\n\
}\n\
\n\
# Device input controls\n\
frame .i\n\
button         .i.mute   -highlightthickness 0 -padx 0 -pady 0 -text mute -command toggle_send -relief sunken\n\
button         .i.device -highlightthickness 0 -padx 0 -pady 0 -command toggle_input_port -bitmap \"mic_mute\"\n\
scale          .i.scale  -highlightthickness 0 -from 0 -to 99 -command set_gain -orient horizontal -relief raised\n\
bargraphCreate .i.powermeter\n\
pack .i.mute       -side left -fill y\n\
pack .i.device     -side left -fill y\n\
pack .i.powermeter -side top  -fill x\n\
pack .i.scale      -side top  -fill x\n\
pack .i -side top -fill x -expand 1\n\
\n\
# Device output controls\n\
frame .o\n\
button         .o.mute   -highlightthickness 0 -padx 0 -pady 0 -text mute -command toggle_play \n\
button         .o.device -highlightthickness 0 -padx 0 -pady 0 -command toggle_output_port -bitmap \"speaker\"\n\
scale          .o.scale  -highlightthickness 0 -from 0 -to 99 -command set_vol -orient horizontal -relief raised \n\
bargraphCreate .o.powermeter\n\
pack .o.mute       -side left -fill y\n\
pack .o.device     -side left -fill y\n\
pack .o.powermeter -side top  -fill x\n\
pack .o.scale      -side top  -fill x\n\
pack .o -side top -fill x -expand 1\n\
\n\
bind all <ButtonPress-3> \"toggle_send\"\n\
bind all <ButtonRelease-3> \"toggle_send\"\n\
bind all <q> {destroy .}\n\
\n\
proc set_gain {value} {\n\
	global RAT_ADDR\n\
	cb_send $RAT_ADDR \"input gain $value\"\n\
}\n\
\n\
proc set_vol {value} {\n\
	global RAT_ADDR\n\
	cb_send $RAT_ADDR \"output gain $value\"\n\
}\n\
\n\
proc toggle_input_port {} {\n\
  global RAT_ADDR\n\
  cb_send $RAT_ADDR \"toggle_input_port\"\n\
}\n\
\n\
proc toggle_output_port {} {\n\
  global RAT_ADDR\n\
  cb_send $RAT_ADDR \"toggle_output_port\"\n\
}\n\
\n\
proc toggle_send {} {\n\
  global RAT_ADDR\n\
  cb_send $RAT_ADDR \"toggle_send\"\n\
  if {[string compare [.i.mute cget -relief] raised] == 0} {\n\
    .i.mute configure -relief sunken\n\
  } else {\n\
    .i.mute configure -relief raised\n\
  }\n\
}\n\
\n\
proc toggle_play {} {\n\
  global RAT_ADDR\n\
  cb_send $RAT_ADDR \"toggle_play\"\n\
  if {[string compare [.o.mute cget -relief] raised] == 0} {\n\
    .o.mute configure -relief sunken\n\
  } else {\n\
    .o.mute configure -relief raised\n\
  }\n\
}\n\
\n\
# \n\
# The following function deal with receiving messages from the conference bus. The code\n\
# in ui.c will call cb_recv with the appropriate arguments when a message is received. \n\
#\n\
\n\
proc cb_recv {src cmd} {\n\
  if [string match [info procs [lindex cb_recv_$cmd 0]] [lindex cb_recv_$cmd 0]] {\n\
    eval cb_recv_$cmd \n\
  }\n\
}\n\
\n\
proc cb_recv_init {rat_addr ui_addr} {\n\
	global RAT_ADDR UI_ADDR\n\
\n\
	set RAT_ADDR $rat_addr\n\
	set  UI_ADDR  $ui_addr\n\
\n\
	cb_send $RAT_ADDR \"primary    DVI\"\n\
	cb_send $RAT_ADDR \"redundancy DVI\"\n\
}\n\
\n\
proc cb_recv_powermeter {type level} {\n\
	# powermeter input  <value>\n\
	# powermeter output <value>\n\
	# powermeter <ssrc> <value>\n\
	switch $type {\n\
		input   {bargraphSetHeight .i.powermeter $level}\n\
		output  {bargraphSetHeight .o.powermeter $level}\n\
	 	default {}\n\
	}\n\
}\n\
\n\
proc cb_recv_input {cmd args} {\n\
	switch $cmd {\n\
		gain	{.i.scale set $args}\n\
		device	{.i.device configure -bitmap $args}\n\
		mute    {.i.mute configure -relief sunken}\n\
		unmute  {.i.mute configure -relief raised}\n\
	}\n\
}\n\
\n\
proc cb_recv_output {cmd args} {\n\
	switch $cmd {\n\
		gain	{.o.scale set $args}\n\
		device	{.o.device configure -bitmap $args}\n\
		mute    {.o.mute configure -relief sunken}\n\
		unmute  {.o.mute configure -relief raised}\n\
	}\n\
}\n\
\n\
";
