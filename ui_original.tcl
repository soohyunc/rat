#
# Copyright (c) 1995,1996,1997 University College London
# All rights reserved.
#
# $Revision$
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

# Default conference bus addresses. These are overridden later...
set RAT_ADDR  "NONE"
set  MY_ADDR  "NONE"

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

set statsfont     -*-courier-medium-r-*-*-12-*-*-*-*-*-iso8859-1
set titlefont     -*-helvetica-medium-r-normal--14-*-p-*-iso8859-1
set infofont      -*-helvetica-medium-r-normal--12-*-p-*-iso8859-1
set smallfont     -*-helvetica-medium-r-normal--10-*-p-*-iso8859-1
set verysmallfont -*-courier-medium-o-normal--8-*-m-*-iso8859-1

set V(class) "Mbone Applications"
set V(app)   "rat"

set iht			16
set iwd 		190
set fw 			.l.t.list.f
set cancel_info_timer 	0
set num_ssrc		0
set DEBUG		0

# Commands to send message over the conference bus...
proc toggle_play {} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "toggle_play"
  if {[string compare [.r.c.vol.t1 cget -relief] raised] == 0} {
    .r.c.vol.t1 configure -relief sunken
  } else {
    .r.c.vol.t1 configure -relief raised
  }
}

proc toggle_send {} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "toggle_send"
}

proc redundancy {coding} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "redundancy $coding"
}

proc primary {coding} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "primary $coding"
}

proc set_vol {volume} {
  global RAT_ADDR
  if {$RAT_ADDR != "NONE"} {
    cb_send "U" $RAT_ADDR "output gain $volume"
  }
}

proc set_gain {gain} {
  global RAT_ADDR
  if {$RAT_ADDR != "NONE"} {
    cb_send "U" $RAT_ADDR "input gain $gain"
  }
}

proc toggle_input_port {} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "toggle_input_port"
}

proc toggle_output_port {} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "toggle_output_port"
}

proc silence {s} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "silence $s"
}

proc lecture {l} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "lecture $l"
}

proc agc {a} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "agc $a"
}

proc repair {r} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "repair $r"
}

proc output_mode {o} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "output mode $o"
}

proc powermeter {pm} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "powermeter $pm"
}

proc rate {r} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "rate $r"
}

proc InstallKey {key} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "update_key $key"
}

proc play {file} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "play $file"
}

proc rec {file} {
  global RAT_ADDR
  cb_send "U" $RAT_ADDR "rec $file"
}

# 
# The following function deal with receiving messages from the conference bus. The code
# in ui.c will call cb_recv with the appropriate arguments when a message is received. 
#

proc cb_recv {src cmd} {
  global DEBUG
  if [string match [info procs [lindex cb_recv_$cmd 0]] [lindex cb_recv_$cmd 0]] {
    eval cb_recv_$cmd 
  } else {
    if $DEBUG {
      puts stdout "ConfBus: ERROR unknown command $cmd"
    }
  }
}

proc cb_recv_init {rat_addr my_addr} {
	# RAT has initialised itself, and we're now ready to go. 
	# Perform any last minute initialisation...
	global RAT_ADDR MY_ADDR
	set RAT_ADDR $rat_addr
	set  MY_ADDR  $my_addr
}

proc cb_recv_agc {args} {
  global agc_var
  set agc_var $args
}

proc cb_recv_primary {args} {
  global prenc
  set prenc $args
}

proc cb_recv_redundancy {args} {
  global secenc
  set secenc $args
}

proc cb_recv_repair {args} {
  set repair_var $args
}

proc cb_recv_repair {val} {
	global repair_val
	set repair_val $val
}

proc cb_recv_powermeter {type level} {
	# powermeter input  <value>
	# powermeter output <value>
	# powermeter <ssrc> <value>
	switch $type {
		input   {bargraphSetHeight .r.c.gain.b2 $level}
		output  {bargraphSetHeight .r.c.vol.b1  $level}
	 	default {}
	}
}

proc cb_recv_input {cmd args} {
	switch $cmd {
		gain	{.r.c.gain.s2 set $args}
		device	{.r.c.gain.l2 configure -bitmap $args}
		mute    {.r.c.gain.t2 configure -relief sunken}
		unmute  {.r.c.gain.t2 configure -relief raised}
	}
}

proc cb_recv_output {cmd args} {
	switch $cmd {
		gain	{.r.c.vol.s1 set $args}
		device	{.r.c.vol.l1 configure -bitmap $args}
		mute    {.r.c.vol.t1 configure -relief sunken}
		unmute  {.r.c.vol.t1 configure -relief raised}
	}
}

proc cb_recv_half_duplex {} {
	set output_var {Mike mutes net}
	output_mode $output_var
}

proc cb_recv_debug {} {
	global DEBUG
	set DEBUG 1
	.r.b.ucl configure -background salmon
	.r.b.v   configure -background salmon
}

proc cb_recv_address {addr port ttl} {
	.b.a.address configure -text "Dest: $addr  Port: $port  TTL: $ttl"
}

proc cb_recv_lecture_mode {mode} {
	set lecture_var $mode
}

proc cb_recv_detect_silence {mode} {
	set silence_var $mode
}

proc cb_recv_my_ssrc {ssrc} {
	global my_ssrc rtcp_name rtcp_email rtcp_phone rtcp_loc
	global RAT_ADDR
	set my_ssrc $ssrc

	# Now we know our SSRC, we can inform RAT of our SDES information...
	cb_send "U" $RAT_ADDR "ssrc $ssrc name  $rtcp_name"
	cb_send "U" $RAT_ADDR "ssrc $ssrc email $rtcp_email"
	cb_send "U" $RAT_ADDR "ssrc $ssrc phone $rtcp_phone"
	cb_send "U" $RAT_ADDR "ssrc $ssrc loc   $rtcp_loc"
}

