char ui_anna[] = "\
#\n\
# Copyright (c) 1995,1996,1997 University College London\n\
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
\n\
# Standard RAT colours, etc...\n\
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
set RAT_ADDR \"NONE\"\n\
set  UI_ADDR \"NONE\"\n\
set attendance 0\n\
set stop 0\n\
set person \"\"\n\
set toggled 0\n\
set V(class) \"MBone Applications\"\n\
set V(app)   \"rat\"\n\
# The following function deal with receiving messages from the conference bus. The code\n\
# in ui.c will call cb_recv with the appropriate arguments when a message is received. \n\
# Messages can be sent to RAT by calling 'cb_send \"U\" $RAT_ADDR \"message\"'.\n\
###############################################################################\n\
proc cb_recv_output {cmd args} {\n\
switch $cmd {\n\
    gain {.rat.power.in.cin set $args\n\
}\n\
    device {.rat.power.in.inicon configure -bitmap $args\n\
}\n\
    mute {.rat.power.in.rec configure -relief sunken -text UnMute -bg seashell4 -fg white\n\
}\n\
unmute {.rat.power.in.rec configure -relief raised -text Mute -bg grey80 -fg black\n\
}\n\
}\n\
}\n\
##############################################################################\n\
proc cb_recv_input {cmd args} {\n\
global sending stop \n\
global RAT_ADDR\n\
switch $cmd {\n\
    global RAT_ADDR\n\
    gain {.rat.power.out.cout set $args}\n\
    device {.rat.power.out.outicon configure -bitmap $args}\n\
    mute {.rat.power.out.send configure -relief sunken -text UnMute -bg seashell4 -fg white\n\
#    if {$sending !=1} {\n\
#	.rat.power.transmit configure -text \"Push to Talk\"\n\
#	cb_send \"U\" $RAT_ADDR \"silence 0\"\n\
#    }\n\
}\n\
    unmute {.rat.power.out.outicon configure -relief raised -text Mute -bg grey80 -fg black\n\
#        if {$sending !=1} {\n\
#            .rat.power.transmit configure -text \"Push to Stop\"\n\
#            set stop 1\n\
#            cb_send \"U\" $RAT_ADDR \"silence 1\"\n\
#        }\n\
    }\n\
}\n\
}\n\
##############################################################################\n\
proc cb_recv_address {add port ttl} {\n\
global mode_int\n\
global title\n\
global Dest Port TTL\n\
set Dest $add\n\
set Port $port\n\
set TTL $ttl\n\
set title \"$add $port $ttl\"\n\
}\n\
##############################################################################\n\
proc set_gain gain {\n\
global RAT_ADDR\n\
cb_send \"U\" $RAT_ADDR \"input gain $gain\"\n\
}\n\
proc set_vol volume {\n\
global RAT_ADDR\n\
cb_send \"U\" $RAT_ADDR \"output gain $volume\"\n\
}\n\
###############################################################################\n\
proc update_names {} {\n\
	global myssrc\n\
global NAME EMAIL PHONE LOC \n\
	global RAT_ADDR\n\
	cb_send \"U\" $RAT_ADDR \"ssrc $myssrc name $NAME($myssrc)\"\n\
	cb_send \"U\" $RAT_ADDR \"ssrc $myssrc email $EMAIL($myssrc)\"\n\
	cb_send \"U\" $RAT_ADDR \"ssrc $myssrc phone $PHONE($myssrc)\"\n\
	cb_send \"U\" $RAT_ADDR \"ssrc $myssrc loc $LOC($myssrc)\" \n\
}\n\
##############################################################################\n\
proc cb_recv {src cmd} {\n\
  if [string match [info procs [lindex cb_recv_$cmd 0]] [lindex cb_recv_$cmd 0]] {\n\
    eval cb_recv_$cmd \n\
  }\n\
}\n\
#############################################################################\n\
proc cb_recv_powermeter {type level} {\n\
	switch $type {\n\
		output  {bargraphSetHeight .rat.power.in.cin    $level}\n\
		input   {bargraphSetHeight .rat.power.out.cout  $level}\n\
	}\n\
}\n\
##########################################################################\n\
proc rate size {\n\
global RAT_ADDR\n\
cb_send \"U\" $RAT_ADDR \"rate $size\"\n\
}\n\
##########################################################################\n\
proc cb_recv_my_ssrc ssrc {\n\
	global myssrc\n\
	set myssrc $ssrc\n\
}\n\
###########################################################################\n\
#Rat finished initialising, change defaults\n\
proc cb_recv_init {rat_addr ui_addr} {\n\
	global RAT_ADDR UI_ADDR\n\
	set RAT_ADDR $rat_addr\n\
	set  UI_ADDR  $ui_addr\n\
}\n\
\n\
##############################################################################\n\
proc cb_recv_ssrc {ssrc args} {\n\
    global SSRC\n\
    global CNAME NAME EMAIL LOC PHONE TOOL attendance \n\
    global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME\n\
    set cmd [lindex $args 0]\n\
    set arg [lrange $args 1 end]\n\
    switch $cmd {\n\
	cname {\n\
	    # All RTCP packets have a CNAME field, so when a new user arrives, this is the first\n\
	    # field to be filled in. We must therefore, check if this SSRC exists already, and\n\
	    # if not, fill in dummy values for all the SDES fields.\n\
	    set       CNAME($ssrc) $arg\n\
	    set        NAME($ssrc) $arg\n\
	    set       EMAIL($ssrc) \" \"\n\
	    set       PHONE($ssrc) \" \"\n\
	    set         LOC($ssrc) \" \"\n\
	    set        TOOL($ssrc) \" \"\n\
	    set    ENCODING($ssrc) \"unknown\"\n\
	    set    DURATION($ssrc) \" \"\n\
	    set  PCKTS_RECV($ssrc) \"0\"\n\
	    set  PCKTS_LOST($ssrc) \"0\"\n\
	    set  PCKTS_MISO($ssrc) \"0\"\n\
	    set JITTER_DROP($ssrc) \"0\"\n\
	    set      JITTER($ssrc) \"0\"\n\
	    set   LOSS_TO_ME($ssrc) \"101\"\n\
	    set LOSS_FROM_ME($ssrc) \"101\"\n\
	    incr attendance\n\
	    if [winfo exists .rat.received.attendance] {\n\
		.rat.received.attendance configure -text \"Attendance:$attendance\"\n\
	    }\n\
	}\n\
	name            { set NAME($ssrc) \"$arg\"}\n\
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
	loss_to_me      { set LOSS_TO_ME($ssrc) $arg }\n\
	loss_from_me    { set LOSS_FROM_ME($ssrc) $arg }\n\
	active          {\n\
	    if {[string compare $arg \"now\"] == 0} {\n\
#Find the location in the listbox CORRECT?\n\
                 set now [lsearch -exact $SSRC $ssrc]\n\
                .rat.group.names selection set $now\n\
                .rat.group.names configure -selectbackground white\n\
                .rat.group.names selection clear $now\n\
\n\
                #By deselecting cover prob of getting userloss stats\n\
                \n\
	    }\n\
	    if {[string compare $arg \"recent\"] == 0} { \n\
                 set recent [lsearch -exact $SSRC $ssrc]\n\
                .rat.group.names selection set $recent\n\
                .rat.group.names configure -selectbackground gray90\n\
                .rat.group.names selection clear $recent\n\
	    }\n\
	}\n\
	inactive        {\n\
            set inactive [lsearch -exact $SSRC $ssrc]\n\
            .rat.group.names selection set $inactive\n\
            .rat.group.names configure -selectbackground gray80\n\
            .rat.group.names selection clear $inactive \n\
	}	\n\
	default {\n\
	    puts stdout \"Unknown ssrc command 'ssrc $ssrc $cmd $arg'\"\n\
	}\n\
    }\n\
    ssrc_update $ssrc\n\
}\n\
\n\
###############################################################################\n\
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
##############################################################################\n\
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
#GLOBALS\n\
set attendance 0\n\
set globalhelp 1\n\
set basic_access 0\n\
#DEFAULT STARTING MODE IS MUTE\n\
set mute_status_out 1\n\
set mute_status_in 1\n\
#READ INFO. ONLY ON FIRST WINDOW ACCESS\n\
set access 0\n\
#DEFAULT FOR PRIMARY+SECONDARY ENCODING\n\
set pgp \"\"\n\
set name DVI\n\
set sname \"Redundancy: \"\n\
set x lossin\n\
#TO CONTROL BITMAPS DISPLAYED ON POWERMETERS FRAME\n\
set toggle_in 2\n\
set toggle_out 2\n\
set stop 0\n\
set sending 0\n\
#CONTROLLING CANVAS\n\
set mode_int 1\n\
set scaleset 2\n\
#CHANGE THIS VARIABLE TO DISPLAY LARGER/SMALLER 'LOSS-DIAMONDS'\n\
set treeht 8.5\n\
set raised 0\n\
set some_coding DVI\n\
global title\n\
\n\
frame .start -width 8c -height 3c -borderwidth 1m -relief groove\n\
pack .start -fill x\n\
frame .purpose -width 8c -height 3c -borderwidth 1m -relief groove\n\
pack .purpose -fill x\n\
frame .encrypt -width 8c -height 3c -borderwidth 1m -relief groove\n\
pack .encrypt -fill x\n\
\n\
checkbutton .again -text \"Show this screen next time\" -variable again -anchor e\n\
.again select\n\
button .proceed -text \"   Proceed   \" -command MainBasic\n\
bind .proceed <Button-1> set_defaults\n\
pack .again .proceed -side top -expand 1 -fill x\n\
button .quit -text \"   Quit   \" -command exit\n\
pack .quit -side right -fill x\n\
pack .proceed .quit -side left\n\
pack .quit -side right\n\
pack .again -side top\n\
\n\
label .start.stmode -text \"Start Mode :\" -padx 2m -pady 2m -anchor w\n\
radiobutton .start.basic -text Basic -variable mode -value basic -padx 2m -pady 2m -anchor w -command {selectmode $mode}\n\
radiobutton .start.advanced -text Advanced -variable mode -value advanced -padx 2m -pady 2m -anchor w -command {selectmode $mode}\n\
pack .start.stmode .start.basic .start.advanced -side left \n\
.start.basic select\n\
\n\
label .purpose.plabel -text \"Purpose :\" -padx 2m -pady 2m \n\
tk_optionMenu .purpose.pmenu choice  Meeting \"Receive lecture\" \"Give lecture\" \n\
pack .purpose.plabel .purpose.pmenu -side left \n\
\n\
#Sets the default modes, primary encodings\n\
proc set_defaults {} {\n\
	global RAT_ADDR\n\
	global choice\n\
	switch $choice {\n\
		Meeting {cb_send \"U\" $RAT_ADDR \"primary DVI\"\n\
			cb_send \"U\" $RAT_ADDR \"lecture 1\"\n\
			cb_send \"U\" $RAT_ADDR \"output mode Full duplex\"\n\
			}\n\
		\"Receive lecture\" {cb_send \"U\" $RAT_ADDR \"primary DVI\"\n\
			cb_send \"U\" $RAT_ADDR \"lecture 0\"\n\
			cb_send \"U\" $RAT_ADDR \"output mode Net mutes mike\"\n\
		}\n\
		\"Give lecture\" {cb_send \"U\" $RAT_ADDR \"primary DVI\"\n\
			cb_send \"U\" $RAT_ADDR \"lecture 1\"\n\
			cb_send \"U\" $RAT_ADDR \"output mode Mike mutes net\"\n\
		}\n\
	}\n\
}\n\
###############################################################################\n\
proc cb_recv_redundancy {args} {\n\
	#Set the redundancy variable to the chosen encoding value\n\
	global some_coding sname\n\
	if {$args != \"NONE\"} {\n\
		set some_coding $args\n\
		puts \"Entered recv some_coding = $some_coding\"\n\
	}\n\
	set sname \"Redundancy: $args\"\n\
}\n\
###############################################################################\n\
checkbutton .encrypt.setencrypt -text on/off -variable setencrypt -command checkentered -anchor w \n\
button .encrypt.usekey -text Encryption -pady 2m\n\
pack .encrypt.usekey .encrypt.setencrypt -side left \n\
wm title . \"Start-up Window\"\n\
\n\
\n\
bind .encrypt.usekey <Button-1> {setKey}\n\
##############################################################################\n\
proc selectmode mode {\n\
puts $mode\n\
    if {$mode == \"basic\"} {\n\
	.proceed configure -command MainBasic\n\
    } else {\n\
	.proceed configure -command MainAdvanced\n\
    }\n\
}\n\
##############################################################################\n\
proc setKey {} {\n\
    toplevel .key\n\
    global .encrypt.setencrypt\n\
    label .key.keylabel -text \"Key :\" -padx 2m -font *-times-medium-r-normal--*-140-*\n\
    entry .key.keyentry -width 50 -relief sunken -textvariable pgp\n\
    button .key.accept -text \"Accept\" -command \"destroy .key\"\n\
    button .key.cancel -text \"Cancel\" -command \"destroy .key\"\n\
    bind .key.cancel <Button-1>  {.encrypt.setencrypt deselect}\n\
\n\
    pack .key.keylabel .key.keyentry -side top\n\
    pack .key.accept .key.cancel -padx 1m -pady 2m -side left\n\
    pack .key.cancel -side right\n\
    focus .key.keyentry\n\
}\n\
##############################################################################\n\
#CANNOT SET ENCRYPTION IF KEY NOT ENTERED\n\
proc checkentered {} {\n\
    global pgp\n\
    global RAT_ADDR\n\
    global setencrypt\n\
    if {$setencrypt == 1} {\n\
	if {$pgp == \"\"} {\n\
	    .encrypt.setencrypt deselect\n\
	    setKey\n\
	} else {\n\
          Install_key $pgp\n\
	}\n\
    }\n\
}\n\
###############################################################################\n\
proc Install_key key {\n\
global RAT_ADDR\n\
cb_send \"U\" $RAT_ADDR \"update_key $key\"\n\
}\n\
\n\
###################################################################\n\
proc MainBasic {} {\n\
    wm withdraw .\n\
    global title\n\
    global basic_access\n\
    incr basic_access\n\
    global choice\n\
    global lossout\n\
    toplevel .rat\n\
    wm title .rat RAT:$choice:$title\n\
    frame .rat.left\n\
    pack .rat.left -side left\n\
    frame .rat.right\n\
    pack .rat.right -side right\n\
    frame .rat.group -width 8c -height 4c -bd 1m -relief groove\n\
    pack propagate .rat.group 0\n\
\n\
    frame .rat.received -width 8c -height 1.5c -bd 1m -relief groove\n\
    pack propagate .rat.received 0\n\
\n\
    frame .rat.sent -width 8c -height 2.7c -bd 1m -relief groove\n\
    pack propagate .rat.sent 0\n\
    pack .rat.group .rat.received .rat.sent -in .rat.left -side top \n\
    frame .rat.power -width 32m -height 8.2c -bd 1m -relief groove\n\
    pack propagate .rat.power 0\n\
    pack .rat.power -in .rat.right\n\
\n\
    #ADD TO RECEIVED PANEL\n\
    checkbutton .rat.received.adjr -text \"Adjust audio received    \" -variable adjr -command {repair $adjr}\n\
    label .rat.received.attendance -text \"Attendance :\" \n\
    pack .rat.received.adjr .rat.received.attendance -side left \n\
\n\
    #ADD TO SENT PANEL\n\
    checkbutton .rat.sent.adjs -text \"Adjust audio sent   \" -variable adjs -command {selectcolour $adjs}\n\
    pack .rat.sent.adjs -side left -anchor w\n\
    pack .rat.sent.adjs -side top\n\
    button .rat.sent.bandwidth -text \"Bandwidth\"\n\
    pack .rat.sent.bandwidth -side left\n\
    global name\n\
    bind .rat.sent.bandwidth <Button-1> {showCodecs name $scaleset} \n\
###############################################################################\n\
proc repair adjr {\n\
    global RAT_ADDR\n\
    if {$adjr == 0} {\n\
    cb_send \"U\" $RAT_ADDR \"repair None\"\n\
    } else {\n\
      cb_send \"U\" $RAT_ADDR \"repair Packet Repetition\"\n\
    }\n\
}\n\
###############################################################################\n\
    proc codechelp {} {\n\
	toplevel .ch\n\
	wm geometry .ch +450+200\n\
	wm title .ch Bandwidth\n\
	message .ch.bandhelp -width 4c -justify left -relief raised -bd 2 -font\\\n\
		-Adobe-Helvetica-Medium-R-Normal--*-140-* -text \"Bandwidth.\\nThe bandwidth \\\n\
		encoding defaults to DVI. Should audio quality be poor, the bandwidth can be lowered\\\n\
		by clicking on this button\"\n\
	pack .ch.bandhelp\n\
    }\n\
    #################################################################\n\
    #ADD COMPONENTS TO 'POWERMETERS' PANEL\n\
    button .rat.power.transmit -text \"Push to Talk\"\n\
global sending\n\
bind .rat.power.transmit <ButtonPress-1> \"set sending 1 ; checkmute2\"\n\
bind .rat.power.transmit <ButtonRelease-1> {set sending 0 ;cb_send \"U\" $RAT_ADDR \"toggle_send\"}\n\
    pack .rat.power.transmit -fill x -side top -ipady 5m\n\
    button .rat.power.logo -bitmap \"ucl\"\n\
    pack .rat.power.logo -fill x -side bottom\n\
\n\
\n\
    #TO HOLD THE POWERMETERS\n\
\n\
    frame .rat.power.in -height 5c -width 15m \n\
    pack propagate .rat.power.in 0\n\
    frame .rat.power.out -height 5c -width 15m \n\
    pack propagate .rat.power.out 0\n\
    pack .rat.power.in .rat.power.out -side left\n\
    pack .rat.power.in -anchor n\n\
    pack .rat.power.out -anchor n\n\
\n\
    button .rat.power.in.inicon -command toggle_in -bitmap \"head\"\n\
    button .rat.power.out.outicon -command toggle_out -bitmap \"mic\"\n\
    pack .rat.power.in.inicon -expand 1 -fill x -anchor n\n\
    pack .rat.power.out.outicon -expand 1 -fill x -anchor n\n\
    ##########################################################################\n\
    proc toggle_in {} {\n\
    global RAT_ADDR\n\
    cb_send \"U\" $RAT_ADDR \"toggle_output_port\"\n\
    \n\
}\n\
############################################################################\n\
proc toggle_out {} {\n\
global RAT_ADDR\n\
cb_send \"U\" $RAT_ADDR \"toggle_input_port\"\n\
}\n\
\n\
#############################################################################\n\
button .rat.power.in.rec -text Mute -relief raised -command checkmute1 \n\
\n\
button .rat.power.out.send -text UnMute -command checkmute2 -relief sunken -bg seashell4 -fg white\n\
pack .rat.power.in.rec -side top\n\
pack .rat.power.out.send -side top\n\
###################################################################\n\
proc checkmute1 {} {\n\
    global RAT_ADDR\n\
    cb_send \"U\" $RAT_ADDR \"toggle_play\"\n\
    if {[string compare [.rat.power.in.rec cget -relief] raised] == 0} {\n\
	.rat.power.in.rec configure -relief sunken -text UnMute -bg seashell4 -fg white\n\
	\n\
\n\
    } else {\n\
	.rat.power.in.rec configure -relief raised -text Mute -bg gray80 -fg black\n\
	\n\
}\n\
}\n\
##################################################################\n\
proc checkmute2 {} { \n\
global sending\n\
global raised\n\
global stop\n\
global RAT_ADDR\n\
cb_send \"U\" $RAT_ADDR \"toggle_send\"\n\
    if {[string compare [.rat.power.out.send cget -relief] raised] == 0} {\n\
        .rat.power.out.send configure -relief sunken -text UnMute -bg seashell4 -fg white\n\
\n\
	    if {$sending !=1} {\n\
.rat.power.transmit configure -text \"Push to Talk\"\n\
set raised 0\n\
}\n\
\n\
    } else {\n\
        .rat.power.out.send configure -relief raised -text Mute -bg grey80 -fg black\n\
	if {$sending !=1} {\n\
.rat.power.transmit configure -text \"Push to Stop\"\n\
set stop 1\n\
set raised 1\n\
}\n\
}   \n\
\n\
}\n\
##################################################################\n\
\n\
\n\
#############################################################################\n\
#ADD THE CANVAS' AND SCALE\n\
scale .rat.power.in.sin -from 100 -to 0 -length 3c -width 1.8m -orient vertical  -command set_vol\n\
.rat.power.in.sin set 0\n\
pack .rat.power.in.sin -side bottom\n\
\n\
\n\
bargraphCreate .rat.power.in.cin\n\
\n\
pack .rat.power.in.cin .rat.power.in.sin -side left\n\
pack .rat.power.in.cin -side bottom\n\
pack .rat.power.in.sin -side right\n\
pack .rat.power.in.cin .rat.power.in.sin -anchor s\n\
\n\
bargraphCreate .rat.power.out.cout\n\
pack .rat.power.out.cout -side bottom\n\
pack .rat.power.out.cout -side left\n\
pack .rat.power.out.cout -anchor s\n\
\n\
scale .rat.power.out.sout -from 100 -to 0 -length 3c -width 1.8m -orient vertical -command set_gain\n\
.rat.power.out.sout set 0\n\
pack .rat.power.out.sout -side bottom\n\
##################################################################\n\
proc agchelp {} {\n\
    toplevel .agch\n\
    wm geometry .agch +450+400\n\
    wm title .agch \"AGC\"\n\
    message .agch.agchelp -width 4c -justify left -relief raised -bd 2\\\n\
	    -font -Adobe-Helvetica-Medium-R-Normal--*-140-* -text \"Automatic Gain Control.\\n\\Buffers incoming sound to volume as set by this scale\"\n\
    pack .agch.agchelp\n\
}\n\
###################################################################\n\
\n\
\n\
#ADD TO RECEIVED PANEL\n\
global mode_int\n\
if {$mode_int == 2} {\n\
\n\
    listbox .rat.group.names -yscrollcommand [list .rat.group.namescroll set] -width 23\n\
    scrollbar .rat.group.namescroll -command [list BindYview [list .rat.group.names .rat.group.tree]]\n\
    canvas .rat.group.tree -bg black -width 0.7c -height 4c -yscrollcommand [list .rat.group.namescroll set]\n\
    pack .rat.group.namescroll -side left -fill y\n\
    pack .rat.group.tree -side left\n\
    pack .rat.group.names -side left \n\
} else {\n\
    listbox .rat.group.names -yscrollcommand \".rat.group.namescroll set\" -width 27\n\
    scrollbar .rat.group.namescroll -command \".rat.group.names yview\"\n\
    pack .rat.group.namescroll -side left -fill y\n\
    pack .rat.group.names -side left -fill x\n\
}\n\
##############################################################################\n\
proc BindYview { lists args } {\n\
foreach l $lists {\n\
    eval {$l yview} $args\n\
}\n\
}\n\
##############################################################################\n\
button .rat.group.userdetails -text User\\ndetails -command userwindow\n\
menubutton .rat.group.useroptions -text options -relief raised -menu .rat.group.useroptions.menu\n\
\n\
pack .rat.group.userdetails -side right -anchor n\n\
pack .rat.group.userdetails .rat.group.useroptions -side top -anchor w\n\
menu .rat.group.useroptions.menu\n\
global globalhelp\n\
.rat.group.useroptions.menu add checkbutton -label \"Show Startup Window\" -variable again\n\
.rat.group.useroptions.menu add checkbutton -label \"Show Help\" -variable showhelp -command {togglehelp $showhelp}\n\
\n\
.rat.group.useroptions.menu add separator\n\
\n\
proc warn {} {\n\
toplevel .warn \n\
wm geometry .warn +400+300\n\
wm title .warn \"Invalid Click\"\n\
message .warn.whelp -width 4c -justify left -relief raised -bd 2 -font -Adobe-Helvetica-Medium-R-Normal--*-140-* -text \"Push to Stop Mode.\\nYou cannot display audio loss statistics on yourself in this mode.\"\n\
pack .warn.whelp\n\
button .warn.ok -text ok -command \"destroy .warn\"\n\
pack .warn.ok -side bottom -anchor e\n\
}\n\
#Cannot bring up stats on oneself due to listbox selection problem\n\
#Cannot get the selection when push to stop is running\n\
#Toggle back to bring up warning window\n\
#Not enough time to toggle? Therefore works only 50% of time\n\
bind .rat.group.names <Double-1> {if {$stop == 1} {\n\
puts \"Toggle send from binding\"\n\
cb_send \"U\" $RAT_ADDR \"toggle_send\"\n\
global NAME CNAME RAT_ADDR myssrc\n\
  if {[selection get] != $NAME($myssrc) | [selection get] != $CNAME($myssrc)} {\n\
puts [selection get]\n\
showloss [selection get] [lindex $SSRC [.rat.group.names curselection]]\n\
  } else {\n\
warn\n\
return\n\
    }\n\
\n\
} else {\n\
    showloss [selection get] [lindex $SSRC [.rat.group.names curselection]]\n\
}\n\
}\n\
\n\
\n\
#ALLOW THE USER TO CHANGE PURPOSE FROM MAIN WINDOW\n\
.rat.group.useroptions.menu add cascade -label Purpose -menu .rat.group.useroptions.menu.purposechange\n\
menu .rat.group.useroptions.menu.purposechange\n\
.rat.group.useroptions.menu add separator\n\
global choice \n\
global purpose\n\
.rat.group.useroptions.menu.purposechange add radiobutton -label Meeting -variable choice -value Meeting -command changep\n\
.rat.group.useroptions.menu.purposechange add radiobutton -label \"Receive lecture\" -variable choice -value \"Receive lecture\" -command changep\n\
.rat.group.useroptions.menu.purposechange add radiobutton -label \"Give lecture\" -variable choice -value \"Give lecture\" -command changep\n\
.rat.group.useroptions.menu add command -label \"Show Advanced\" -command MainAdvanced\n\
###################################################################\n\
proc changep {} {\n\
    global RAT_ADDR\n\
    global title\n\
    global choice\n\
    wm title .rat RAT:$choice:$title\n\
switch $choice {\n\
Meeting {cb_send \"U\" $RAT_ADDR \"primary DVI\"\n\
cb_send \"U\" $RAT_ADDR \"lecture 1\"\n\
}\n\
\"Receive lecture\" {cb_send \"U\" $RAT_ADDR \"primary DVI\"\n\
cb_send \"U\" $RAT_ADDR \"lecture 0\"\n\
}\n\
\"Give lecture\" {cb_send \"U\" $RAT_ADDR \"primary DVI\"\n\
cb_send \"U\" $RAT_ADDR \"lecture 1\"\n\
}\n\
}\n\
}\n\
###################################################################\n\
\n\
#############################################################################\n\
#Not possible to put the flash command in because of interrupt dynamic \n\
#message passing on conference bus\n\
proc update_loss {ssrc} {\n\
	global adjs SSRC\n\
	global LOSS_FROM_ME \n\
	global red \n\
	global orange\n\
        global attendance av_loss\n\
    set count 0\n\
    set av_loss 0\n\
    for {set i 0} {$i < $attendance} {incr i} {\n\
       if [winfo exists .rat.group.names] {\n\
        set rec_loss [lindex $SSRC $i]\n\
	   if {$rec_loss == \"\"} {\n\
                break\n\
	   }\n\
        \n\
        incr count $LOSS_FROM_ME($rec_loss)\n\
        incr i\n\
       }	\n\
}\n\
      \n\
        set av_loss [expr $count / $attendance]\n\
       \n\
###############################################################################\n\
	if {$av_loss >=20} {\n\
    		if {$adjs !=1} {\n\
			.rat.sent.adjs configure -bg red\n\
        		set red 1\n\
        		set orange 0 \n\
    		} else {\n\
			if {$av_loss >10} {\n\
           			if  {$adjs !=1} {\n\
           				set orange 1\n\
           				set red 0\n\
            				.rat.sent.adjs configure -bg orange\n\
       				}   \n\
			} else {\n\
          			.rat.sent.adjs configure -bg grey\n\
           			set red 0\n\
           			set orange 0\n\
			}\n\
    		}\n\
	}\n\
}\n\
##############################################################################\n\
#If redundancy is selected no red configuration\n\
proc selectcolour {adjs} {\n\
global RAT_ADDR\n\
global mode_int\n\
global sname\n\
global red\n\
global orange\n\
global some_coding\n\
if {$mode_int == 2} {\n\
    if {$adjs == 1} {\n\
         cb_send \"U\" $RAT_ADDR \"redundancy $some_coding\"\n\
	.rat.sent.adjs configure -bg grey\n\
	# Because of the .rat.sent.adjs configure -textvariable sname, any\n\
	# change we make to sname will automagically be updated on the screen! :-(\n\
	set sname \"Redundancy: $some_coding\"\n\
    } else {\n\
            puts \"Entered off statements sname org= $sname\"\n\
            set sname \"Redundancy: $some_coding\"\n\
            cb_send \"U\" $RAT_ADDR \"redundancy NONE\"\n\
	if {$red == 1} {\n\
           .rat.sent.adjs configure -bg red\n\
	}\n\
        if {$orange == 1} {\n\
           .rat.sent.adjs configure -bg orange\n\
	}\n\
           \n\
            \n\
    }\n\
###############BASIC######################################################    \n\
} else {\n\
    if {$adjs == 1} {\n\
     .rat.sent.adjs configure -bg grey\n\
     cb_send \"U\" $RAT_ADDR \"redundancy DVI\"\n\
    } else {\n\
        cb_send \"U\" $RAT_ADDR \"redundancy NONE\"\n\
	if {$red == 1} {\n\
           .rat.sent.adjs configure -bg red\n\
	}\n\
        if {$orange == 1} {\n\
           .rat.sent.adjs configure -bg orange\n\
	}\n\
    }\n\
}\n\
}\n\
###############################################################################\n\
proc nameshelp {} {\n\
    toplevel .nh\n\
    wm geometry .nh +450+300\n\
    wm title .nh Participants\n\
    message .nh.nhhelp -width 4c -justify left -relief raised -bd 2 -font -Adobe-Helvetica-Medium-R-Normal--*-140-* -text \"Participants.\\n\\Single click name to mute.\\nDouble click name to display information and \\\n\
	    audio loss statistics for that participant.\"\n\
    pack .nh.nhhelp\n\
}\n\
##############################################################################\n\
proc showloss {name ssrc} {\n\
puts \"Entered Showloss\"\n\
global stop\n\
global RAT_ADDR\n\
if {$stop == 1} {\n\
cb_send \"U\" $RAT_ADDR \"toggle_send\"\n\
}\n\
global SSRC\n\
set select [lsearch -exact $SSRC $ssrc]\n\
    .rat.group.names selection clear $select\n\
    global EMAIL PHONE LOC TOOL ENCODING CNAME DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME NAME\n\
    global mode_int\n\
    if {$mode_int == 2} {\n\
	if [winfo exists .advsl$ssrc] {\n\
	    updateshowloss $ssrc\n\
	} else {\n\
	    toplevel .advsl$ssrc\n\
	    wm title .advsl$ssrc $name\n\
	     \n\
	    label .advsl$ssrc.name -text \"Name:   $NAME($ssrc)\" \n\
	    label .advsl$ssrc.email -text \"Email:   $EMAIL($ssrc)\" \n\
	    label .advsl$ssrc.phone -text \"Phone:   $PHONE($ssrc)\" \n\
	    label .advsl$ssrc.location -text \"Location:  $LOC($ssrc)\" \n\
	    label .advsl$ssrc.tool -text \"Tool:   $TOOL($ssrc)\" \n\
	    label .advsl$ssrc.cname -text \"CNAME:  $CNAME($ssrc)\" \n\
	    label .advsl$ssrc.enc -text \"Audio Encoding: $ENCODING($ssrc)\" \n\
	    label .advsl$ssrc.length -text \"Audio Length: $DURATION($ssrc)\" \n\
	    label .advsl$ssrc.rec -text \"Packets Received: $PCKTS_RECV($ssrc)\" \n\
	    label .advsl$ssrc.lost -text \"Packets Lost: $PCKTS_LOST($ssrc)\" \n\
	    label .advsl$ssrc.mis -text \"Packets Misordered: $PCKTS_MISO($ssrc)\" \n\
	    label .advsl$ssrc.drop -text \"Units Dropped: $JITTER_DROP($ssrc)\" \n\
	    label .advsl$ssrc.jitter -text \"Jitter: $JITTER($ssrc)\" \n\
	    label .advsl$ssrc.inst_loss -text \"Instantaneous Loss Rate: $LOSS_TO_ME($ssrc)\" \n\
	    button .advsl$ssrc.ok -text ok -command \"destroy .advsl$ssrc\"\n\
\n\
	    pack .advsl$ssrc.name .advsl$ssrc.email .advsl$ssrc.phone .advsl$ssrc.location .advsl$ssrc.tool .advsl$ssrc.cname .advsl$ssrc.enc .advsl$ssrc.length .advsl$ssrc.rec .advsl$ssrc.lost .advsl$ssrc.lost .advsl$ssrc.mis .advsl$ssrc.drop .advsl$ssrc.jitter .advsl$ssrc.inst_loss .advsl$ssrc.ok -side top -anchor w\n\
	    pack .advsl$ssrc.ok -side right\n\
	}\n\
    } else {\n\
	if [winfo exists .sl$ssrc] {\n\
	    updateshowloss $ssrc\n\
	} else {\n\
	    toplevel .sl$ssrc\n\
	    wm title .sl$ssrc $name\n\
	    frame .sl$ssrc.left\n\
	    frame .sl$ssrc.right \n\
	    pack .sl$ssrc.left -side left\n\
	    pack .sl$ssrc.right -side right\n\
	    label .sl$ssrc.left.quality -text \"Audio Quality From: \" \n\
	    label .sl$ssrc.right.qualityl -width 1 -height 1 \n\
	    label .sl$ssrc.left.pname -text Name: \n\
	    label .sl$ssrc.left.pemail -text Email:\n\
	    label .sl$ssrc.right.realname -text $NAME($ssrc)\n\
	    label .sl$ssrc.right.realmail -text \"$EMAIL($ssrc)\"\n\
	    label .sl$ssrc.left.lossrate -text \"Loss Rate:\"\n\
\n\
	    if { [.rat.group.names curselection] == 0 } {\n\
		set x $LOSS_FROM_ME($ssrc)\n\
		label .sl$ssrc.right.lossstat -text $x\n\
		.sl$ssrc.left.quality configure -text \"Avr. Audio Quality From: \"\n\
	    } else {\n\
		set x $LOSS_TO_ME($ssrc)\n\
		label .sl$ssrc.right.lossstat -text $x\n\
	    }\n\
\n\
	    button .sl$ssrc.right.ok -text ok -width 1 -height 1 -command \"destroy .sl$ssrc\"\n\
         \n\
\n\
	    if {$x <= 10} {\n\
		.sl$ssrc.right.qualityl configure -bg green\n\
	    } else {\n\
		if {$x <=20} {\n\
		    .sl$ssrc.right.qualityl configure -bg orange\n\
		} else {\n\
		    .sl$ssrc.right.qualityl configure -bg red\n\
	    }   }\n\
	    label .sl$ssrc.right.dummy\n\
	    pack .sl$ssrc.left.quality .sl$ssrc.left.pname .sl$ssrc.left.pemail  .sl$ssrc.left.lossrate -side top  \n\
	    pack .sl$ssrc.left.quality -pady 1\n\
	    pack .sl$ssrc.right.qualityl .sl$ssrc.right.realname .sl$ssrc.right.realmail .sl$ssrc.right.dummy .sl$ssrc.right.lossstat -side top -anchor s -padx 10\n\
	    pack .sl$ssrc.right.ok -side bottom -anchor e\n\
\n\
	    #ADJUST FOR BUTTON SPACE \n\
	    pack .sl$ssrc.left.lossrate -pady 25\n\
	    pack .sl$ssrc.right.realmail -pady 8\n\
	    pack .sl$ssrc.left.pemail -pady 7\n\
\n\
	}\n\
    }\n\
}\n\
\n\
##############################################################################\n\
proc updateshowloss {ssrc} { \n\
    global EMAIL PHONE LOC TOOL ENCODING CNAME DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME NAME\n\
    global mode_int av_loss\n\
    if {$mode_int == 2} {\n\
	\n\
	.advsl$ssrc.name configure -text \"Name:   $NAME($ssrc)\" \n\
	.advsl$ssrc.email configure -text \"Email:   $EMAIL($ssrc)\" \n\
	.advsl$ssrc.phone configure -text \"Phone:   $PHONE($ssrc)\" \n\
	.advsl$ssrc.location configure -text \"Location:  $LOC($ssrc)\" \n\
	.advsl$ssrc.tool configure -text \"Tool:   $TOOL($ssrc)\" \n\
	.advsl$ssrc.cname configure -text \"CNAME:  $CNAME($ssrc)\" \n\
	.advsl$ssrc.enc configure -text \"Audio Encoding: $ENCODING($ssrc)\" \n\
	.advsl$ssrc.length configure -text \"Audio Length: $DURATION($ssrc)\" \n\
	.advsl$ssrc.rec configure -text \"Packets Received: $PCKTS_RECV($ssrc)\"\n\
	.advsl$ssrc.lost configure -text \"Packets Lost: $PCKTS_LOST($ssrc)\" \n\
	.advsl$ssrc.mis configure -text \"Packets Misordered: $PCKTS_MISO($ssrc)\" \n\
	.advsl$ssrc.drop configure -text \"Units Dropped: $JITTER_DROP($ssrc)\" \n\
	.advsl$ssrc.jitter configure -text \"Jitter: $JITTER($ssrc)\" \n\
	.advsl$ssrc.inst_loss configure -text \"Instantaneous Loss Rate: $LOSS_TO_ME($ssrc)\" \n\
\n\
	#ELSE CONFIGURE BASIC INFO.\n\
    } else {\n\
	.sl$ssrc.right.realmail configure -text \"$EMAIL($ssrc)\"\n\
	.sl$ssrc.left.lossrate configure -text \"Loss Rate: $LOSS_TO_ME($ssrc)\"\n\
\n\
	if { [.rat.group.names curselection] == 0 } {\n\
	    set x $av_loss\n\
	    .sl$ssrc.left.quality configure -text \"Avr. Audio Quality From: \"\n\
	} else {\n\
	    set x $LOSS_TO_ME($ssrc)\n\
	    .sl$ssrc.right.lossstat configure -text $x\n\
	}\n\
\n\
	if {$x <= 10} {\n\
	    .sl$ssrc.right.qualityl configure -bg green\n\
	} else {\n\
	    if {$x <=20} {\n\
		.sl$ssrc.right.qualityl configure -bg orange\n\
	    } else {\n\
		.sl$ssrc.right.qualityl configure -bg red\n\
	}   }\n\
    }\n\
\n\
}\n\
\n\
    #Add 'Top-level' buttons\n\
    button .rat.right.mainquit -text \"   Quit   \" -command exit\n\
    bind .rat.right.mainquit <Button-1> exit\n\
    pack .rat.right.mainquit -side bottom\n\
    button .rat.left.about -text \"   About   \" -command showabout\n\
    pack .rat.left.about -side bottom -expand 1 -fill x\n\
    pack .rat.left.about -side left\n\
    pack .rat.right.mainquit -side right -expand 1 -fill both\n\
\n\
\n\
\n\
    # This is the end of the MainBasic procedure. Last thing we do, now we've\n\
    # created the window, is update the list of participants...\n\
    ssrc_update_all\n\
    read_details\n\
\n\
\n\
}\n\
\n\
#Command for reading in from the file########################################\n\
#Called when myssrc has been received\n\
proc read_details {} {\n\
global NAME EMAIL PHONE LOC \n\
global myssrc win32 rtpfname\n\
puts \"Email is $EMAIL($myssrc)\"\n\
puts \"myssrc is $myssrc\"\n\
set $NAME($myssrc)  [option get . rtpName  rat]\n\
set $EMAIL($myssrc) [option get . rtpEmail rat]\n\
set $PHONE($myssrc) [option get . rtpPhone rat]\n\
set $LOC($myssrc)   [option get . rtpLoc   rat]\n\
\n\
if {$win32 == 0} {\n\
    if {$NAME($myssrc) == \"\" && [file readable $rtpfname] == 1} {\n\
	set f [open $rtpfname]\n\
	while {[eof $f] == 0} {\n\
	    gets $f line\n\
	    if {[string compare \"*rtpName:\"  [lindex $line 0]] == 0} {set $NAME($myssrc)  [lrange $line 1 end]}\n\
	    if {[string compare \"*rtpEmail:\" [lindex $line 0]] == 0} {set $EMAIL($myssrc) [lrange $line 1 end]}\n\
	    if {[string compare \"*rtpPhone:\" [lindex $line 0]] == 0} {set $PHONE($myssrc) [lrange $line 1 end]}\n\
	    if {[string compare \"*rtpLoc:\"   [lindex $line 0]] == 0} {set $LOC($myssrc)   [lrange $line 1 end]}\n\
	}\n\
	close $f\n\
    }\n\
} else {\n\
    if {$NAME($myssrc) == \"\"} {\n\
	catch {set $NAME($myssrc)  [getregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpName\"]  } \n\
	catch {set $EMAIL($myssrc) [getregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpEmail\"] } \n\
	catch {set $PHONE($myssrc) [getregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpPhone\"] } \n\
	catch {set $LOC($myssrc)   [getregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpLoc\"  ] } \n\
    }\n\
}\n\
\n\
if {$NAME($myssrc) == \"\"} {\n\
    toplevel .name\n\
    wm title .name \"RAT User Information\"\n\
    message .name.m -width 1000 -text {\n\
	Please enter the following details, for transmission\n\
	to other conference participants.\n\
    }\n\
	frame  .name.b\n\
    label  .name.b.res -text \"Name:\"\n\
    entry  .name.b.e -highlightthickness 0 -width 20 -relief sunken -textvariable NAME($myssrc)\n\
    button .name.d -highlightthickness 0 -padx 0 -pady 0 -text Done -command {NAME($myssrc $NAME($myssrc); saving; destroy .name}\n\
    bind   .name.b.e <Return> {NAME($myssrc) $NAME($myssrc); saving; destroy .name}\n\
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
}\n\
\n\
\n\
\n\
\n\
#HANDLE BANDWIDTH CLICK########################################################\n\
proc showCodecs {name scaleset} {\n\
    frame .rat.sent.packetframe -width 5c -height 2c\n\
    pack .rat.sent.packetframe -side right\n\
    scale .rat.sent.packetframe.packetscale -from 0 -to 4 -length 3c -orient horizontal -command changec\n\
    .rat.sent.packetframe.packetscale set $scaleset\n\
    label .rat.sent.packetframe.packetlabel -textvariable $name\n\
\n\
    button .rat.sent.packetframe.packetok -text ok -width 1 -height 1 -command \"destroy .rat.sent.packetframe\"\n\
    bind .rat.sent.packetframe.packetok <Button-1> {cb_send \"U\" $RAT_ADDR \"primary $name\"}\n\
\n\
\n\
    pack .rat.sent.packetframe.packetlabel .rat.sent.packetframe.packetscale .rat.sent.packetframe.packetok -side left\n\
    pack .rat.sent.packetframe.packetscale .rat.sent.packetframe.packetok -side top\n\
}\n\
###############################################################################\n\
proc changec value {\n\
    global name\n\
    global scaleset\n\
    set psize [.rat.sent.packetframe.packetscale get]\n\
    switch $psize {\n\
	0 {set name LPC\n\
	set scaleset 0\n\
    }\n\
    1 {set name GSM\n\
    set scaleset 1\n\
}\n\
2 {set name DVI\n\
set scaleset 2\n\
}\n\
3 {set name PCM\n\
set scaleset 3\n\
}\n\
4 {set name \"16-bit linear\"\n\
set scaleset 4\n\
}\n\
}\n\
}\n\
\n\
###############################################################################\n\
\n\
\n\
\n\
#ADJUST AUTOMATIC GAIN CONTROL\n\
proc agc {} {\n\
}\n\
\n\
#COPYRIGHT INFO\n\
proc showabout {} {\n\
    toplevel .about\n\
    wm title .about \"About RAT\"\n\
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
button .about.c -highlightthickness 0 -padx 0 -pady 0 -text Copyright -command copyright\n\
button .about.d -highlightthickness 0 -padx 0 -pady 0 -text Quit   -command \"destroy .about\"\n\
pack .about.a .about.b .about.c .about.d -side top -fill x\n\
pack .about.a.b -side left -fill y\n\
pack .about.a.m -side right -fill both -expand 1\n\
wm resizable .about 0 0\n\
}\n\
############################################################################\n\
proc copyright {} {\n\
toplevel .copyright\n\
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
button .copyright.d -highlightthickness 0 -padx 0 -pady 0 -text Dismiss   -command \"destroy .copyright\"\n\
pack   .copyright.m .copyright.d -side top -fill x -expand 1\n\
wm title     .copyright \"RAT Copyright\"\n\
wm resizable .copyright 0 0\n\
}\n\
\n\
#SHOW USER INFO################################################################\n\
proc userwindow {} {\n\
global CNAME NAME EMAIL LOC PHONE TOOL SSRC\n\
global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME \n\
global myssrc\n\
\n\
    toplevel .uw\n\
    wm title .uw \"User Details\"\n\
    frame .uw.left\n\
    frame .uw.right\n\
    pack .uw.left -side left\n\
    pack .uw.right -side right\n\
    label .uw.left.name -text Name:\n\
    label .uw.left.email -text Email:\n\
    label .uw.left.phone -text Phone:\n\
    label .uw.left.location -text Location:\n\
    entry .uw.right.namee -relief sunken -bd 1 -textvariable NAME($myssrc) -width 20\n\
#Need to change where the insertion curser is?\n\
    entry .uw.right.emaile -relief sunken -bd 1 -textvariable EMAIL($myssrc) -width 20\n\
    entry .uw.right.phonee -relief sunken -bd 1 -textvariable PHONE($myssrc) -width 20\n\
    entry .uw.right.locatione -relief sunken -bd 1 -textvariable LOC($myssrc) -width 20\n\
\n\
    pack .uw.right.namee .uw.right.emaile .uw.right.phonee .uw.right.locatione -side top -pady 2m\n\
    focus .uw.right.namee\n\
    button .uw.right.ok -text \"OK\" -height 1 -command \"update_names; saving; destroy .uw\"\n\
\n\
\n\
\n\
    pack .uw.right.ok -anchor se\n\
    pack .uw.left.name .uw.left.email .uw.left.phone .uw.left.location -side top -anchor w -pady 5\n\
\n\
    #ADJUST FOR BUTTON SPACE\n\
    pack .uw.left.location -pady 20\n\
    pack .uw.left.email -pady 12\n\
    pack .uw.left.name -pady 1\n\
\n\
    bind .uw.right.ok <Button-1> {\n\
	.rat.group.names delete 0\n\
	.rat.group.names insert 0 $NAME($myssrc)\n\
    }\n\
}\n\
\n\
###############################################################################\n\
proc togglehelp showhelp {\n\
    global globalhelp\n\
    #eval showhelp\n\
    if {$showhelp == 1} {\n\
	bind .rat.sent.bandwidth <Enter> {codechelp}\n\
	bind .rat.sent.bandwidth <Leave> \"destroy .ch\"\n\
	bind .rat.power.in.sin <Enter> {agchelp}\n\
	bind .rat.power.in.sin <Leave> \"destroy .agch\"\n\
	bind .rat.group.names <Enter> {nameshelp}\n\
	bind .rat.group.names <Leave> \"destroy .nh\"\n\
    } else {\n\
	bind .rat.sent.bandwidth <Enter> \"\"\n\
	bind .rat.sent.bandwidth <Leave> \"\"\n\
	bind .rat.power.in.sin <Enter> \"\"\n\
	bind .rat.power.in.sin <Leave> \"\"\n\
	bind .rat.group.names <Enter> \"\"\n\
	bind .rat.group.names <Leave> \"\"\n\
    }\n\
}\n\
\n\
##############################################################################\n\
#CREATE THE ADVANCED RAT\n\
proc MainAdvanced {} {\n\
global SSRC\n\
global raised\n\
global RAT_ADDR\n\
\n\
\n\
    global mode_int\n\
    set mode_int 2\n\
    global basic_access\n\
    if {$basic_access == 1} {\n\
	destroy .rat\n\
        foreach i $SSRC {\n\
          if [winfo exists .sl$i ] {\n\
             destroy .sl$i\n\
          }\n\
        }\n\
    }\n\
    {MainBasic}\n\
    if {$raised == 1} {\n\
      .rat.power.transmit configure -text \"Push to Stop\"\n\
     \n\
    }\n\
    #Add to the advanced RAT\n\
    global globalhelp\n\
    global name\n\
    global attendance\n\
    .rat.received.attendance configure -text Attendance:$attendance\n\
    global scaleset\n\
    global sname\n\
    global Dest Port TTL\n\
    .rat.received.adjr configure -text \"Packet Repetition\"\n\
    .rat.sent.adjs configure -textvariable sname\n\
    .rat.group.useroptions.menu delete end\n\
    destroy .rat.sent.bandwidth\n\
    button .rat.sent.bandwidth\n\
\n\
    pack .rat.received.adjr -anchor n\n\
    pack .rat.received.attendance -anchor n\n\
\n\
    label .rat.received.dlabel -text Duration\n\
    tk_optionMenu .rat.received.dmenu size 20ms 40ms 80ms 160ms\n\
   # bind .rat.received.dmenu <Button-1> \"getrate\"\n\
    bind .rat.received.dmenu <ButtonRelease> \"getrate\"\n\
    proc getrate {} {\n\
    set size [.rat.received.dmenu cget -text]\n\
    rate $size\n\
    }\n\
\n\
    pack .rat.received.adjr .rat.received.dlabel .rat.received.dmenu -side top -anchor w\n\
    pack .rat.received.attendance -side right\n\
    pack .rat.received.dlabel .rat.received.dmenu -side left -anchor w\n\
\n\
\n\
\n\
    proc draw_enc_frame {} {\n\
	frame .rat.sent.encoding -width 7.5c -height 1.5c -bd 1m -relief groove\n\
\n\
	pack propagate .rat.sent.encoding 0\n\
	pack .rat.sent.adjs .rat.sent.encoding .rat.sent.rtp -side top\n\
	pack .rat.sent.rtp -side bottom\n\
\n\
	button .rat.sent.encoding.encoding -text Encoding -command advanced_enc\n\
	pack .rat.sent.encoding.encoding -side left\n\
\n\
    }\n\
    label .rat.sent.rtp -text \"  Dest: $Dest     Port: $Port     TTL: $TTL\" \n\
    pack .rat.sent.adjs .rat.sent.rtp -side top -anchor w\n\
    pack .rat.sent.rtp -side left -anchor s\n\
    draw_enc_frame\n\
\n\
\n\
\n\
\n\
\n\
\n\
    proc advanced_enc {} {\n\
	global name sname scaleset\n\
	#REDRAW THE FRAME FOR SCALES\n\
	destroy .rat.sent.encoding\n\
	frame .rat.sent.encoding -width 7.5c -height 1.5c -bd 1m -relief groove\n\
	pack propagate .rat.sent.encoding 0\n\
\n\
	pack .rat.sent.adjs .rat.sent.encoding .rat.sent.rtp -side top\n\
	pack .rat.sent.rtp -side bottom\n\
\n\
        global RAT_ADDR\n\
	scale .rat.sent.encoding.pscale -from 0 -to 4 -length 3c -orient horizontal -command changeadvc -showvalue 0\n\
	.rat.sent.encoding.pscale set $scaleset\n\
	label .rat.sent.encoding.plabel -textvariable name -width 15\n\
	button .rat.sent.encoding.pbutton -text ok -width 1 -height 1 -command \"destroy .rat.sent.encoding ; draw_enc_frame\"\n\
	pack .rat.sent.encoding.plabel .rat.sent.encoding.pscale .rat.sent.encoding.pbutton -side left\n\
	pack .rat.sent.encoding.pbutton -side right\n\
\n\
\n\
	bind .rat.sent.encoding.pbutton <Button-1> {\n\
        cb_send \"U\" $RAT_ADDR \"redundancy $short\"\n\
	#Rat will automatically reconfigure the primary to the appropriate bdwdth\n\
        global prim\n\
        cb_send \"U\" $RAT_ADDR \"primary $prim\"       \n\
}\n\
\n\
\n\
\n\
}\n\
\n\
proc changeadvc value {\n\
    global short\n\
    global prim\n\
    set short DVI\n\
    global name\n\
    global scaleset\n\
    global sname\n\
    set psize [.rat.sent.encoding.pscale get]\n\
    switch $psize {\n\
	0 {set name Primary:LPC\n\
        set prim LPC\n\
	set sname Redundancy:LPC\n\
        set short LPC\n\
	set scaleset 0\n\
    }\n\
    1 {set name Primary:GSM\n\
    set prim GSM\n\
    set sname Redundancy:GSM\n\
    set short GSM\n\
    set scaleset 1\n\
}\n\
2 {set name Primary:DVI\n\
set prim DVI\n\
set scaleset 2\n\
set sname Redundancy:DVI\n\
set short DVI\n\
}\n\
3 {set name Primary:PCM\n\
set prim PCM\n\
set sname Redundancy:DVI\n\
set scaleset 3\n\
set short DVI\n\
}\n\
4 {set name Primary:16-bit\\nlinear\n\
set prim 16-bitlinear\n\
set scaleset 4\n\
set sname Redundancy:DVI\n\
set short DVI\n\
}\n\
}\n\
}\n\
\n\
\n\
}\n\
\n\
proc ssrc_update {ssrc} {\n\
    global mode_int\n\
    global CNAME NAME EMAIL LOC PHONE TOOL SSRC\n\
    global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME\n\
\n\
    # Only try to update the list if the window has been created. This\n\
    # prevents problems occuring whilst the initial window is active.\n\
    if [winfo exists .rat.group.names] {\n\
	set pos [lsearch -exact $SSRC $ssrc]\n\
	if {$pos == -1} {\n\
	    # This is a new name, so add to the end of the list...\n\
	    .rat.group.names insert end $NAME($ssrc)\n\
            set pos [expr [.rat.group.names index end] - 1]\n\
	    lappend SSRC $ssrc\n\
	} else {\n\
	    # It's someone we've seen before, who's changed their name...\n\
	    .rat.group.names delete $pos\n\
	    .rat.group.names insert $pos $NAME($ssrc)\n\
	}\n\
    }\n\
    #UPDATE THE PARTICIPANT STATS\n\
   \n\
    if {$mode_int ==2} {\n\
	if [winfo exists .advsl$ssrc] {\n\
	    updateshowloss $ssrc\n\
	} else {\n\
	    if [winfo exists .sl$ssrc] {\n\
		updateshowloss $ssrc\n\
	    }\n\
	}\n\
\n\
\n\
\n\
    }\n\
    update_loss $ssrc\n\
\n\
    if {[winfo exists .rat.group.tree]} {\n\
       global treeht\n\
       set i [expr $pos + 1]\n\
#Set the appropriate position for drawing the polygon\n\
       set ht [expr $treeht * [expr $i + $pos]]\n\
       set top [expr $ht - $treeht]\n\
       set bottom [expr $ht + $treeht]\n\
#Create the polygon to hold the loss data, identified by ssrc\n\
      .rat.group.tree create poly 0 $ht $treeht $top $treeht $bottom -outline white -fill grey50 -tag To$ssrc\n\
      .rat.group.tree create poly 16 $ht $treeht $top $treeht $bottom -outline white -fill grey -tag From$ssrc\n\
\n\
	if {$LOSS_TO_ME($ssrc) < 5} {\n\
catch [.rat.group.tree itemconfigure To$ssrc -fill green]\n\
} elseif {$LOSS_TO_ME($ssrc) < 10} {\n\
catch [.rat.group.tree itemconfigure To$ssrc -fill orange]\n\
} elseif {$LOSS_TO_ME($ssrc) <=100} {\n\
catch [.rat.group.tree itemconfigure To$ssrc -fill red]\n\
} else {\n\
catch [.rat.group.tree itemconfigure To$ssrc -fill grey50]\n\
}\n\
\n\
if {$LOSS_FROM_ME($ssrc) < 10} {\n\
catch [.rat.group.tree itemconfigure From$ssrc -fill green]\n\
} elseif {$LOSS_FROM_ME($ssrc) < 20} {\n\
catch [.rat.group.tree itemconfigure From$ssrc -fill orange]\n\
} elseif {$LOSS_FROM_ME($ssrc) <= 100} {\n\
catch [.rat.group.tree itemconfigure From$ssrc -fill red]\n\
} else {\n\
catch [.rat.group.tree itemconfigure From$ssrc -fill grey]\n\
} \n\
}\n\
}\n\
proc ssrc_update_all {} {\n\
    global CNAME SSRC\n\
\n\
    set SSRC \"\"\n\
    foreach ssrc [array names CNAME] {\n\
	ssrc_update $ssrc\n\
    }\n\
}\n\
\n\
if {[glob ~] == \"/\"} {\n\
   set rtpfname /.RTPdefaults\n\
} else {\n\
   set rtpfname ~/.RTPdefaults\n\
}\n\
###############################################################################\n\
proc saving {} {\n\
global NAME EMAIL PHONE LOC\n\
puts \"Entered saving\"\n\
global rtpfname V win32 \n\
global myssrc\n\
    if {$win32} {\n\
\n\
	putregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpName\"  \"$NAME($myssrc)\"\n\
	putregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpEmail\"  \"$EMAIL($myssrc)\"\n\
	putregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpPhone\"  \"$PHONE($myssrc)\"\n\
	putregistry \"HKEY_CURRENT_USER\\\\Software\\\\$V(class)\\\\$V(app)\" \"*rtpLoc\"  \"$LOC($myssrc)\"\n\
    } else {\n\
	set f [open $rtpfname w]\n\
	if {$NAME($myssrc)  != \"\"} {puts $f \"*rtpName:  $NAME($myssrc)\"}\n\
	if {$EMAIL($myssrc) != \"\"} {puts $f \"*rtpEmail: $EMAIL($myssrc)\"}\n\
	if {$PHONE($myssrc) != \"\"} {puts $f \"*rtpPhone: $PHONE($myssrc)\"}\n\
	if {$LOC($myssrc)   != \"\"} {puts $f \"*rtpLoc:   $LOC($myssrc)\"}\n\
	close $f\n\
      }\n\
}\n\
";
