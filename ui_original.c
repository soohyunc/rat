char ui_original[] = "\
#\n\
# Copyright (c) 1995,1996,1997 University College London\n\
# All rights reserved.\n\
#\n\
# $Revision$\n\
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
# Default conference bus addresses. These are overridden later...\n\
set RAT_ADDR  \"NONE\"\n\
set  MY_ADDR  \"NONE\"\n\
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
\n\
set statsfont     -*-courier-medium-r-*-*-12-*-*-*-*-*-iso8859-1\n\
set titlefont     -*-helvetica-medium-r-normal--14-*-p-*-iso8859-1\n\
set infofont      -*-helvetica-medium-r-normal--12-*-p-*-iso8859-1\n\
set smallfont     -*-helvetica-medium-r-normal--10-*-p-*-iso8859-1\n\
set verysmallfont -*-courier-medium-o-normal--8-*-m-*-iso8859-1\n\
\n\
set V(class) \"Mbone Applications\"\n\
set V(app)   \"rat\"\n\
\n\
set iht			16\n\
set iwd 		190\n\
set fw 			.l.t.list.f\n\
set cancel_info_timer 	0\n\
set num_ssrc		0\n\
set DEBUG		0\n\
\n\
# Commands to send message over the conference bus...\n\
proc toggle_play {} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"toggle_play\"\n\
  if {[string compare [.r.c.vol.t1 cget -relief] raised] == 0} {\n\
    .r.c.vol.t1 configure -relief sunken\n\
  } else {\n\
    .r.c.vol.t1 configure -relief raised\n\
  }\n\
}\n\
\n\
proc toggle_send {} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"toggle_send\"\n\
}\n\
\n\
proc redundancy {coding} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"redundancy $coding\"\n\
}\n\
\n\
proc primary {coding} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"primary $coding\"\n\
}\n\
\n\
proc set_vol {volume} {\n\
  global RAT_ADDR\n\
  if {$RAT_ADDR != \"NONE\"} {\n\
    cb_send \"U\" $RAT_ADDR \"output gain $volume\"\n\
  }\n\
}\n\
\n\
proc set_gain {gain} {\n\
  global RAT_ADDR\n\
  if {$RAT_ADDR != \"NONE\"} {\n\
    cb_send \"U\" $RAT_ADDR \"input gain $gain\"\n\
  }\n\
}\n\
\n\
proc toggle_input_port {} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"toggle_input_port\"\n\
}\n\
\n\
proc toggle_output_port {} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"toggle_output_port\"\n\
}\n\
\n\
proc silence {s} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"silence $s\"\n\
}\n\
\n\
proc lecture {l} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"lecture $l\"\n\
}\n\
\n\
proc agc {a} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"agc $a\"\n\
}\n\
\n\
proc repair {r} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"repair $r\"\n\
}\n\
\n\
proc output_mode {o} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"output mode $o\"\n\
}\n\
\n\
proc powermeter {pm} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"powermeter $pm\"\n\
}\n\
\n\
proc rate {r} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"rate $r\"\n\
}\n\
\n\
proc InstallKey {key} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"update_key $key\"\n\
}\n\
\n\
proc play {file} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"play $file\"\n\
}\n\
\n\
proc rec {file} {\n\
  global RAT_ADDR\n\
  cb_send \"U\" $RAT_ADDR \"rec $file\"\n\
}\n\
\n\
# \n\
# The following function deal with receiving messages from the conference bus. The code\n\
# in ui.c will call cb_recv with the appropriate arguments when a message is received. \n\
#\n\
\n\
proc cb_recv {src cmd} {\n\
  global DEBUG\n\
  if [string match [info procs [lindex cb_recv_$cmd 0]] [lindex cb_recv_$cmd 0]] {\n\
    eval cb_recv_$cmd \n\
  } else {\n\
    if $DEBUG {\n\
      puts stdout \"ConfBus: ERROR unknown command $cmd\"\n\
    }\n\
  }\n\
}\n\
\n\
proc cb_recv_init {rat_addr my_addr} {\n\
	# RAT has initialised itself, and we're now ready to go. \n\
	# Perform any last minute initialisation...\n\
	global RAT_ADDR MY_ADDR\n\
	set RAT_ADDR $rat_addr\n\
	set  MY_ADDR  $my_addr\n\
}\n\
\n\
proc cb_recv_agc {args} {\n\
  global agc_var\n\
  set agc_var $args\n\
}\n\
\n\
proc cb_recv_primary {args} {\n\
  global prenc\n\
  set prenc $args\n\
}\n\
\n\
proc cb_recv_redundancy {args} {\n\
  global secenc\n\
  set secenc $args\n\
}\n\
\n\
proc cb_recv_repair {args} {\n\
  set repair_var $args\n\
}\n\
\n\
proc cb_recv_repair {val} {\n\
	global repair_val\n\
	set repair_val $val\n\
}\n\
\n\
proc cb_recv_powermeter {type level} {\n\
	# powermeter input  <value>\n\
	# powermeter output <value>\n\
	# powermeter <ssrc> <value>\n\
	switch $type {\n\
		input   {bargraphSetHeight .r.c.gain.b2 $level}\n\
		output  {bargraphSetHeight .r.c.vol.b1  $level}\n\
	 	default {}\n\
	}\n\
}\n\
\n\
proc cb_recv_input {cmd args} {\n\
	switch $cmd {\n\
		gain	{.r.c.gain.s2 set $args}\n\
		device	{.r.c.gain.l2 configure -bitmap $args}\n\
		mute    {.r.c.gain.t2 configure -relief sunken}\n\
		unmute  {.r.c.gain.t2 configure -relief raised}\n\
	}\n\
}\n\
\n\
proc cb_recv_output {cmd args} {\n\
	switch $cmd {\n\
		gain	{.r.c.vol.s1 set $args}\n\
		device	{.r.c.vol.l1 configure -bitmap $args}\n\
		mute    {.r.c.vol.t1 configure -relief sunken}\n\
		unmute  {.r.c.vol.t1 configure -relief raised}\n\
	}\n\
}\n\
\n\
proc cb_recv_half_duplex {} {\n\
	set output_var {Mike mutes net}\n\
	output_mode $output_var\n\
}\n\
\n\
proc cb_recv_debug {} {\n\
	global DEBUG\n\
	set DEBUG 1\n\
	.r.b.ucl configure -background salmon\n\
	.r.b.v   configure -background salmon\n\
}\n\
\n\
proc cb_recv_address {addr port ttl} {\n\
	.b.a.address configure -text \"Dest: $addr  Port: $port  TTL: $ttl\"\n\
}\n\
\n\
proc cb_recv_lecture_mode {mode} {\n\
	set lecture_var $mode\n\
}\n\
\n\
proc cb_recv_detect_silence {mode} {\n\
	set silence_var $mode\n\
}\n\
\n\
proc cb_recv_my_ssrc {ssrc} {\n\
	global my_ssrc rtcp_name rtcp_email rtcp_phone rtcp_loc\n\
	global RAT_ADDR\n\
	set my_ssrc $ssrc\n\
\n\
	# Now we know our SSRC, we can inform RAT of our SDES information...\n\
	cb_send \"U\" $RAT_ADDR \"ssrc $ssrc name  $rtcp_name\"\n\
	cb_send \"U\" $RAT_ADDR \"ssrc $ssrc email $rtcp_email\"\n\
	cb_send \"U\" $RAT_ADDR \"ssrc $ssrc phone $rtcp_phone\"\n\
	cb_send \"U\" $RAT_ADDR \"ssrc $ssrc loc   $rtcp_loc\"\n\
}\n\
\n\
proc cb_recv_ssrc {ssrc args} {\n\
	global CNAME NAME EMAIL LOC PHONE TOOL num_ssrc fw iht losstimers my_ssrc\n\
	global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME INDEX\n\
	set cmd [lindex $args 0]\n\
	set arg [lrange $args 1 end]\n\
	if {[array names INDEX $ssrc] != $ssrc} {\n\
		# This is an SSRC we've not seen before...\n\
		set        CNAME($ssrc) \"unknown\"\n\
		set         NAME($ssrc) \"unknown\"\n\
		set        EMAIL($ssrc) \"\"\n\
		set        PHONE($ssrc) \"\"\n\
		set          LOC($ssrc) \"\"\n\
		set         TOOL($ssrc) \"\"\n\
		set     ENCODING($ssrc) \"unknown\"\n\
		set     DURATION($ssrc) \"\"\n\
		set   PCKTS_RECV($ssrc) \"0\"\n\
		set   PCKTS_LOST($ssrc) \"0\"\n\
		set   PCKTS_MISO($ssrc) \"0\"\n\
		set  JITTER_DROP($ssrc) \"0\"\n\
		set       JITTER($ssrc) \"0\"\n\
		set   LOSS_TO_ME($ssrc) \"101\"\n\
		set LOSS_FROM_ME($ssrc) \"101\"\n\
		set        INDEX($ssrc) $num_ssrc\n\
		incr num_ssrc\n\
		chart_enlarge $num_ssrc \n\
	}\n\
	switch $cmd {\n\
		cname {\n\
			set CNAME($ssrc) $arg\n\
			if {[string compare NAME($ssrc) \"unknown\"] != 0} {\n\
				set NAME($ssrc) $arg\n\
			}\n\
			chart_label $ssrc\n\
		}\n\
		name            { \n\
			set NAME($ssrc) $arg\n\
			chart_label $ssrc\n\
		}\n\
		email           { set EMAIL($ssrc) $arg}\n\
		phone           { set PHONE($ssrc) $arg}\n\
		loc             { set LOC($ssrc) $arg}\n\
		tool            { set TOOL($ssrc) $arg}\n\
		encoding        { set ENCODING($ssrc) $arg}\n\
		packet_duration { set DURATION($ssrc) $arg}\n\
		packets_recv    { set PCKTS_RECV($ssrc) $arg}\n\
		packets_lost    { set PCKTS_LOST($ssrc) $arg}\n\
		packets_miso    { set PCKTS_MISO($ssrc) $arg}\n\
		jitter_drop     { set JITTER_DROP($ssrc) $arg}\n\
		jitter          { set JITTER($ssrc) $arg}\n\
		loss_to_me      { \n\
			set LOSS_TO_ME($ssrc) $arg \n\
			set srce $ssrc\n\
			set dest $my_ssrc\n\
			set loss [lindex $args 1]\n\
			catch {after cancel $losstimers($srce,$dest)}\n\
			chart_set $srce $dest $loss\n\
			set losstimers($srce,$dest) [after 30000 \"chart_set $srce $dest 101\"]\n\
		}\n\
		loss_from_me    { \n\
			set LOSS_FROM_ME($ssrc) $arg \n\
			set srce $my_ssrc\n\
			set dest $ssrc\n\
			set loss [lindex $args 1]\n\
			catch {after cancel $losstimers($srce,$dest)}\n\
			chart_set $srce $dest $loss\n\
			set losstimers($srce,$dest) [after 30000 \"chart_set $srce $dest 101\"]\n\
		}\n\
		loss_from	{ \n\
			set dest $ssrc\n\
			set srce [lindex $args 1]\n\
			set loss [lindex $args 2]\n\
			catch {after cancel $losstimers($srce,$dest)}\n\
			chart_set $srce $dest $loss\n\
			set losstimers($srce,$dest) [after 30000 \"chart_set $srce $dest 101\"]\n\
		}\n\
		active          {\n\
			if {[string compare $arg \"now\"] == 0} {\n\
				catch [$fw.c$ssrc configure -background white]\n\
			}\n\
			if {[string compare $arg \"recent\"] == 0} {\n\
				catch [$fw.c$ssrc configure -background gray90]\n\
			}\n\
		}\n\
		inactive {\n\
			catch [$fw.c$ssrc configure -background gray80]\n\
		}	\n\
		remove {\n\
			catch [destroy $fw.c$ssrc]\n\
			unset CNAME($ssrc) NAME($ssrc) EMAIL($ssrc) PHONE($ssrc) LOC($ssrc) TOOL($ssrc)\n\
			unset ENCODING($ssrc) DURATION($ssrc) PCKTS_RECV($ssrc) PCKTS_LOST($ssrc) PCKTS_MISO($ssrc)\n\
			unset JITTER_DROP($ssrc) JITTER($ssrc) LOSS_TO_ME($ssrc) LOSS_FROM_ME($ssrc) INDEX($ssrc)\n\
			incr num_ssrc -1\n\
			chart_redraw $num_ssrc\n\
			# Make sure we don't try to update things later...\n\
			return\n\
		}\n\
		mute {\n\
			$fw.c$ssrc create line [expr $iht + 2] [expr $iht / 2] 500 [expr $iht / 2] -tags a -width 2.0 -fill gray95\n\
		}\n\
		unmute {\n\
			catch [$fw.c$ssrc delete a]\n\
		}\n\
		default {\n\
			puts stdout \"WARNING: ConfBus message 'ssrc $ssrc $cmd $arg' not understood\"\n\
		}\n\
	}\n\
	ssrc_update $ssrc\n\
}\n\
\n\
\n\
\n\
proc ssrc_update {ssrc} {\n\
	global CNAME NAME EMAIL LOC PHONE TOOL \n\
	global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME\n\
	global fw iht iwd my_ssrc mylosstimers his_or_her_losstimers\n\
\n\
	set cw 		$fw.c$ssrc\n\
\n\
	if {[winfo exists $cw]} {\n\
		$cw itemconfigure t -text $NAME($ssrc)\n\
	} else {\n\
		set thick 0\n\
		set l $thick\n\
		set h [expr $iht / 2 + $thick]\n\
		set f [expr $iht + $thick]\n\
		canvas $cw -width $iwd -height $f -highlightthickness $thick\n\
		$cw create text [expr $f + 2] $h -anchor w -text $NAME($ssrc) -fill black -tag t\n\
		$cw create polygon $l $h $h $l $h $f -outline black -fill grey50 -tag m\n\
		$cw create polygon $f $h $h $l $h $f -outline black -fill grey -tag h\n\
\n\
		bind $cw <Button-1> \"toggle_stats $ssrc\"\n\
		bind $cw <Button-2> \"toggle_mute $cw $ssrc\"\n\
	}\n\
\n\
	# XXX This is not very efficient\n\
	if {[info exists my_ssrc] && [string compare $ssrc $my_ssrc]} {\n\
		foreach i [pack slaves $fw] {\n\
			set u [string toupper $NAME($ssrc)]\n\
			if {[string compare $u [string toupper [$i itemcget t -text]]] < 0 && [string compare $i $fw.c$my_ssrc] != 0} {\n\
				pack $cw -before $i\n\
				break\n\
			}\n\
		}\n\
	} else {\n\
		if {[pack slaves $fw] != \"\"} {\n\
			pack $cw -before [lindex [pack slaves $fw] 0]\n\
		}\n\
	}\n\
	pack $cw -fill x\n\
\n\
	fix_scrollbar\n\
	update_stats $ssrc\n\
\n\
	if {$LOSS_TO_ME($ssrc) < 5} {\n\
		catch [$fw.c$ssrc itemconfigure m -fill green]\n\
	} elseif {$LOSS_TO_ME($ssrc) < 10} {\n\
		catch [$fw.c$ssrc itemconfigure m -fill orange]\n\
	} elseif {$LOSS_TO_ME($ssrc) <= 100} {\n\
		catch [$fw.c$ssrc itemconfigure m -fill red]\n\
	} else {\n\
		catch [$fw.c$ssrc itemconfigure m -fill grey50]\n\
	}\n\
	catch {after cancel $mylosstimers($ssrc)}\n\
	if {$LOSS_TO_ME($ssrc) <= 100} {\n\
		set mylosstimers($ssrc) [after 10000 \"set LOSS_TO_ME($ssrc) 101; ssrc_update $ssrc\"]\n\
	}\n\
\n\
	if {$LOSS_FROM_ME($ssrc) < 5} {\n\
		catch [$fw.c$ssrc itemconfigure h -fill green]\n\
	} elseif {$LOSS_FROM_ME($ssrc) < 10} {\n\
		catch [$fw.c$ssrc itemconfigure h -fill orange]\n\
	} elseif {$LOSS_FROM_ME($ssrc) <= 100} {\n\
		catch [$fw.c$ssrc itemconfigure h -fill red]\n\
	} else {\n\
		catch [$fw.c$ssrc itemconfigure h -fill grey]\n\
	}\n\
	catch {after cancel $his_or_her_losstimers($ssrc)}\n\
	if {$LOSS_FROM_ME($ssrc)<=100} {\n\
		set his_or_her_losstimers($ssrc) [after 30000 \"set LOSS_FROM_ME($ssrc) 101; ssrc_update $ssrc\"]\n\
	}\n\
}\n\
\n\
#power meters\n\
proc bargraphCreate {bgraph} {\n\
	global oh$bgraph\n\
\n\
	frame $bgraph -bg black\n\
	frame $bgraph.inner0 -width 8 -height 4 -bg green\n\
	pack $bgraph.inner0 -side bottom -pady 1 -fill both -expand true\n\
	for {set i 1} {$i < 16} {incr i} {\n\
		frame $bgraph.inner$i -width 8 -height 4 -bg black\n\
		pack $bgraph.inner$i -side bottom -pady 1 -fill both -expand true\n\
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
#dropdown list\n\
proc dropdown {w varName command args} {\n\
\n\
    global $varName \n\
    set firstValue [lindex $args 0]\n\
\n\
    if ![info exists $varName] {\n\
	set $varName $firstValue\n\
    }\n\
\n\
    menubutton $w -textvariable var -indicatoron 0 -menu $w.menu -text $firstValue -textvariable $varName -relief raised \n\
    menu $w.menu -tearoff 0\n\
    foreach i $args {\n\
	$w.menu add radiobutton -variable $varName -label $i -value $i -command \"$command [lindex $i 0]\" \n\
    }\n\
    return $w.menu\n\
}\n\
\n\
proc toggle_mute {cw ssrc} {\n\
	global RAT_ADDR\n\
	global iht\n\
	if {[$cw gettags a] == \"\"} {\n\
		cb_send \"U\" $RAT_ADDR \"ssrc $ssrc mute\"\n\
	} else {\n\
		cb_send \"U\" $RAT_ADDR \"ssrc $ssrc unmute\"\n\
	}\n\
}\n\
\n\
proc fix_scrollbar {} {\n\
	global iht iwd fw\n\
\n\
	set ch [expr $iht * ([llength [pack slaves $fw]] + 2)]\n\
	set bh [winfo height .l.t.scr]\n\
	if {$ch > $bh} {set h $ch} else {set h $bh}\n\
	.l.t.list configure -scrollregion \"0.0 0.0 $iwd $h\"\n\
}\n\
\n\
proc info_timer {} {\n\
	global cancel_info_timer\n\
	if {$cancel_info_timer == 1} {\n\
		set cancel_info_timer 0\n\
	} else {\n\
		update_rec_info\n\
		after 1000 info_timer\n\
	}\n\
}\n\
\n\
proc update_stats {ssrc} {\n\
	global CNAME NAME EMAIL LOC PHONE TOOL\n\
	global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME\n\
\n\
	if {$LOSS_TO_ME($ssrc) == 101} {\n\
		set loss_to_me \"unknown\"\n\
	} else {\n\
		set loss_to_me \"$LOSS_TO_ME($ssrc)%\"\n\
	}\n\
\n\
	if {$LOSS_FROM_ME($ssrc) == 101} {\n\
		set loss_from_me \"unknown\"\n\
	} else {\n\
		set loss_from_me \"$LOSS_FROM_ME($ssrc)%\"\n\
	}\n\
\n\
	if {[winfo exists .stats$ssrc]} {\n\
		.stats$ssrc.m configure -text \" Name:                    $NAME($ssrc)\\n\\\n\
	                                	Email:                   $EMAIL($ssrc)\\n\\\n\
				        	Phone:                   $PHONE($ssrc)\\n\\\n\
				        	Location:                $LOC($ssrc)\\n\\\n\
				        	Tool:                    $TOOL($ssrc)\\n\\\n\
				        	CNAME:                   $CNAME($ssrc)\\n\\\n\
				        	Audio Encoding:          $ENCODING($ssrc)\\n\\\n\
				        	Audio Length:            $DURATION($ssrc)\\n\\\n\
				        	Packets Received:        $PCKTS_RECV($ssrc)\\n\\\n\
				        	Packets Lost:            $PCKTS_LOST($ssrc)\\n\\\n\
				        	Packets Misordered:      $PCKTS_MISO($ssrc)\\n\\\n\
				        	Units Dropped (jitter):  $JITTER_DROP($ssrc)\\n\\\n\
				        	Network Timing Jitter:   $JITTER($ssrc)\\n\\\n\
				        	Instantaneous Loss Rate: $loss_to_me\\n\\\n\
						Loss from me:            $loss_from_me\"\n\
	}\n\
}\n\
\n\
proc toggle_stats {ssrc} {\n\
	global statsfont\n\
	if {[winfo exists .stats$ssrc]} {\n\
		destroy .stats$ssrc\n\
	} else {\n\
		# Window does not exist so create it\n\
		toplevel .stats$ssrc\n\
		message .stats$ssrc.m -width 600 -font $statsfont\n\
		pack .stats$ssrc.m -side top\n\
		button .stats$ssrc.d -highlightthickness 0 -padx 0 -pady 0 -text \"Dismiss\" -command \"destroy .stats$ssrc\" \n\
		pack .stats$ssrc.d -side bottom -fill x\n\
		wm title .stats$ssrc \"RAT user info\"\n\
		wm resizable .stats$ssrc 0 0\n\
		update_stats $ssrc\n\
	}\n\
}\n\
\n\
# Initialise RAT MAIN window\n\
frame .r \n\
frame .l \n\
frame .l.t -relief raised\n\
scrollbar .l.t.scr -relief flat -highlightthickness 0 -command \".l.t.list yview\"\n\
canvas .l.t.list -highlightthickness 0 -bd 0 -relief raised -width $iwd -yscrollcommand \".l.t.scr set\" -yscrollincrement $iht\n\
frame .l.t.list.f -highlightthickness 0 -bd 0\n\
.l.t.list create window 0 0 -anchor nw -window .l.t.list.f\n\
frame  .l.s1 -bd 0\n\
button .l.s1.opts  -highlightthickness 0 -padx 0 -pady 0 -text \"Options\" -command {wm deiconify .b}\n\
button .l.s1.about -highlightthickness 0 -padx 0 -pady 0 -text \"About\"   -command {wm deiconify .about}\n\
button .l.s1.quit  -highlightthickness 0 -padx 0 -pady 0 -text \"Quit\"    -command {destroy .}\n\
frame  .l.s2 -bd 0\n\
button .l.s2.stats -highlightthickness 0 -padx 0 -pady 0 -text \"Reception Quality\" -command {wm deiconify .chart}\n\
button .l.s2.audio -highlightthickness 0 -padx 0 -pady 0 -text \"Get Audio\"         -command {cb_send \"U\" $RAT_ADDR get_audio}\n\
\n\
pack .r -side right -fill y\n\
frame .r.c\n\
pack .r.c -side top -fill y -expand 1\n\
frame .r.c.vol\n\
frame .r.c.gain\n\
pack .r.c.vol -side left -fill y\n\
pack .r.c.gain -side right -fill y\n\
\n\
pack .l -side left -fill both -expand 1\n\
pack .l.s1 -side bottom -fill x\n\
pack .l.s1.opts .l.s1.about .l.s1.quit -side left -fill x -expand 1\n\
pack .l.s2 -side bottom -fill x\n\
pack .l.s2.stats .l.s2.audio -side left -fill x -expand 1\n\
pack .l.t -side top -fill both -expand 1\n\
pack .l.t.scr -side left -fill y\n\
pack .l.t.list -side left -fill both -expand 1\n\
bind .l.t.list <Configure> {fix_scrollbar}\n\
\n\
# Device output controls\n\
button .r.c.vol.t1 -highlightthickness 0 -padx 0 -pady 0 -text mute -command toggle_play \n\
button .r.c.vol.l1 -highlightthickness 0 -padx 0 -pady 0 -command toggle_output_port -bitmap \"speaker\"\n\
bargraphCreate .r.c.vol.b1\n\
scale .r.c.vol.s1 -highlightthickness 0 -font $verysmallfont -from 99 -to 0 -command set_vol -orient vertical -relief raised \n\
\n\
pack .r.c.vol.t1 -side top -fill x\n\
pack .r.c.vol.l1 -side top -fill x\n\
pack .r.c.vol.b1 -side left -fill y\n\
pack .r.c.vol.s1 -side right -fill y\n\
\n\
# Device input controls\n\
button .r.c.gain.t2 -highlightthickness 0 -padx 0 -pady 0 -text mute -command toggle_send -relief sunken\n\
button .r.c.gain.l2 -highlightthickness 0 -padx 0 -pady 0 -command toggle_input_port -bitmap \"mic_mute\"\n\
bargraphCreate .r.c.gain.b2\n\
scale .r.c.gain.s2 -highlightthickness 0 -font $verysmallfont -from 99 -to 0 -command set_gain -orient vertical -relief raised \n\
\n\
pack .r.c.gain.t2 -side top -fill x\n\
pack .r.c.gain.l2 -side top -fill x\n\
pack .r.c.gain.b2 -side left -fill y\n\
pack .r.c.gain.s2 -side right -fill y\n\
\n\
proc cb_recv_disable_audio_ctls {} {\n\
	.r.c.vol.t1 configure -state disabled\n\
	.r.c.vol.l1 configure -state disabled\n\
	.r.c.vol.s1 configure -state disabled\n\
	.r.c.gain.t2 configure -state disabled\n\
	.r.c.gain.l2 configure -state disabled\n\
	.r.c.gain.s2 configure -state disabled\n\
	.l.s2.audio  configure -state normal\n\
}\n\
\n\
proc cb_recv_enable_audio_ctls {} {\n\
	.r.c.vol.t1 configure -state normal\n\
	.r.c.vol.l1 configure -state normal\n\
	.r.c.vol.s1 configure -state normal\n\
	.r.c.gain.t2 configure -state normal\n\
	.r.c.gain.l2 configure -state normal\n\
	.r.c.gain.s2 configure -state normal\n\
	.l.s2.audio  configure -state disabled\n\
}\n\
\n\
frame .r.b -relief raised \n\
pack .r.b -side bottom -fill x\n\
label .r.b.v -highlightthickness 0 -bd 0 -font $smallfont -text $ratversion\n\
pack .r.b.v -side bottom -fill x\n\
label .r.b.ucl -highlightthickness 0 -bd 0 -bitmap \"ucl\"\n\
pack .r.b.ucl -side bottom -fill x\n\
\n\
bind all <ButtonPress-3> \"toggle_send\"\n\
bind all <ButtonRelease-3> \"toggle_send\"\n\
bind all <q> {destroy .}\n\
\n\
wm iconbitmap . rat_small\n\
wm resizable . 1 1\n\
if ([info exists geometry]) {\n\
        wm geometry . $geometry\n\
}\n\
\n\
# Initialise CONTROL toplevel window\n\
toplevel .b\n\
wm withdraw .b\n\
\n\
set rate_var 	\"40 ms\"\n\
set output_var 	\"Full duplex\"\n\
set redun 	0\n\
set sync_var 	0\n\
set meter_var	1\n\
\n\
frame .b.f\n\
pack .b.f -side top -fill x\n\
\n\
# packet format options\n\
frame .b.f.pkt \n\
pack  .b.f.pkt -side top -fill both -expand 1\n\
label .b.f.pkt.l -highlightthickness 0 -text \"Packet Format\"\n\
pack  .b.f.pkt.l\n\
# length\n\
frame .b.f.pkt.len -relief sunken\n\
pack  .b.f.pkt.len -side left -fill x\n\
label .b.f.pkt.len.l -highlightthickness 0 -justify left -text  \"Duration\"\n\
pack  .b.f.pkt.len.l -side top -fill both -expand 1\n\
dropdown .b.f.pkt.len.dl rate_var rate \"20 ms\" \"40 ms\" \"80 ms\"  \"160 ms\"\n\
pack     .b.f.pkt.len.dl -side left -fill x -expand 1\n\
# primary\n\
frame .b.f.pkt.pr -relief sunken\n\
pack  .b.f.pkt.pr -side left -fill x\n\
label .b.f.pkt.pr.l -highlightthickness 0 -justify left -text  \"Primary Encoding\"\n\
pack  .b.f.pkt.pr.l -side top -fill both -expand 1\n\
dropdown .b.f.pkt.pr.dl prenc primary WBS \"16-bit linear\" \"PCM (mu-law)\" DVI GSM LPC\n\
pack  .b.f.pkt.pr.dl -side left -fill x -expand 1\n\
\n\
# secondary\n\
frame .b.f.pkt.sec -relief sunken\n\
pack  .b.f.pkt.sec -side left -fill x\n\
label .b.f.pkt.sec.l -highlightthickness 0 -justify left -text  \"Secondary Encoding\"\n\
pack  .b.f.pkt.sec.l -side top -fill both -expand 1\n\
dropdown .b.f.pkt.sec.dl secenc redundancy NONE \"PCM (mu-law)\" DVI GSM LPC\n\
pack  .b.f.pkt.sec.dl -side left -fill x -expand 1\n\
\n\
# Local Options\n\
frame .b.f.loc\n\
pack  .b.f.loc -side top -fill both -expand 1\n\
label .b.f.loc.l -highlightthickness 0 -text \"Local Options\"\n\
pack  .b.f.loc.l -side top -fill x -expand 1\n\
\n\
# Mode\n\
\n\
frame .b.f.loc.mode -relief sunken -width 500\n\
pack  .b.f.loc.mode -side left -fill x -expand 0\n\
label .b.f.loc.mode.l -highlightthickness 0 -justify left -text  \"Mode                \"\n\
pack  .b.f.loc.mode.l -side top -expand 1 -anchor w\n\
dropdown .b.f.loc.mode.dl output_var output \"Net mutes mike\" \"Mike mutes net\" \"Full duplex\"\n\
pack  .b.f.loc.mode.dl -side left -fill x -expand 1\n\
\n\
# Receiver Repair Options\n\
frame .b.f.loc.rep -relief sunken\n\
pack  .b.f.loc.rep -side left  -fill x -expand 1\n\
label .b.f.loc.rep.l -highlightthickness 0 -justify left -text \"Loss Repair\"\n\
pack  .b.f.loc.rep.l -side top -expand 1 -anchor w\n\
dropdown .b.f.loc.rep.dl repair_var repair None \"Packet Repetition\" \n\
pack  .b.f.loc.rep.dl -side left -fill x -expand 1\n\
\n\
# Misc controls\n\
frame .b.f2 -bd 0\n\
pack .b.f2 -side top -fill x\n\
\n\
# Generic toggles\n\
frame .b.f2.l -relief sunken \n\
pack  .b.f2.l -side left -expand 1 -fill both\n\
frame .b.f2.r -relief sunken \n\
pack  .b.f2.r -side left -expand 1 -fill both\n\
checkbutton .b.f2.l.sil   -anchor w -highlightthickness 0 -relief flat -text \"Suppress Silence\"       -variable silence_var -command {silence    $silence_var}\n\
checkbutton .b.f2.l.meter -anchor w -highlightthickness 0 -relief flat -text \"Powermeters\"            -variable meter_var   -command {powermeter $meter_var}\n\
checkbutton .b.f2.l.lec   -anchor w -highlightthickness 0 -relief flat -text \"Lecture Mode\"           -variable lecture_var -command {lecture    $lecture_var}\n\
checkbutton .b.f2.r.agc   -anchor w -highlightthickness 0 -relief flat -text \"Automatic Gain Control\" -variable agc_var     -command {agc        $agc_var}\n\
checkbutton .b.f2.r.syn   -anchor w -highlightthickness 0 -relief flat -text \"Video Synchronisation\"  -variable sync_var    -command {sync       $sync_var} -state disabled\n\
checkbutton .b.f2.r.afb   -anchor w -highlightthickness 0 -relief flat -text \"Acoustic Feedback\"      -variable afb_var     -command {afb        $afb_var} -state disabled\n\
\n\
pack .b.f2.l.sil   -side top -fill x -expand 1\n\
pack .b.f2.l.meter -side top -fill x -expand 1\n\
pack .b.f2.l.lec   -side top -fill x -expand 0\n\
pack .b.f2.r.syn   -side top -fill x -expand 0\n\
pack .b.f2.r.agc   -side top -fill x -expand 1\n\
pack .b.f2.r.afb   -side top -fill x -expand 0\n\
\n\
#Session Key\n\
frame .b.crypt -bd 0\n\
pack  .b.crypt -side top -fill x -expand 1\n\
label .b.crypt.l -text Encryption \n\
pack  .b.crypt.l\n\
\n\
label .b.crypt.kn -highlightthickness 0 -text \"Key:\"\n\
pack .b.crypt.kn -side left -fill x\n\
entry .b.crypt.name -highlightthickness 0 -width 20 -relief sunken -textvariable key\n\
bind .b.crypt.name <Return> {UpdateKey $key .b.crypt }\n\
bind .b.crypt.name <BackSpace> { CheckKeyErase $key .b.crypt }\n\
bind .b.crypt.name <Any-Key> { KeyEntryCheck $key .b.crypt \"%A\" }\n\
bind .b.crypt.name <Control-Key-h> { CheckKeyErase $key .b.crypt }\n\
pack .b.crypt.name -side left -fill x -expand 1\n\
\n\
checkbutton .b.crypt.enc -state disabled -highlightthickness 0 -relief flat -text \"On/Off\" -variable key_var -command {ToggleKey $key_var}\n\
pack .b.crypt.enc -side left -fill x \n\
\n\
\n\
proc DisableSessionKey w {\n\
    global key_var\n\
 \n\
    set key_var 0\n\
    $w.enc configure -state disabled\n\
}\n\
\n\
proc EnableSessionKey w {\n\
    global key_var\n\
\n\
    $w.enc configure -state normal\n\
}\n\
\n\
proc KeyEntryCheck { key w key_val } {\n\
\n\
  if { $key_val != \"{}\" } {\n\
    if { [string length $key] == 0 } {\n\
	EnableSessionKey $w\n\
    }\n\
  }\n\
} \n\
\n\
proc CheckKeyErase { key w } {\n\
\n\
  if { [string length $key] == 1 } {\n\
    UpdateKey \"\" $w \n\
  }\n\
}\n\
\n\
proc UpdateKey { key w } {\n\
  global key_var\n\
 \n\
  if { $key == \"\" } {\n\
    DisableSessionKey $w\n\
  } else {\n\
    EnableSessionKey $w\n\
    set key_var 1\n\
  }\n\
  InstallKey $key\n\
}\n\
  \n\
proc ToggleKey { encrypt } {\n\
  global key \n\
\n\
  if !($encrypt) {\n\
    InstallKey \"\"\n\
  } else {\n\
    InstallKey $key\n\
  }\n\
}\n\
\n\
set cryptpos [lsearch $argv \"-crypt\"]\n\
if {$cryptpos == -1} then {\n\
  set key_var 0\n\
} else {\n\
  set key   [lindex $argv [expr $cryptpos + 1]]\n\
  UpdateKey [lindex $argv [expr $cryptpos + 1]] .b.crypt\n\
}\n\
\n\
# File options\n\
frame .b.f3\n\
pack .b.f3 -side top -fill x\n\
\n\
# Play\n\
set play_file infile.rat\n\
frame .b.f3.p -relief sunken \n\
pack .b.f3.p -side left -fill both -expand 1\n\
label .b.f3.p.l -highlightthickness 0 -text \"Play file\"\n\
pack .b.f3.p.l -side top -fill x\n\
frame .b.f3.p.f -bd 0\n\
pack .b.f3.p.f -side top -fill x\n\
label .b.f3.p.f.l -highlightthickness 0 -text \"File:\"\n\
pack .b.f3.p.f.l -side left -fill x\n\
entry .b.f3.p.f.file -highlightthickness 0 -width 10 -relief sunken -textvariable play_file\n\
pack .b.f3.p.f.file -side right -fill x -expand 1\n\
frame .b.f3.p.f2 -bd 0\n\
pack .b.f3.p.f2 -side top -fill x\n\
button .b.f3.p.f2.on -padx 0 -pady 0 -highlightthickness 0 -text \"Start\" -command {play $play_file}\n\
pack .b.f3.p.f2.on -side left -fill x -expand 1\n\
button .b.f3.p.f2.off -padx 0 -pady 0 -highlightthickness 0 -text \"Stop\" -command {play stop}\n\
pack .b.f3.p.f2.off -side right -fill x -expand 1\n\
\n\
# Record\n\
set rec_file outfile.rat\n\
frame .b.f3.r -relief sunken\n\
pack .b.f3.r -side right -fill both -expand 1\n\
label .b.f3.r.l -highlightthickness 0 -text \"Record file\"\n\
pack .b.f3.r.l -side top -fill x\n\
frame .b.f3.r.f -bd 0\n\
pack .b.f3.r.f -side top -fill x\n\
label .b.f3.r.f.l -highlightthickness 0 -text \"File:\"\n\
pack .b.f3.r.f.l -side left -fill x\n\
entry .b.f3.r.f.file -highlightthickness 0 -width 10 -relief sunken -textvariable rec_file\n\
pack .b.f3.r.f.file -side right -fill x -expand 1\n\
frame .b.f3.r.f2 -bd 0\n\
pack .b.f3.r.f2 -side top -fill x\n\
button .b.f3.r.f2.on -padx 0 -pady 0 -highlightthickness 0 -text \"Start\" -command {rec $rec_file}\n\
pack .b.f3.r.f2.on -side left -fill x -expand 1\n\
button .b.f3.r.f2.off -padx 0 -pady 0 -highlightthickness 0 -text \"Stop\" -command {rec stop}\n\
pack .b.f3.r.f2.off -side right -fill x -expand 1\n\
\n\
# Address...\n\
frame .b.a -relief sunken\n\
pack  .b.a -side top -fill x\n\
label .b.a.l -highlightthickness 0 -text \"RTP Configuration\"\n\
pack  .b.a.l -side top\n\
label .b.a.address -highlightthickness 0 -text \"Group: Port: TTL:\"\n\
pack  .b.a.address -side top -fill x\n\
frame .b.a.rn -bd 0\n\
pack  .b.a.rn -side top -fill x\n\
entry .b.a.rn.name -highlightthickness 0 -width 35 -relief sunken -textvariable rtcp_name\n\
bind  .b.a.rn.name <Return> {cb_send \"U\" $RAT_ADDR \"ssrc $my_ssrc name $rtcp_name\"; savename}\n\
bind  .b.a.rn.name <Tab>    {cb_send \"U\" $RAT_ADDR \"ssrc $my_ssrc name $rtcp_name\"; savename}\n\
pack  .b.a.rn.name -side right -fill x \n\
label .b.a.rn.l -highlightthickness 0 -text \"Name:\"\n\
pack  .b.a.rn.l -side left -fill x -expand 1\n\
frame .b.a.re -bd 0\n\
pack  .b.a.re -side top -fill x\n\
entry .b.a.re.name -highlightthickness 0 -width 35 -relief sunken -textvariable rtcp_email\n\
bind  .b.a.re.name <Return> {cb_send \"U\" $RAT_ADDR \"ssrc $my_ssrc email $rtcp_email\"; savename}\n\
bind  .b.a.re.name <Tab>    {cb_send \"U\" $RAT_ADDR \"ssrc $my_ssrc email $rtcp_email\"; savename}\n\
pack  .b.a.re.name -side right -fill x\n\
label .b.a.re.l -highlightthickness 0 -text \"Email:\"\n\
pack  .b.a.re.l -side left -fill x -expand 1\n\
frame .b.a.rp -bd 0\n\
pack  .b.a.rp -side top -fill x\n\
entry .b.a.rp.name -highlightthickness 0 -width 35 -relief sunken -textvariable rtcp_phone\n\
bind  .b.a.rp.name <Return> {cb_send \"U\" $RAT_ADDR \"ssrc $my_ssrc phone $rtcp_phone\"; savename}\n\
bind  .b.a.rp.name <Tab>    {cb_send \"U\" $RAT_ADDR \"ssrc $my_ssrc phone $rtcp_phone\"; savename}\n\
pack  .b.a.rp.name -side right -fill x\n\
label .b.a.rp.l -highlightthickness 0 -text \"Phone:\"\n\
pack  .b.a.rp.l -side left -fill x -expand 1\n\
frame .b.a.rl -bd 0\n\
pack  .b.a.rl -side top -fill x\n\
entry .b.a.rl.name -highlightthickness 0 -width 35 -relief sunken -textvariable rtcp_loc\n\
bind  .b.a.rl.name <Return> {cb_send \"U\" $RAT_ADDR \"ssrc $my_ssrc loc $rtcp_loc\"; savename}\n\
bind  .b.a.rl.name <Tab>    {cb_send \"U\" $RAT_ADDR \"ssrc $my_ssrc loc $rtcp_loc\"; savename}\n\
pack  .b.a.rl.name -side right -fill x\n\
label .b.a.rl.l -highlightthickness 0 -text \"Location:\"\n\
pack  .b.a.rl.l -side left -fill x -expand 1\n\
\n\
label .b.rat -bitmap rat2 -relief sunken\n\
pack .b.rat -fill x -expand 1\n\
\n\
button .b.d -highlightthickness 0 -padx 0 -pady 0 -text \"Dismiss\" -command \"wm withdraw .b\"\n\
pack .b.d -side bottom -fill x\n\
\n\
wm title .b \"RAT controls\"\n\
wm resizable .b 0 0\n\
\n\
# Initialise ABOUT toplevel window\n\
toplevel .about\n\
wm withdraw .about\n\
frame .about.a\n\
label .about.a.b -highlightthickness 0 -bitmap rat_med\n\
message .about.a.m -text {\n\
RAT Development Team:\n\
    Angela Sasse\n\
    Vicky Hardman\n\
    Isidor Kouvelas\n\
    Colin Perkins\n\
    Orion Hodson\n\
    Darren Harris\n\
    Anna Watson \n\
    Mark Handley\n\
    Jon Crowcroft\n\
    Anna Bouch\n\
}\n\
message .about.b -width 1000 -text {\n\
The Robust-Audio Tool has been written in the Department of Computer\n\
Science, University College London, UK. The work was supported by projects: \n\
\n\
   - Multimedia Integrated Conferencing for Europe (MICE)\n\
   - Multimedia European Research in Conferencing Integration (MERCI)\n\
   - Remote Language Teaching for SuperJANET (ReLaTe)\n\
   - Robust Audio Tool (RAT)\n\
\n\
Further information is available on the world-wide-web, please access URL:\n\
http://www-mice.cs.ucl.ac.uk/mice/rat/\n\
\n\
Please send comments/suggestions/bug-reports to <rat-trap@cs.ucl.ac.uk>\n\
}\n\
button .about.c -highlightthickness 0 -padx 0 -pady 0 -text Copyright -command {wm deiconify .copyright}\n\
button .about.d -highlightthickness 0 -padx 0 -pady 0 -text Dismiss   -command {wm withdraw  .about}\n\
pack .about.a .about.b .about.c .about.d -side top -fill x\n\
pack .about.a.b -side left -fill y\n\
pack .about.a.m -side right -fill both -expand 1\n\
wm title .about \"About RAT\"\n\
wm resizable .about 0 0\n\
 \n\
toplevel .copyright\n\
wm withdraw .copyright\n\
message   .copyright.m -text {\n\
 Robust-Audio Tool (RAT)\n\
 \n\
 Copyright (C) 1995,1996,1997 University College London\n\
 All rights reserved.\n\
 \n\
 Redistribution and use in source and binary forms, with or without\n\
 modification, is permitted, for non-commercial use only, provided \n\
 that the following conditions are met:\n\
 1. Redistributions of source code must retain the above copyright\n\
    notice, this list of conditions and the following disclaimer.\n\
 2. Redistributions in binary form must reproduce the above copyright\n\
    notice, this list of conditions and the following disclaimer in the\n\
    documentation and/or other materials provided with the distribution.\n\
 3. All advertising materials mentioning features or use of this software\n\
    must display the following acknowledgement:\n\
      This product includes software developed by the Computer Science\n\
      Department at University College London\n\
 4. Neither the name of the University nor of the Department may be used\n\
    to endorse or promote products derived from this software without\n\
    specific prior written permission.\n\
 Use of this software for commercial purposes is explicitly forbidden\n\
 unless prior written permission is obtained from the authors. \n\
 \n\
 THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND\n\
 ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n\
 ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE\n\
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n\
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n\
 OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n\
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n\
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n\
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n\
 SUCH DAMAGE.\n\
 \n\
 Modifications for HP-UX by Terje Vernly <terjeve@usit.uio.no> and \n\
 Geir Harald Hansen <g.h.hansen@usit.uio.no>.\n\
\n\
 This software is derived, in part, from publically available source code\n\
 with the following copyright:\n\
 \n\
 Copyright (c) 1991 University of Southern California\n\
 Copyright (c) 1993,1994 AT&T Bell Laboratories\n\
 Copyright (c) 1991-1993,1996 Regents of the University of California\n\
 Copyright (c) 1992 Stichting Mathematisch Centrum, Amsterdam\n\
 Copyright (c) 1991,1992 RSA Data Security, Inc\n\
 Copyright (c) 1992 Jutta Degener and Carsten Bormann, Technische Universitaet Berlin\n\
 Copyright (c) 1994 Henning Schulzrinne\n\
 Copyright (c) 1994 Paul Stewart\n\
 \n\
 This product includes software developed by the Computer Systems\n\
 Engineering Group and by the Network Research Group at Lawrence \n\
 Berkeley Laboratory.\n\
 \n\
 Encryption features of this software use the RSA Data Security, Inc. \n\
 MD5 Message-Digest Algorithm.\n\
} \n\
button .copyright.d -highlightthickness 0 -padx 0 -pady 0 -text Dismiss   -command {wm withdraw  .copyright}\n\
pack   .copyright.m .copyright.d -side top -fill x -expand 1\n\
wm title     .copyright \"RAT Copyright\"\n\
wm resizable .copyright 0 0\n\
\n\
if {[glob ~] == \"/\"} {\n\
	set rtpfname /.RTPdefaults\n\
} else {\n\
	set rtpfname ~/.RTPdefaults\n\
}\n\
\n\
\n\
proc savename {} {\n\
    global rtpfname rtcp_name rtcp_email rtcp_phone rtcp_loc V win32\n\
    if {$win32} {\n\
	putregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpName\"  \"$rtcp_name\"\n\
	putregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpEmail\"  \"$rtcp_email\"\n\
	putregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpPhone\"  \"$rtcp_phone\"\n\
	putregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpLoc\"  \"$rtcp_loc\"\n\
    } else {\n\
	set f [open $rtpfname w]\n\
	if {$rtcp_name  != \"\"} {puts $f \"*rtpName:  $rtcp_name\"}\n\
	if {$rtcp_email != \"\"} {puts $f \"*rtpEmail: $rtcp_email\"}\n\
	if {$rtcp_phone != \"\"} {puts $f \"*rtpPhone: $rtcp_phone\"}\n\
	if {$rtcp_loc   != \"\"} {puts $f \"*rtpLoc:   $rtcp_loc\"}\n\
	close $f\n\
    }\n\
}\n\
\n\
set rtcp_name  [option get . rtpName  rat]\n\
set rtcp_email [option get . rtpEmail rat]\n\
set rtcp_phone [option get . rtpPhone rat]\n\
set rtcp_loc   [option get . rtpLoc   rat]\n\
\n\
if {$win32 == 0} {\n\
    if {$rtcp_name == \"\" && [file readable $rtpfname] == 1} {\n\
	set f [open $rtpfname]\n\
	while {[eof $f] == 0} {\n\
	    gets $f line\n\
	    if {[string compare \"*rtpName:\"  [lindex $line 0]] == 0} {set rtcp_name  [lrange $line 1 end]}\n\
	    if {[string compare \"*rtpEmail:\" [lindex $line 0]] == 0} {set rtcp_email [lrange $line 1 end]}\n\
	    if {[string compare \"*rtpPhone:\" [lindex $line 0]] == 0} {set rtcp_phone [lrange $line 1 end]}\n\
	    if {[string compare \"*rtpLoc:\"   [lindex $line 0]] == 0} {set rtcp_loc   [lrange $line 1 end]}\n\
	}\n\
	close $f\n\
    }\n\
} else {\n\
    if {$rtcp_name == \"\"} {\n\
	catch {set rtcp_name  [getregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpName\"]  } \n\
	catch {set rtcp_email [getregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpEmail\"] } \n\
	catch {set rtcp_phone [getregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpPhone\"] } \n\
	catch {set rtcp_loc   [getregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpLoc\"  ] } \n\
    }\n\
}\n\
\n\
if {$rtcp_name == \"\"} {\n\
    toplevel .name\n\
    wm title .name \"RAT User Information\"\n\
    message .name.m -width 1000 -text {\n\
	Please enter the following details, for transmission\n\
	to other conference participants.\n\
    }\n\
	frame  .name.b\n\
    label  .name.b.res -text \"Name:\"\n\
    entry  .name.b.e -highlightthickness 0 -width 20 -relief sunken -textvariable rtcp_name\n\
    button .name.d -highlightthickness 0 -padx 0 -pady 0 -text Done -command {rtcp_name $rtcp_name; savename; destroy .name}\n\
    bind   .name.b.e <Return> {rtcp_name $rtcp_name; savename; destroy .name}\n\
    \n\
    pack .name.m -side top -fill x -expand 1\n\
    pack .name.b -side top -fill x -expand 1\n\
    pack .name.b.res -side left\n\
    pack .name.b.e -side right -fill x -expand 1\n\
    pack .name.d -side bottom -fill x -expand 1\n\
    wm resizable .name 0 0\n\
    update\n\
    raise .name .\n\
}\n\
\n\
#\n\
# Routines to display the \"chart\" of RTCP RR statistics...\n\
#\n\
\n\
set chart_font    6x10\n\
set chart_size    1\n\
set chart_boxsize 15\n\
set chart_xoffset 180\n\
set chart_yoffset 17\n\
\n\
toplevel  .chart\n\
canvas    .chart.c  -background white  -xscrollcommand {.chart.sb set} -yscrollcommand {.chart.sr set} \n\
scrollbar .chart.sr -orient vertical   -command {.chart.c yview}\n\
scrollbar .chart.sb -orient horizontal -command {.chart.c xview}\n\
button    .chart.d  -text \"Dismiss\"    -command {wm withdraw .chart} -padx 0 -pady 0\n\
\n\
pack .chart.d  -side bottom -fill x    -expand 0 -anchor s\n\
pack .chart.sb -side bottom -fill x    -expand 0 -anchor s\n\
pack .chart.sr -side right  -fill y    -expand 0 -anchor e\n\
pack .chart.c  -side left   -fill both -expand 1 -anchor n\n\
\n\
# Add a few labels to the chart...\n\
.chart.c create text 2 [expr ($chart_boxsize / 2) + 2] -anchor w -text \"Receiver:\" -font $chart_font \n\
.chart.c create line $chart_xoffset [expr $chart_boxsize + 2] $chart_xoffset 2\n\
.chart.c create line $chart_xoffset 2 2 2\n\
\n\
proc chart_enlarge {new_size} {\n\
  global chart_size\n\
  global chart_boxsize\n\
  global chart_xoffset\n\
  global chart_yoffset\n\
  global chart_font\n\
\n\
  if {$new_size < $chart_size} {\n\
    return\n\
  }\n\
  \n\
  for {set i 0} {$i <= $new_size} {incr i} {\n\
    set s1 $chart_xoffset\n\
    set s2 [expr $chart_yoffset + ($chart_boxsize * $i)]\n\
    set s3 [expr $chart_xoffset + ($chart_boxsize * $new_size)]\n\
    set s4 [expr $chart_xoffset + ($chart_boxsize * $i)]\n\
    set s5 $chart_yoffset\n\
    set s6 [expr $chart_yoffset + ($chart_boxsize * $new_size)]\n\
    .chart.c create line $s1 $s2 $s3 $s2\n\
    .chart.c create line $s4 $s5 $s4 $s6 \n\
  }\n\
\n\
  for {set i 0} {$i < $new_size} {incr i} {\n\
    set s1 $chart_xoffset\n\
    set s2 [expr $chart_yoffset + ($chart_boxsize * $i)]\n\
    set s3 [expr $chart_xoffset + ($chart_boxsize * $new_size)]\n\
    set s4 [expr $chart_xoffset + ($chart_boxsize * $i)]\n\
    set s5 $chart_yoffset\n\
    set s6 [expr $chart_yoffset + ($chart_boxsize * $new_size)]\n\
    .chart.c create text [expr $chart_xoffset - 2] [expr $s2 + ($chart_boxsize / 2)] -text $i -anchor e -font $chart_font\n\
    .chart.c create text [expr $s4 + ($chart_boxsize / 2)] [expr $chart_yoffset - 2] -text $i -anchor s -font $chart_font\n\
  }\n\
  .chart.c configure -scrollregion \"0 0 [expr $s3 + 1] [expr $s6 + 1]\"\n\
  set chart_size $new_size\n\
}\n\
\n\
proc chart_set {srce dest val} {\n\
  global INDEX chart_size chart_boxsize chart_xoffset chart_yoffset\n\
\n\
  if {[array names INDEX $srce] != $srce} {\n\
    return\n\
  }\n\
  if {[array names INDEX $dest] != $dest} {\n\
    return\n\
  }\n\
  set x $INDEX($srce)\n\
  set y $INDEX($dest)\n\
\n\
  if {($x > $chart_size) || ($x < 0)} return\n\
  if {($y > $chart_size) || ($y < 0)} return\n\
  if {($val > 101) || ($val < 0)}     return\n\
\n\
  set xv [expr ($x * $chart_boxsize) + $chart_xoffset + 1]\n\
  set yv [expr ($y * $chart_boxsize) + $chart_yoffset + 1]\n\
\n\
  if {$val < 5} {\n\
    set colour green\n\
  } elseif {$val < 10} {\n\
    set colour orange\n\
  } elseif {$val <= 100} {\n\
    set colour red\n\
  } else {\n\
    set colour white\n\
  }\n\
\n\
  .chart.c create rectangle $xv $yv [expr $xv + $chart_boxsize - 2] [expr $yv + $chart_boxsize - 2] -fill $colour -outline $colour\n\
}\n\
\n\
proc chart_label {ssrc} {\n\
  global CNAME NAME INDEX chart_size chart_boxsize chart_xoffset chart_yoffset chart_font\n\
\n\
  set pos $INDEX($ssrc)\n\
  set val $NAME($ssrc)\n\
  if {[string length $val] == 0} {\n\
    set val $CNAME($ssrc)\n\
  }\n\
\n\
  if {($pos > $chart_size) || ($pos < 0)} return\n\
\n\
  set ypos [expr $chart_yoffset + ($chart_boxsize * $pos) + ($chart_boxsize / 2)]\n\
  .chart.c delete $ssrc\n\
  .chart.c create text 2 $ypos -text [string range $val 0 25] -anchor w -font $chart_font -tag $ssrc\n\
}\n\
\n\
proc chart_redraw {size} {\n\
  # This is not very efficient... [csp]\n\
  global CNAME NAME INDEX chart_size chart_boxsize chart_xoffset chart_yoffset chart_font\n\
\n\
  .chart.c delete all\n\
  set chart_size 0\n\
  chart_enlarge $size\n\
  set j 0\n\
  foreach i [array names CNAME] {\n\
    set INDEX($i) $j\n\
    chart_label $i\n\
    incr j\n\
  }\n\
}\n\
\n\
wm withdraw .chart\n\
wm title    .chart \"Reception quality matrix\"\n\
wm geometry .chart 320x200\n\
\n\
chart_enlarge 1\n\
\n\
#\n\
# End of RTCP RR chart routines\n\
#\n\
\n\
bind . <t> {cb_send \"R\" $RAT_ADDR \"toggle_input_port\"}\n\
\n\
";