proc cb_recv_ssrc {ssrc args} {
	global CNAME NAME EMAIL LOC PHONE TOOL num_ssrc fw iht losstimers my_ssrc
	global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME INDEX
	set cmd [lindex $args 0]
	set arg [lrange $args 1 end]
	if {[array names INDEX $ssrc] != $ssrc} {
		# This is an SSRC we've not seen before...
		set        CNAME($ssrc) "unknown"
		set         NAME($ssrc) "unknown"
		set        EMAIL($ssrc) ""
		set        PHONE($ssrc) ""
		set          LOC($ssrc) ""
		set         TOOL($ssrc) ""
		set     ENCODING($ssrc) "unknown"
		set     DURATION($ssrc) ""
		set   PCKTS_RECV($ssrc) "0"
		set   PCKTS_LOST($ssrc) "0"
		set   PCKTS_MISO($ssrc) "0"
		set  JITTER_DROP($ssrc) "0"
		set       JITTER($ssrc) "0"
		set   LOSS_TO_ME($ssrc) "101"
		set LOSS_FROM_ME($ssrc) "101"
		set        INDEX($ssrc) $num_ssrc
		incr num_ssrc
		chart_enlarge $num_ssrc 
	}
	switch $cmd {
		cname {
			set CNAME($ssrc) $arg
			if {[string compare NAME($ssrc) "unknown"] != 0} {
				set NAME($ssrc) $arg
			}
			chart_label $ssrc
		}
		name            { 
			set NAME($ssrc) $arg
			chart_label $ssrc
		}
		email           { set EMAIL($ssrc) $arg}
		phone           { set PHONE($ssrc) $arg}
		loc             { set LOC($ssrc) $arg}
		tool            { set TOOL($ssrc) $arg}
		encoding        { set ENCODING($ssrc) $arg}
		packet_duration { set DURATION($ssrc) $arg}
		packets_recv    { set PCKTS_RECV($ssrc) $arg}
		packets_lost    { set PCKTS_LOST($ssrc) $arg}
		packets_miso    { set PCKTS_MISO($ssrc) $arg}
		jitter_drop     { set JITTER_DROP($ssrc) $arg}
		jitter          { set JITTER($ssrc) $arg}
		loss_to_me      { 
			set LOSS_TO_ME($ssrc) $arg 
			set srce $ssrc
			set dest $my_ssrc
			set loss [lindex $args 1]
			catch {after cancel $losstimers($srce,$dest)}
			chart_set $srce $dest $loss
			set losstimers($srce,$dest) [after 30000 "chart_set $srce $dest 101"]
		}
		loss_from_me    { 
			set LOSS_FROM_ME($ssrc) $arg 
			set srce $my_ssrc
			set dest $ssrc
			set loss [lindex $args 1]
			catch {after cancel $losstimers($srce,$dest)}
			chart_set $srce $dest $loss
			set losstimers($srce,$dest) [after 30000 "chart_set $srce $dest 101"]
		}
		loss_from	{ 
			set dest $ssrc
			set srce [lindex $args 1]
			set loss [lindex $args 2]
			catch {after cancel $losstimers($srce,$dest)}
			chart_set $srce $dest $loss
			set losstimers($srce,$dest) [after 30000 "chart_set $srce $dest 101"]
		}
		active          {
			if {[string compare $arg "now"] == 0} {
				catch [$fw.c$ssrc configure -background white]
			}
			if {[string compare $arg "recent"] == 0} {
				catch [$fw.c$ssrc configure -background gray90]
			}
		}
		inactive {
			catch [$fw.c$ssrc configure -background gray80]
		}	
		remove {
			catch [destroy $fw.c$ssrc]
			unset CNAME($ssrc) NAME($ssrc) EMAIL($ssrc) PHONE($ssrc) LOC($ssrc) TOOL($ssrc)
			unset ENCODING($ssrc) DURATION($ssrc) PCKTS_RECV($ssrc) PCKTS_LOST($ssrc) PCKTS_MISO($ssrc)
			unset JITTER_DROP($ssrc) JITTER($ssrc) LOSS_TO_ME($ssrc) LOSS_FROM_ME($ssrc) INDEX($ssrc)
			incr num_ssrc -1
			chart_redraw $num_ssrc
			# Make sure we don't try to update things later...
			return
		}
		mute {
			$fw.c$ssrc create line [expr $iht + 2] [expr $iht / 2] 500 [expr $iht / 2] -tags a -width 2.0 -fill gray95
		}
		unmute {
			catch [$fw.c$ssrc delete a]
		}
		default {
			puts stdout "WARNING: ConfBus message 'ssrc $ssrc $cmd $arg' not understood"
		}
	}
	ssrc_update $ssrc
}



