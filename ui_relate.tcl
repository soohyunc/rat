#
# Copyright (c) 1997 University College London
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, is permitted, for non-commercial use only, provided 
# that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#      This product includes software developed by the Computer Science
#      Department at University College London
# 4. Neither the name of the University nor of the Department may be used
#    to endorse or promote products derived from this software without
#    specific prior written permission.
# Use of this software for commercial purposes is explicitly forbidden
# unless prior written permission is obtained from the authors. 
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

option add *background 			gray80 		widgetDefault
option add *foreground 			black 		widgetDefault
option add *activeBackground 		gray85 		widgetDefault
option add *selectBackground 		gray85 		widgetDefault
option add *Scale.sliderForeground 	gray80 		widgetDefault
option add *Scale.activeForeground 	gray85 		widgetDefault
option add *Scale.background 		gray80		widgetDefault
option add *Entry.background 		gray70 		widgetDefault
option add *Menu*selectColor 		forestgreen 	widgetDefault
option add *Radiobutton*selectColor 	forestgreen 	widgetDefault
option add *Checkbutton*selectColor 	forestgreen 	widgetDefault
option add *borderWidth 		1 
option add *font			 -*-courier-medium-o-normal--8-*-m-*-iso8859-1

#power meters
proc bargraphCreate {bgraph} {
	global oh$bgraph

	frame $bgraph -bg black
	frame $bgraph.inner0 -width 8 -height 8 -bg green
	pack $bgraph.inner0 -side left -padx 1 -fill both -expand true
	for {set i 1} {$i < 16} {incr i} {
		frame $bgraph.inner$i -width 8 -height 8 -bg black
		pack $bgraph.inner$i -side left -padx 1 -fill both -expand true
	}
	set oh$bgraph 0
}

proc bargraphSetHeight {bgraph height} {
	upvar #0 oh$bgraph oh

	if {$oh > $height} {
		for {set i [expr $height + 1]} {$i <= $oh} {incr i} {
			$bgraph.inner$i config -bg black
		}
	} else {
		for {set i [expr $oh + 1]} {$i <= $height} {incr i} {
			if {$i > 12} {
				$bgraph.inner$i config -bg red
			} else {
				$bgraph.inner$i config -bg green
			}
		}
	}
	set oh $height
}

# Device input controls
frame .i
button         .i.mute   -highlightthickness 0 -padx 0 -pady 0 -text mute -command toggle_send -relief sunken
button         .i.device -highlightthickness 0 -padx 0 -pady 0 -command toggle_input_port -bitmap "mic_mute"
scale          .i.scale  -highlightthickness 0 -from 0 -to 99 -command set_gain -orient horizontal -relief raised
bargraphCreate .i.powermeter
pack .i.mute       -side left -fill y
pack .i.device     -side left -fill y
pack .i.powermeter -side top  -fill x
pack .i.scale      -side top  -fill x
pack .i -side top -fill x -expand 1

# Device output controls
frame .o
button         .o.mute   -highlightthickness 0 -padx 0 -pady 0 -text mute -command toggle_play 
button         .o.device -highlightthickness 0 -padx 0 -pady 0 -command toggle_output_port -bitmap "speaker"
scale          .o.scale  -highlightthickness 0 -from 0 -to 99 -command set_vol -orient horizontal -relief raised 
bargraphCreate .o.powermeter
pack .o.mute       -side left -fill y
pack .o.device     -side left -fill y
pack .o.powermeter -side top  -fill x
pack .o.scale      -side top  -fill x
pack .o -side top -fill x -expand 1

bind all <ButtonPress-3> "toggle_send"
bind all <ButtonRelease-3> "toggle_send"
bind all <q> {destroy .}

proc set_gain {value} {
	cb_send "input gain $value"
}

proc set_vol {value} {
	cb_send "output gain $value"
}

proc toggle_input_port {} {
  cb_send "toggle_input_port"
}

proc toggle_output_port {} {
  cb_send "toggle_output_port"
}

proc toggle_send {} {
  cb_send "toggle_send"
  if {[string compare [.i.mute cget -relief] raised] == 0} {
    .i.mute configure -relief sunken
  } else {
    .i.mute configure -relief raised
  }
}

proc toggle_play {} {
  cb_send "toggle_play"
  if {[string compare [.o.mute cget -relief] raised] == 0} {
    .o.mute configure -relief sunken
  } else {
    .o.mute configure -relief raised
  }
}

# 
# The following function deal with receiving messages from the conference bus. The code
# in ui.c will call cb_recv with the appropriate arguments when a message is received. 
#

proc cb_recv {cmd} {
  if [string match [info procs [lindex cb_recv_$cmd 0]] [lindex cb_recv_$cmd 0]] {
    eval cb_recv_$cmd 
  }
}

proc cb_recv_init {rat_addr ui_addr} {
	cb_send "primary    DVI"
	cb_send "redundancy DVI"
}

proc cb_recv_powermeter {type level} {
	# powermeter input  <value>
	# powermeter output <value>
	# powermeter <ssrc> <value>
	switch $type {
		input   {bargraphSetHeight .i.powermeter $level}
		output  {bargraphSetHeight .o.powermeter $level}
	 	default {}
	}
}

proc cb_recv_input {cmd args} {
	switch $cmd {
		gain	{.i.scale set $args}
		device	{.i.device configure -bitmap $args}
		mute    {.i.mute configure -relief sunken}
		unmute  {.i.mute configure -relief raised}
	}
}

proc cb_recv_output {cmd args} {
	switch $cmd {
		gain	{.o.scale set $args}
		device	{.o.device configure -bitmap $args}
		mute    {.o.mute configure -relief sunken}
		unmute  {.o.mute configure -relief raised}
	}
}