proc ssrc_update {ssrc} {
	global CNAME NAME EMAIL LOC PHONE TOOL 
	global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME
	global fw iht iwd my_ssrc mylosstimers his_or_her_losstimers

	set cw 		$fw.c$ssrc

	if {[winfo exists $cw]} {
		$cw itemconfigure t -text $NAME($ssrc)
	} else {
		set thick 0
		set l $thick
		set h [expr $iht / 2 + $thick]
		set f [expr $iht + $thick]
		canvas $cw -width $iwd -height $f -highlightthickness $thick
		$cw create text [expr $f + 2] $h -anchor w -text $NAME($ssrc) -fill black -tag t
		$cw create polygon $l $h $h $l $h $f -outline black -fill grey50 -tag m
		$cw create polygon $f $h $h $l $h $f -outline black -fill grey -tag h

		bind $cw <Button-1> "toggle_stats $ssrc"
		bind $cw <Button-2> "toggle_mute $cw $ssrc"
	}

	# XXX This is not very efficient
	if {[info exists my_ssrc] && [string compare $ssrc $my_ssrc]} {
		foreach i [pack slaves $fw] {
			set u [string toupper $NAME($ssrc)]
			if {[string compare $u [string toupper [$i itemcget t -text]]] < 0 && [string compare $i $fw.c$my_ssrc] != 0} {
				pack $cw -before $i
				break
			}
		}
	} else {
		if {[pack slaves $fw] != ""} {
			pack $cw -before [lindex [pack slaves $fw] 0]
		}
	}
	pack $cw -fill x

	fix_scrollbar
	update_stats $ssrc

	if {$LOSS_TO_ME($ssrc) < 5} {
		catch [$fw.c$ssrc itemconfigure m -fill green]
	} elseif {$LOSS_TO_ME($ssrc) < 10} {
		catch [$fw.c$ssrc itemconfigure m -fill orange]
	} elseif {$LOSS_TO_ME($ssrc) <= 100} {
		catch [$fw.c$ssrc itemconfigure m -fill red]
	} else {
		catch [$fw.c$ssrc itemconfigure m -fill grey50]
	}
	catch {after cancel $mylosstimers($ssrc)}
	if {$LOSS_TO_ME($ssrc) <= 100} {
		set mylosstimers($ssrc) [after 10000 "set LOSS_TO_ME($ssrc) 101; ssrc_update $ssrc"]
	}

	if {$LOSS_FROM_ME($ssrc) < 5} {
		catch [$fw.c$ssrc itemconfigure h -fill green]
	} elseif {$LOSS_FROM_ME($ssrc) < 10} {
		catch [$fw.c$ssrc itemconfigure h -fill orange]
	} elseif {$LOSS_FROM_ME($ssrc) <= 100} {
		catch [$fw.c$ssrc itemconfigure h -fill red]
	} else {
		catch [$fw.c$ssrc itemconfigure h -fill grey]
	}
	catch {after cancel $his_or_her_losstimers($ssrc)}
	if {$LOSS_FROM_ME($ssrc)<=100} {
		set his_or_her_losstimers($ssrc) [after 30000 "set LOSS_FROM_ME($ssrc) 101; ssrc_update $ssrc"]
	}
}

#power meters
proc bargraphCreate {bgraph} {
	global oh$bgraph

	frame $bgraph -bg black
	frame $bgraph.inner0 -width 8 -height 4 -bg green
	pack $bgraph.inner0 -side bottom -pady 1 -fill both -expand true
	for {set i 1} {$i < 16} {incr i} {
		frame $bgraph.inner$i -width 8 -height 4 -bg black
		pack $bgraph.inner$i -side bottom -pady 1 -fill both -expand true
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

#dropdown list
proc dropdown {w varName command args} {

    global $varName 
    set firstValue [lindex $args 0]

    if ![info exists $varName] {
	set $varName $firstValue
    }

    menubutton $w -textvariable var -indicatoron 0 -menu $w.menu -text $firstValue -textvariable $varName -relief raised 
    menu $w.menu -tearoff 0
    foreach i $args {
	$w.menu add radiobutton -variable $varName -label $i -value $i -command "$command [lindex $i 0]" 
    }
    return $w.menu
}

proc toggle_mute {cw ssrc} {
	global RAT_ADDR
	global iht
	if {[$cw gettags a] == ""} {
		cb_send "U" $RAT_ADDR "ssrc $ssrc mute"
	} else {
		cb_send "U" $RAT_ADDR "ssrc $ssrc unmute"
	}
}

proc fix_scrollbar {} {
	global iht iwd fw

	set ch [expr $iht * ([llength [pack slaves $fw]] + 2)]
	set bh [winfo height .l.t.scr]
	if {$ch > $bh} {set h $ch} else {set h $bh}
	.l.t.list configure -scrollregion "0.0 0.0 $iwd $h"
}

proc info_timer {} {
	global cancel_info_timer
	if {$cancel_info_timer == 1} {
		set cancel_info_timer 0
	} else {
		update_rec_info
		after 1000 info_timer
	}
}

proc update_stats {ssrc} {
	global CNAME NAME EMAIL LOC PHONE TOOL
	global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME

	if {$LOSS_TO_ME($ssrc) == 101} {
		set loss_to_me "unknown"
	} else {
		set loss_to_me "$LOSS_TO_ME($ssrc)%"
	}

	if {$LOSS_FROM_ME($ssrc) == 101} {
		set loss_from_me "unknown"
	} else {
		set loss_from_me "$LOSS_FROM_ME($ssrc)%"
	}

	if {[winfo exists .stats$ssrc]} {
		.stats$ssrc.m configure -text " Name:                    $NAME($ssrc)\n\
	                                	Email:                   $EMAIL($ssrc)\n\
				        	Phone:                   $PHONE($ssrc)\n\
				        	Location:                $LOC($ssrc)\n\
				        	Tool:                    $TOOL($ssrc)\n\
				        	CNAME:                   $CNAME($ssrc)\n\
				        	Audio Encoding:          $ENCODING($ssrc)\n\
				        	Audio Length:            $DURATION($ssrc)\n\
				        	Packets Received:        $PCKTS_RECV($ssrc)\n\
				        	Packets Lost:            $PCKTS_LOST($ssrc)\n\
				        	Packets Misordered:      $PCKTS_MISO($ssrc)\n\
				        	Units Dropped (jitter):  $JITTER_DROP($ssrc)\n\
				        	Network Timing Jitter:   $JITTER($ssrc)\n\
				        	Instantaneous Loss Rate: $loss_to_me\n\
						Loss from me:            $loss_from_me"
	}
}

proc toggle_stats {ssrc} {
	global statsfont
	if {[winfo exists .stats$ssrc]} {
		destroy .stats$ssrc
	} else {
		# Window does not exist so create it
		toplevel .stats$ssrc
		message .stats$ssrc.m -width 600 -font $statsfont
		pack .stats$ssrc.m -side top
		button .stats$ssrc.d -highlightthickness 0 -padx 0 -pady 0 -text "Dismiss" -command "destroy .stats$ssrc" 
		pack .stats$ssrc.d -side bottom -fill x
		wm title .stats$ssrc "RAT user info"
		wm resizable .stats$ssrc 0 0
		update_stats $ssrc
	}
}

# Initialise RAT MAIN window
frame .r 
frame .l 
frame .l.t -relief raised
scrollbar .l.t.scr -relief flat -highlightthickness 0 -command ".l.t.list yview"
canvas .l.t.list -highlightthickness 0 -bd 0 -relief raised -width $iwd -yscrollcommand ".l.t.scr set" -yscrollincrement $iht
frame .l.t.list.f -highlightthickness 0 -bd 0
.l.t.list create window 0 0 -anchor nw -window .l.t.list.f
frame  .l.s1 -bd 0
button .l.s1.opts  -highlightthickness 0 -padx 0 -pady 0 -text "Options" -command {wm deiconify .b}
button .l.s1.about -highlightthickness 0 -padx 0 -pady 0 -text "About"   -command {wm deiconify .about}
button .l.s1.quit  -highlightthickness 0 -padx 0 -pady 0 -text "Quit"    -command {destroy .}
frame  .l.s2 -bd 0
button .l.s2.stats -highlightthickness 0 -padx 0 -pady 0 -text "Reception Quality" -command {wm deiconify .chart}
button .l.s2.audio -highlightthickness 0 -padx 0 -pady 0 -text "Get Audio"         -command {cb_send "U" $RAT_ADDR get_audio}

pack .r -side right -fill y
frame .r.c
pack .r.c -side top -fill y -expand 1
frame .r.c.vol
frame .r.c.gain
pack .r.c.vol -side left -fill y
pack .r.c.gain -side right -fill y

pack .l -side left -fill both -expand 1
pack .l.s1 -side bottom -fill x
pack .l.s1.opts .l.s1.about .l.s1.quit -side left -fill x -expand 1
pack .l.s2 -side bottom -fill x
pack .l.s2.stats .l.s2.audio -side left -fill x -expand 1
pack .l.t -side top -fill both -expand 1
pack .l.t.scr -side left -fill y
pack .l.t.list -side left -fill both -expand 1
bind .l.t.list <Configure> {fix_scrollbar}

# Device output controls
button .r.c.vol.t1 -highlightthickness 0 -padx 0 -pady 0 -text mute -command toggle_play 
button .r.c.vol.l1 -highlightthickness 0 -padx 0 -pady 0 -command toggle_output_port -bitmap "speaker"
bargraphCreate .r.c.vol.b1
scale .r.c.vol.s1 -highlightthickness 0 -font $verysmallfont -from 99 -to 0 -command set_vol -orient vertical -relief raised 

pack .r.c.vol.t1 -side top -fill x
pack .r.c.vol.l1 -side top -fill x
pack .r.c.vol.b1 -side left -fill y
pack .r.c.vol.s1 -side right -fill y

# Device input controls
button .r.c.gain.t2 -highlightthickness 0 -padx 0 -pady 0 -text mute -command toggle_send -relief sunken
button .r.c.gain.l2 -highlightthickness 0 -padx 0 -pady 0 -command toggle_input_port -bitmap "mic_mute"
bargraphCreate .r.c.gain.b2
scale .r.c.gain.s2 -highlightthickness 0 -font $verysmallfont -from 99 -to 0 -command set_gain -orient vertical -relief raised 

pack .r.c.gain.t2 -side top -fill x
pack .r.c.gain.l2 -side top -fill x
pack .r.c.gain.b2 -side left -fill y
pack .r.c.gain.s2 -side right -fill y

proc cb_recv_disable_audio_ctls {} {
	.r.c.vol.t1 configure -state disabled
	.r.c.vol.l1 configure -state disabled
	.r.c.vol.s1 configure -state disabled
	.r.c.gain.t2 configure -state disabled
	.r.c.gain.l2 configure -state disabled
	.r.c.gain.s2 configure -state disabled
	.l.s2.audio  configure -state normal
}

proc cb_recv_enable_audio_ctls {} {
	.r.c.vol.t1 configure -state normal
	.r.c.vol.l1 configure -state normal
	.r.c.vol.s1 configure -state normal
	.r.c.gain.t2 configure -state normal
	.r.c.gain.l2 configure -state normal
	.r.c.gain.s2 configure -state normal
	.l.s2.audio  configure -state disabled
}

frame .r.b -relief raised 
pack .r.b -side bottom -fill x
label .r.b.v -highlightthickness 0 -bd 0 -font $smallfont -text $ratversion
pack .r.b.v -side bottom -fill x
label .r.b.ucl -highlightthickness 0 -bd 0 -bitmap "ucl"
pack .r.b.ucl -side bottom -fill x

bind all <ButtonPress-3> "toggle_send"
bind all <ButtonRelease-3> "toggle_send"
bind all <q> {destroy .}

wm iconbitmap . rat_small
wm resizable . 1 1
if ([info exists geometry]) {
        wm geometry . $geometry
}

# Initialise CONTROL toplevel window
toplevel .b
wm withdraw .b

set rate_var 	"40 ms"
set output_var 	"Full duplex"
set redun 	0
set sync_var 	0
set meter_var	1

frame .b.f
pack .b.f -side top -fill x

# packet format options
frame .b.f.pkt 
pack  .b.f.pkt -side top -fill both -expand 1
label .b.f.pkt.l -highlightthickness 0 -text "Packet Format"
pack  .b.f.pkt.l
# length
frame .b.f.pkt.len -relief sunken
pack  .b.f.pkt.len -side left -fill x
label .b.f.pkt.len.l -highlightthickness 0 -justify left -text  "Duration"
pack  .b.f.pkt.len.l -side top -fill both -expand 1
dropdown .b.f.pkt.len.dl rate_var rate "20 ms" "40 ms" "80 ms"  "160 ms"
pack     .b.f.pkt.len.dl -side left -fill x -expand 1
# primary
frame .b.f.pkt.pr -relief sunken
pack  .b.f.pkt.pr -side left -fill x
label .b.f.pkt.pr.l -highlightthickness 0 -justify left -text  "Primary Encoding"
pack  .b.f.pkt.pr.l -side top -fill both -expand 1
dropdown .b.f.pkt.pr.dl prenc primary WBS "16-bit linear" "PCM (mu-law)" DVI GSM LPC
pack  .b.f.pkt.pr.dl -side left -fill x -expand 1

# secondary
frame .b.f.pkt.sec -relief sunken
pack  .b.f.pkt.sec -side left -fill x
label .b.f.pkt.sec.l -highlightthickness 0 -justify left -text  "Secondary Encoding"
pack  .b.f.pkt.sec.l -side top -fill both -expand 1
dropdown .b.f.pkt.sec.dl secenc redundancy NONE "PCM (mu-law)" DVI GSM LPC
pack  .b.f.pkt.sec.dl -side left -fill x -expand 1

# Local Options
frame .b.f.loc
pack  .b.f.loc -side top -fill both -expand 1
label .b.f.loc.l -highlightthickness 0 -text "Local Options"
pack  .b.f.loc.l -side top -fill x -expand 1

# Mode

frame .b.f.loc.mode -relief sunken -width 500
pack  .b.f.loc.mode -side left -fill x -expand 0
label .b.f.loc.mode.l -highlightthickness 0 -justify left -text  "Mode                "
pack  .b.f.loc.mode.l -side top -expand 1 -anchor w
dropdown .b.f.loc.mode.dl output_var output "Net mutes mike" "Mike mutes net" "Full duplex"
pack  .b.f.loc.mode.dl -side left -fill x -expand 1

# Receiver Repair Options
frame .b.f.loc.rep -relief sunken
pack  .b.f.loc.rep -side left  -fill x -expand 1
label .b.f.loc.rep.l -highlightthickness 0 -justify left -text "Loss Repair"
pack  .b.f.loc.rep.l -side top -expand 1 -anchor w
dropdown .b.f.loc.rep.dl repair_var repair None "Packet Repetition" "Pattern Matching"
pack  .b.f.loc.rep.dl -side left -fill x -expand 1

# Misc controls
frame .b.f2 -bd 0
pack .b.f2 -side top -fill x

# Generic toggles
frame .b.f2.l -relief sunken 
pack  .b.f2.l -side left -expand 1 -fill both
frame .b.f2.r -relief sunken 
pack  .b.f2.r -side left -expand 1 -fill both
checkbutton .b.f2.l.sil   -anchor w -highlightthickness 0 -relief flat -text "Suppress Silence"       -variable silence_var -command {silence    $silence_var}
checkbutton .b.f2.l.meter -anchor w -highlightthickness 0 -relief flat -text "Powermeters"            -variable meter_var   -command {powermeter $meter_var}
checkbutton .b.f2.l.lec   -anchor w -highlightthickness 0 -relief flat -text "Lecture Mode"           -variable lecture_var -command {lecture    $lecture_var}
checkbutton .b.f2.r.agc   -anchor w -highlightthickness 0 -relief flat -text "Automatic Gain Control" -variable agc_var     -command {agc        $agc_var}
checkbutton .b.f2.r.syn   -anchor w -highlightthickness 0 -relief flat -text "Video Synchronisation"  -variable sync_var    -command {sync       $sync_var} -state disabled
checkbutton .b.f2.r.afb   -anchor w -highlightthickness 0 -relief flat -text "Acoustic Feedback"      -variable afb_var     -command {afb        $afb_var} -state disabled

pack .b.f2.l.sil   -side top -fill x -expand 1
pack .b.f2.l.meter -side top -fill x -expand 1
pack .b.f2.l.lec   -side top -fill x -expand 0
pack .b.f2.r.syn   -side top -fill x -expand 0
pack .b.f2.r.agc   -side top -fill x -expand 1
pack .b.f2.r.afb   -side top -fill x -expand 0

#Session Key
frame .b.crypt -bd 0
pack  .b.crypt -side top -fill x -expand 1
label .b.crypt.l -text Encryption 
pack  .b.crypt.l

label .b.crypt.kn -highlightthickness 0 -text "Key:"
pack .b.crypt.kn -side left -fill x
entry .b.crypt.name -highlightthickness 0 -width 20 -relief sunken -textvariable key
bind .b.crypt.name <Return> {UpdateKey $key .b.crypt }
bind .b.crypt.name <BackSpace> { CheckKeyErase $key .b.crypt }
bind .b.crypt.name <Any-Key> { KeyEntryCheck $key .b.crypt "%A" }
bind .b.crypt.name <Control-Key-h> { CheckKeyErase $key .b.crypt }
pack .b.crypt.name -side left -fill x -expand 1

checkbutton .b.crypt.enc -state disabled -highlightthickness 0 -relief flat -text "On/Off" -variable key_var -command {ToggleKey $key_var}
pack .b.crypt.enc -side left -fill x 


proc DisableSessionKey w {
    global key_var
 
    set key_var 0
    $w.enc configure -state disabled
}

proc EnableSessionKey w {
    global key_var

    $w.enc configure -state normal
}

proc KeyEntryCheck { key w key_val } {

  if { $key_val != "{}" } {
    if { [string length $key] == 0 } {
	EnableSessionKey $w
    }
  }
} 

proc CheckKeyErase { key w } {

  if { [string length $key] == 1 } {
    UpdateKey "" $w 
  }
}

proc UpdateKey { key w } {
  global key_var
 
  if { $key == "" } {
    DisableSessionKey $w
  } else {
    EnableSessionKey $w
    set key_var 1
  }
  InstallKey $key
}
  
proc ToggleKey { encrypt } {
  global key 

  if !($encrypt) {
    InstallKey ""
  } else {
    InstallKey $key
  }
}

set cryptpos [lsearch $argv "-crypt"]
if {$cryptpos == -1} then {
  set key_var 0
} else {
  set key   [lindex $argv [expr $cryptpos + 1]]
  UpdateKey [lindex $argv [expr $cryptpos + 1]] .b.crypt
}

# File options
frame .b.f3
pack .b.f3 -side top -fill x

# Play
set play_file infile.rat
frame .b.f3.p -relief sunken 
pack .b.f3.p -side left -fill both -expand 1
label .b.f3.p.l -highlightthickness 0 -text "Play file"
pack .b.f3.p.l -side top -fill x
frame .b.f3.p.f -bd 0
pack .b.f3.p.f -side top -fill x
label .b.f3.p.f.l -highlightthickness 0 -text "File:"
pack .b.f3.p.f.l -side left -fill x
entry .b.f3.p.f.file -highlightthickness 0 -width 10 -relief sunken -textvariable play_file
pack .b.f3.p.f.file -side right -fill x -expand 1
frame .b.f3.p.f2 -bd 0
pack .b.f3.p.f2 -side top -fill x
button .b.f3.p.f2.on -padx 0 -pady 0 -highlightthickness 0 -text "Start" -command {play $play_file}
pack .b.f3.p.f2.on -side left -fill x -expand 1
button .b.f3.p.f2.off -padx 0 -pady 0 -highlightthickness 0 -text "Stop" -command {play stop}
pack .b.f3.p.f2.off -side right -fill x -expand 1

# Record
set rec_file outfile.rat
frame .b.f3.r -relief sunken
pack .b.f3.r -side right -fill both -expand 1
label .b.f3.r.l -highlightthickness 0 -text "Record file"
pack .b.f3.r.l -side top -fill x
frame .b.f3.r.f -bd 0
pack .b.f3.r.f -side top -fill x
label .b.f3.r.f.l -highlightthickness 0 -text "File:"
pack .b.f3.r.f.l -side left -fill x
entry .b.f3.r.f.file -highlightthickness 0 -width 10 -relief sunken -textvariable rec_file
pack .b.f3.r.f.file -side right -fill x -expand 1
frame .b.f3.r.f2 -bd 0
pack .b.f3.r.f2 -side top -fill x
button .b.f3.r.f2.on -padx 0 -pady 0 -highlightthickness 0 -text "Start" -command {rec $rec_file}
pack .b.f3.r.f2.on -side left -fill x -expand 1
button .b.f3.r.f2.off -padx 0 -pady 0 -highlightthickness 0 -text "Stop" -command {rec stop}
pack .b.f3.r.f2.off -side right -fill x -expand 1

# Address...
frame .b.a -relief sunken
pack  .b.a -side top -fill x
label .b.a.l -highlightthickness 0 -text "RTP Configuration"
pack  .b.a.l -side top
label .b.a.address -highlightthickness 0 -text "Group: Port: TTL:"
pack  .b.a.address -side top -fill x
frame .b.a.rn -bd 0
pack  .b.a.rn -side top -fill x
entry .b.a.rn.name -highlightthickness 0 -width 35 -relief sunken -textvariable rtcp_name
bind  .b.a.rn.name <Return> {cb_send "U" $RAT_ADDR "ssrc $my_ssrc name $rtcp_name"; savename}
bind  .b.a.rn.name <Tab>    {cb_send "U" $RAT_ADDR "ssrc $my_ssrc name $rtcp_name"; savename}
pack  .b.a.rn.name -side right -fill x 
label .b.a.rn.l -highlightthickness 0 -text "Name:"
pack  .b.a.rn.l -side left -fill x -expand 1
frame .b.a.re -bd 0
pack  .b.a.re -side top -fill x
entry .b.a.re.name -highlightthickness 0 -width 35 -relief sunken -textvariable rtcp_email
bind  .b.a.re.name <Return> {cb_send "U" $RAT_ADDR "ssrc $my_ssrc email $rtcp_email"; savename}
bind  .b.a.re.name <Tab>    {cb_send "U" $RAT_ADDR "ssrc $my_ssrc email $rtcp_email"; savename}
pack  .b.a.re.name -side right -fill x
label .b.a.re.l -highlightthickness 0 -text "Email:"
pack  .b.a.re.l -side left -fill x -expand 1
frame .b.a.rp -bd 0
pack  .b.a.rp -side top -fill x
entry .b.a.rp.name -highlightthickness 0 -width 35 -relief sunken -textvariable rtcp_phone
bind  .b.a.rp.name <Return> {cb_send "U" $RAT_ADDR "ssrc $my_ssrc phone $rtcp_phone"; savename}
bind  .b.a.rp.name <Tab>    {cb_send "U" $RAT_ADDR "ssrc $my_ssrc phone $rtcp_phone"; savename}
pack  .b.a.rp.name -side right -fill x
label .b.a.rp.l -highlightthickness 0 -text "Phone:"
pack  .b.a.rp.l -side left -fill x -expand 1
frame .b.a.rl -bd 0
pack  .b.a.rl -side top -fill x
entry .b.a.rl.name -highlightthickness 0 -width 35 -relief sunken -textvariable rtcp_loc
bind  .b.a.rl.name <Return> {cb_send "U" $RAT_ADDR "ssrc $my_ssrc loc $rtcp_loc"; savename}
bind  .b.a.rl.name <Tab>    {cb_send "U" $RAT_ADDR "ssrc $my_ssrc loc $rtcp_loc"; savename}
pack  .b.a.rl.name -side right -fill x
label .b.a.rl.l -highlightthickness 0 -text "Location:"
pack  .b.a.rl.l -side left -fill x -expand 1

label .b.rat -bitmap rat2 -relief sunken
pack .b.rat -fill x -expand 1

button .b.d -highlightthickness 0 -padx 0 -pady 0 -text "Dismiss" -command "wm withdraw .b"
pack .b.d -side bottom -fill x

wm title .b "RAT controls"
wm resizable .b 0 0

# Initialise ABOUT toplevel window
toplevel .about
wm withdraw .about
frame .about.a
label .about.a.b -highlightthickness 0 -bitmap rat_med
message .about.a.m -text {
RAT Development Team:
    Angela Sasse
    Vicky Hardman
    Isidor Kouvelas
    Colin Perkins
    Orion Hodson
    Darren Harris
    Anna Watson 
    Mark Handley
    Jon Crowcroft
    Anna Bouch
}
message .about.b -width 1000 -text {
The Robust-Audio Tool has been written in the Department of Computer
Science, University College London, UK. The work was supported by projects: 

   - Multimedia Integrated Conferencing for Europe (MICE)
   - Multimedia European Research in Conferencing Integration (MERCI)
   - Remote Language Teaching for SuperJANET (ReLaTe)
   - Robust Audio Tool (RAT)

Further information is available on the world-wide-web, please access URL:
http://www-mice.cs.ucl.ac.uk/mice/rat/

Please send comments/suggestions/bug-reports to <rat-trap@cs.ucl.ac.uk>
}
button .about.c -highlightthickness 0 -padx 0 -pady 0 -text Copyright -command {wm deiconify .copyright}
button .about.d -highlightthickness 0 -padx 0 -pady 0 -text Dismiss   -command {wm withdraw  .about}
pack .about.a .about.b .about.c .about.d -side top -fill x
pack .about.a.b -side left -fill y
pack .about.a.m -side right -fill both -expand 1
wm title .about "About RAT"
wm resizable .about 0 0
 
toplevel .copyright
wm withdraw .copyright
message   .copyright.m -text {
 Robust-Audio Tool (RAT)
 
 Copyright (C) 1995,1996,1997 University College London
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, is permitted, for non-commercial use only, provided 
 that the following conditions are met:
 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
 3. All advertising materials mentioning features or use of this software
    must display the following acknowledgement:
      This product includes software developed by the Computer Science
      Department at University College London
 4. Neither the name of the University nor of the Department may be used
    to endorse or promote products derived from this software without
    specific prior written permission.
 Use of this software for commercial purposes is explicitly forbidden
 unless prior written permission is obtained from the authors. 
 
 THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 SUCH DAMAGE.
 
 Modifications for HP-UX by Terje Vernly <terjeve@usit.uio.no> and 
 Geir Harald Hansen <g.h.hansen@usit.uio.no>.

 This software is derived, in part, from publically available source code
 with the following copyright:
 
 Copyright (c) 1991 University of Southern California
 Copyright (c) 1993,1994 AT&T Bell Laboratories
 Copyright (c) 1991-1993,1996 Regents of the University of California
 Copyright (c) 1992 Stichting Mathematisch Centrum, Amsterdam
 Copyright (c) 1991,1992 RSA Data Security, Inc
 Copyright (c) 1992 Jutta Degener and Carsten Bormann, Technische Universitaet Berlin
 Copyright (c) 1994 Henning Schulzrinne
 Copyright (c) 1994 Paul Stewart
 
 This product includes software developed by the Computer Systems
 Engineering Group and by the Network Research Group at Lawrence 
 Berkeley Laboratory.
 
 Encryption features of this software use the RSA Data Security, Inc. 
 MD5 Message-Digest Algorithm.
} 
button .copyright.d -highlightthickness 0 -padx 0 -pady 0 -text Dismiss   -command {wm withdraw  .copyright}
pack   .copyright.m .copyright.d -side top -fill x -expand 1
wm title     .copyright "RAT Copyright"
wm resizable .copyright 0 0

if {[glob ~] == "/"} {
	set rtpfname /.RTPdefaults
} else {
	set rtpfname ~/.RTPdefaults
}


proc savename {} {
    global rtpfname rtcp_name rtcp_email rtcp_phone rtcp_loc V win32
    if {$win32} {
	putregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpName"  "$rtcp_name"
	putregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpEmail"  "$rtcp_email"
	putregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpPhone"  "$rtcp_phone"
	putregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpLoc"  "$rtcp_loc"
    } else {
	set f [open $rtpfname w]
	if {$rtcp_name  != ""} {puts $f "*rtpName:  $rtcp_name"}
	if {$rtcp_email != ""} {puts $f "*rtpEmail: $rtcp_email"}
	if {$rtcp_phone != ""} {puts $f "*rtpPhone: $rtcp_phone"}
	if {$rtcp_loc   != ""} {puts $f "*rtpLoc:   $rtcp_loc"}
	close $f
    }
}

set rtcp_name  [option get . rtpName  rat]
set rtcp_email [option get . rtpEmail rat]
set rtcp_phone [option get . rtpPhone rat]
set rtcp_loc   [option get . rtpLoc   rat]

if {$win32 == 0} {
    if {$rtcp_name == "" && [file readable $rtpfname] == 1} {
	set f [open $rtpfname]
	while {[eof $f] == 0} {
	    gets $f line
	    if {[string compare "*rtpName:"  [lindex $line 0]] == 0} {set rtcp_name  [lrange $line 1 end]}
	    if {[string compare "*rtpEmail:" [lindex $line 0]] == 0} {set rtcp_email [lrange $line 1 end]}
	    if {[string compare "*rtpPhone:" [lindex $line 0]] == 0} {set rtcp_phone [lrange $line 1 end]}
	    if {[string compare "*rtpLoc:"   [lindex $line 0]] == 0} {set rtcp_loc   [lrange $line 1 end]}
	}
	close $f
    }
} else {
    if {$rtcp_name == ""} {
	catch {set rtcp_name  [getregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpName"]  } 
	catch {set rtcp_email [getregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpEmail"] } 
	catch {set rtcp_phone [getregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpPhone"] } 
	catch {set rtcp_loc   [getregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpLoc"  ] } 
    }
}

if {$rtcp_name == ""} {
    toplevel .name
    wm title .name "RAT User Information"
    message .name.m -width 1000 -text {
	Please enter the following details, for transmission
	to other conference participants.
    }
	frame  .name.b
    label  .name.b.res -text "Name:"
    entry  .name.b.e -highlightthickness 0 -width 20 -relief sunken -textvariable rtcp_name
    button .name.d -highlightthickness 0 -padx 0 -pady 0 -text Done -command {rtcp_name $rtcp_name; savename; destroy .name}
    bind   .name.b.e <Return> {rtcp_name $rtcp_name; savename; destroy .name}
    
    pack .name.m -side top -fill x -expand 1
    pack .name.b -side top -fill x -expand 1
    pack .name.b.res -side left
    pack .name.b.e -side right -fill x -expand 1
    pack .name.d -side bottom -fill x -expand 1
    wm resizable .name 0 0
    update
    raise .name .
}

#
# Routines to display the "chart" of RTCP RR statistics...
#

set chart_font    6x10
set chart_size    1
set chart_boxsize 15
set chart_xoffset 180
set chart_yoffset 17

toplevel  .chart
canvas    .chart.c  -background white  -xscrollcommand {.chart.sb set} -yscrollcommand {.chart.sr set} 
scrollbar .chart.sr -orient vertical   -command {.chart.c yview}
scrollbar .chart.sb -orient horizontal -command {.chart.c xview}
button    .chart.d  -text "Dismiss"    -command {wm withdraw .chart} -padx 0 -pady 0

pack .chart.d  -side bottom -fill x    -expand 0 -anchor s
pack .chart.sb -side bottom -fill x    -expand 0 -anchor s
pack .chart.sr -side right  -fill y    -expand 0 -anchor e
pack .chart.c  -side left   -fill both -expand 1 -anchor n

# Add a few labels to the chart...
.chart.c create text 2 [expr ($chart_boxsize / 2) + 2] -anchor w -text "Receiver:" -font $chart_font 
.chart.c create line $chart_xoffset [expr $chart_boxsize + 2] $chart_xoffset 2
.chart.c create line $chart_xoffset 2 2 2

proc chart_enlarge {new_size} {
  global chart_size
  global chart_boxsize
  global chart_xoffset
  global chart_yoffset
  global chart_font

  if {$new_size < $chart_size} {
    return
  }
  
  for {set i 0} {$i <= $new_size} {incr i} {
    set s1 $chart_xoffset
    set s2 [expr $chart_yoffset + ($chart_boxsize * $i)]
    set s3 [expr $chart_xoffset + ($chart_boxsize * $new_size)]
    set s4 [expr $chart_xoffset + ($chart_boxsize * $i)]
    set s5 $chart_yoffset
    set s6 [expr $chart_yoffset + ($chart_boxsize * $new_size)]
    .chart.c create line $s1 $s2 $s3 $s2
    .chart.c create line $s4 $s5 $s4 $s6 
  }

  for {set i 0} {$i < $new_size} {incr i} {
    set s1 $chart_xoffset
    set s2 [expr $chart_yoffset + ($chart_boxsize * $i)]
    set s3 [expr $chart_xoffset + ($chart_boxsize * $new_size)]
    set s4 [expr $chart_xoffset + ($chart_boxsize * $i)]
    set s5 $chart_yoffset
    set s6 [expr $chart_yoffset + ($chart_boxsize * $new_size)]
    .chart.c create text [expr $chart_xoffset - 2] [expr $s2 + ($chart_boxsize / 2)] -text $i -anchor e -font $chart_font
    .chart.c create text [expr $s4 + ($chart_boxsize / 2)] [expr $chart_yoffset - 2] -text $i -anchor s -font $chart_font
  }
  .chart.c configure -scrollregion "0 0 [expr $s3 + 1] [expr $s6 + 1]"
  set chart_size $new_size
}

proc chart_set {srce dest val} {
  global INDEX chart_size chart_boxsize chart_xoffset chart_yoffset

  if {[array names INDEX $srce] != $srce} {
    return
  }
  if {[array names INDEX $dest] != $dest} {
    return
  }
  set x $INDEX($srce)
  set y $INDEX($dest)

  if {($x > $chart_size) || ($x < 0)} return
  if {($y > $chart_size) || ($y < 0)} return
  if {($val > 101) || ($val < 0)}     return

  set xv [expr ($x * $chart_boxsize) + $chart_xoffset + 1]
  set yv [expr ($y * $chart_boxsize) + $chart_yoffset + 1]

  if {$val < 5} {
    set colour green
  } elseif {$val < 10} {
    set colour orange
  } elseif {$val <= 100} {
    set colour red
  } else {
    set colour white
  }

  .chart.c create rectangle $xv $yv [expr $xv + $chart_boxsize - 2] [expr $yv + $chart_boxsize - 2] -fill $colour -outline $colour
}

proc chart_label {ssrc} {
  global CNAME NAME INDEX chart_size chart_boxsize chart_xoffset chart_yoffset chart_font

  set pos $INDEX($ssrc)
  set val $NAME($ssrc)
  if {[string length $val] == 0} {
    set val $CNAME($ssrc)
  }

  if {($pos > $chart_size) || ($pos < 0)} return

  set ypos [expr $chart_yoffset + ($chart_boxsize * $pos) + ($chart_boxsize / 2)]
  .chart.c delete $ssrc
  .chart.c create text 2 $ypos -text [string range $val 0 25] -anchor w -font $chart_font -tag $ssrc
}

proc chart_redraw {size} {
  # This is not very efficient... [csp]
  global CNAME NAME INDEX chart_size chart_boxsize chart_xoffset chart_yoffset chart_font

  .chart.c delete all
  set chart_size 0
  chart_enlarge $size
  set j 0
  foreach i [array names CNAME] {
    set INDEX($i) $j
    chart_label $i
    incr j
  }
}

wm withdraw .chart
wm title    .chart "Reception quality matrix"
wm geometry .chart 320x200

chart_enlarge 1

#
# End of RTCP RR chart routines
#

bind . <t> {cb_send "R" $RAT_ADDR "toggle_input_port"}

