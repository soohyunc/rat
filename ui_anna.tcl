#
# Copyright (c) 1995,1996,1997 University College London
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

# Standard RAT colours, etc...
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

set attendance 0
set stop 0
set person ""
set toggled 0
set V(class) "MBone Applications"
set V(app)   "rat"
# The following function deal with receiving messages from the conference bus. The code
# in ui.c will call cb_recv with the appropriate arguments when a message is received. 
# Messages can be sent to RAT by calling 'cb_send "U" "message"'.
###############################################################################
proc cb_recv_output {cmd args} {
switch $cmd {
    gain {.rat.power.in.cin set $args
}
    device {.rat.power.in.inicon configure -bitmap $args
}
    mute {.rat.power.in.rec configure -relief sunken -text UnMute -bg seashell4 -fg white
}
unmute {.rat.power.in.rec configure -relief raised -text Mute -bg grey80 -fg black
}
}
}
##############################################################################
proc cb_recv_input {cmd args} {
global sending stop 
switch $cmd {
    gain {.rat.power.out.cout set $args}
    device {.rat.power.out.outicon configure -bitmap $args}
    mute {.rat.power.out.send configure -relief sunken -text UnMute -bg seashell4 -fg white
#    if {$sending !=1} {
#	.rat.power.transmit configure -text "Push to Talk"
#	cb_send "U" "silence 0"
#    }
}
    unmute {.rat.power.out.outicon configure -relief raised -text Mute -bg grey80 -fg black
#        if {$sending !=1} {
#            .rat.power.transmit configure -text "Push to Stop"
#            set stop 1
#            cb_send "U" "silence 1"
#        }
    }
}
}
##############################################################################
proc cb_recv_address {add port ttl} {
global mode_int
global title
global Dest Port TTL
set Dest $add
set Port $port
set TTL $ttl
set title "$add $port $ttl"
}
##############################################################################
proc set_gain gain {
cb_send "U" "input gain $gain"
}
proc set_vol volume {
cb_send "U" "output gain $volume"
}
###############################################################################
proc update_names {} {
	global myssrc
global NAME EMAIL PHONE LOC 
	cb_send "U" "ssrc $myssrc name $NAME($myssrc)"
	cb_send "U" "ssrc $myssrc email $EMAIL($myssrc)"
	cb_send "U" "ssrc $myssrc phone $PHONE($myssrc)"
	cb_send "U" "ssrc $myssrc loc $LOC($myssrc)" 
}
##############################################################################
proc cb_recv {cmd} {
  if [string match [info procs [lindex cb_recv_$cmd 0]] [lindex cb_recv_$cmd 0]] {
    eval cb_recv_$cmd 
  }
}
#############################################################################
proc cb_recv_powermeter {type level} {
	switch $type {
		output  {bargraphSetHeight .rat.power.in.cin    $level}
		input   {bargraphSetHeight .rat.power.out.cout  $level}
	}
}
##########################################################################
proc rate size {
cb_send "U" "rate $size"
}
##########################################################################
proc cb_recv_my_ssrc ssrc {
	global myssrc
	set myssrc $ssrc
}
###########################################################################
#Rat finished initialising, change defaults
proc cb_recv_init {} {
}

##############################################################################
proc cb_recv_ssrc {ssrc args} {
    global SSRC
    global CNAME NAME EMAIL LOC PHONE TOOL attendance 
    global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME
    set cmd [lindex $args 0]
    set arg [lrange $args 1 end]
    switch $cmd {
	cname {
	    # All RTCP packets have a CNAME field, so when a new user arrives, this is the first
	    # field to be filled in. We must therefore, check if this SSRC exists already, and
	    # if not, fill in dummy values for all the SDES fields.
	    set       CNAME($ssrc) $arg
	    set        NAME($ssrc) $arg
	    set       EMAIL($ssrc) " "
	    set       PHONE($ssrc) " "
	    set         LOC($ssrc) " "
	    set        TOOL($ssrc) " "
	    set    ENCODING($ssrc) "unknown"
	    set    DURATION($ssrc) " "
	    set  PCKTS_RECV($ssrc) "0"
	    set  PCKTS_LOST($ssrc) "0"
	    set  PCKTS_MISO($ssrc) "0"
	    set JITTER_DROP($ssrc) "0"
	    set      JITTER($ssrc) "0"
	    set   LOSS_TO_ME($ssrc) "101"
	    set LOSS_FROM_ME($ssrc) "101"
	    incr attendance
	    if [winfo exists .rat.received.attendance] {
		.rat.received.attendance configure -text "Attendance:$attendance"
	    }
	}
	name            { set NAME($ssrc) "$arg"}
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
	loss_to_me      { set LOSS_TO_ME($ssrc) $arg }
	loss_from_me    { set LOSS_FROM_ME($ssrc) $arg }
	active          {
	    if {[string compare $arg "now"] == 0} {
#Find the location in the listbox CORRECT?
                 set now [lsearch -exact $SSRC $ssrc]
                .rat.group.names selection set $now
                .rat.group.names configure -selectbackground white
                .rat.group.names selection clear $now

                #By deselecting cover prob of getting userloss stats
                
	    }
	    if {[string compare $arg "recent"] == 0} { 
                 set recent [lsearch -exact $SSRC $ssrc]
                .rat.group.names selection set $recent
                .rat.group.names configure -selectbackground gray90
                .rat.group.names selection clear $recent
	    }
	}
	inactive        {
            set inactive [lsearch -exact $SSRC $ssrc]
            .rat.group.names selection set $inactive
            .rat.group.names configure -selectbackground gray80
            .rat.group.names selection clear $inactive 
	}	
	default {
	    puts stdout "Unknown ssrc command 'ssrc $ssrc $cmd $arg'"
	}
    }
    ssrc_update $ssrc
}

###############################################################################
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
##############################################################################
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
#GLOBALS
set attendance 0
set globalhelp 1
set basic_access 0
#DEFAULT STARTING MODE IS MUTE
set mute_status_out 1
set mute_status_in 1
#READ INFO. ONLY ON FIRST WINDOW ACCESS
set access 0
#DEFAULT FOR PRIMARY+SECONDARY ENCODING
set pgp ""
set name DVI
set sname "Redundancy: "
set x lossin
#TO CONTROL BITMAPS DISPLAYED ON POWERMETERS FRAME
set toggle_in 2
set toggle_out 2
set stop 0
set sending 0
#CONTROLLING CANVAS
set mode_int 1
set scaleset 2
#CHANGE THIS VARIABLE TO DISPLAY LARGER/SMALLER 'LOSS-DIAMONDS'
set treeht 8.5
set raised 0
set some_coding DVI
global title

frame .start -width 8c -height 3c -borderwidth 1m -relief groove
pack .start -fill x
frame .purpose -width 8c -height 3c -borderwidth 1m -relief groove
pack .purpose -fill x
frame .encrypt -width 8c -height 3c -borderwidth 1m -relief groove
pack .encrypt -fill x

checkbutton .again -text "Show this screen next time" -variable again -anchor e
.again select
button .proceed -text "   Proceed   " -command MainBasic
bind .proceed <Button-1> set_defaults
pack .again .proceed -side top -expand 1 -fill x
button .quit -text "   Quit   " -command exit
pack .quit -side right -fill x
pack .proceed .quit -side left
pack .quit -side right
pack .again -side top

label .start.stmode -text "Start Mode :" -padx 2m -pady 2m -anchor w
radiobutton .start.basic -text Basic -variable mode -value basic -padx 2m -pady 2m -anchor w -command {selectmode $mode}
radiobutton .start.advanced -text Advanced -variable mode -value advanced -padx 2m -pady 2m -anchor w -command {selectmode $mode}
pack .start.stmode .start.basic .start.advanced -side left 
.start.basic select

label .purpose.plabel -text "Purpose :" -padx 2m -pady 2m 
tk_optionMenu .purpose.pmenu choice  Meeting "Receive lecture" "Give lecture" 
pack .purpose.plabel .purpose.pmenu -side left 

#Sets the default modes, primary encodings
proc set_defaults {} {
	global choice
	switch $choice {
		Meeting {cb_send "U" "primary DVI"
			cb_send "U" "lecture 1"
			cb_send "U" "output mode Full duplex"
			}
		"Receive lecture" {cb_send "U" "primary DVI"
			cb_send "U" "lecture 0"
			cb_send "U" "output mode Net mutes mike"
		}
		"Give lecture" {cb_send "U" "primary DVI"
			cb_send "U" "lecture 1"
			cb_send "U" "output mode Mike mutes net"
		}
	}
}
###############################################################################
proc cb_recv_redundancy {args} {
	#Set the redundancy variable to the chosen encoding value
	global some_coding sname
	if {$args != "NONE"} {
		set some_coding $args
		puts "Entered recv some_coding = $some_coding"
	}
	set sname "Redundancy: $args"
}
###############################################################################
checkbutton .encrypt.setencrypt -text on/off -variable setencrypt -command checkentered -anchor w 
button .encrypt.usekey -text Encryption -pady 2m
pack .encrypt.usekey .encrypt.setencrypt -side left 
wm title . "Start-up Window"


bind .encrypt.usekey <Button-1> {setKey}
##############################################################################
proc selectmode mode {
puts $mode
    if {$mode == "basic"} {
	.proceed configure -command MainBasic
    } else {
	.proceed configure -command MainAdvanced
    }
}
##############################################################################
proc setKey {} {
    toplevel .key
    global .encrypt.setencrypt
    label .key.keylabel -text "Key :" -padx 2m -font *-times-medium-r-normal--*-140-*
    entry .key.keyentry -width 50 -relief sunken -textvariable pgp
    button .key.accept -text "Accept" -command "destroy .key"
    button .key.cancel -text "Cancel" -command "destroy .key"
    bind .key.cancel <Button-1>  {.encrypt.setencrypt deselect}

    pack .key.keylabel .key.keyentry -side top
    pack .key.accept .key.cancel -padx 1m -pady 2m -side left
    pack .key.cancel -side right
    focus .key.keyentry
}
##############################################################################
#CANNOT SET ENCRYPTION IF KEY NOT ENTERED
proc checkentered {} {
    global pgp
    global setencrypt
    if {$setencrypt == 1} {
	if {$pgp == ""} {
	    .encrypt.setencrypt deselect
	    setKey
	} else {
          Install_key $pgp
	}
    }
}
###############################################################################
proc Install_key key {
cb_send "U" "update_key $key"
}

###################################################################
proc MainBasic {} {
    wm withdraw .
    global title
    global basic_access
    incr basic_access
    global choice
    global lossout
    toplevel .rat
    wm title .rat RAT:$choice:$title
    frame .rat.left
    pack .rat.left -side left
    frame .rat.right
    pack .rat.right -side right
    frame .rat.group -width 8c -height 4c -bd 1m -relief groove
    pack propagate .rat.group 0

    frame .rat.received -width 8c -height 1.5c -bd 1m -relief groove
    pack propagate .rat.received 0

    frame .rat.sent -width 8c -height 2.7c -bd 1m -relief groove
    pack propagate .rat.sent 0
    pack .rat.group .rat.received .rat.sent -in .rat.left -side top 
    frame .rat.power -width 32m -height 8.2c -bd 1m -relief groove
    pack propagate .rat.power 0
    pack .rat.power -in .rat.right

    #ADD TO RECEIVED PANEL
    checkbutton .rat.received.adjr -text "Adjust audio received    " -variable adjr -command {repair $adjr}
    label .rat.received.attendance -text "Attendance :" 
    pack .rat.received.adjr .rat.received.attendance -side left 

    #ADD TO SENT PANEL
    checkbutton .rat.sent.adjs -text "Adjust audio sent   " -variable adjs -command {selectcolour $adjs}
    pack .rat.sent.adjs -side left -anchor w
    pack .rat.sent.adjs -side top
    button .rat.sent.bandwidth -text "Bandwidth"
    pack .rat.sent.bandwidth -side left
    global name
    bind .rat.sent.bandwidth <Button-1> {showCodecs name $scaleset} 
###############################################################################
proc repair adjr {
    if {$adjr == 0} {
    cb_send "U" "repair None"
    } else {
      cb_send "U" "repair Packet Repetition"
    }
}
###############################################################################
    proc codechelp {} {
	toplevel .ch
	wm geometry .ch +450+200
	wm title .ch Bandwidth
	message .ch.bandhelp -width 4c -justify left -relief raised -bd 2 -font\
		-Adobe-Helvetica-Medium-R-Normal--*-140-* -text "Bandwidth.\nThe bandwidth \
		encoding defaults to DVI. Should audio quality be poor, the bandwidth can be lowered\
		by clicking on this button"
	pack .ch.bandhelp
    }
    #################################################################
    #ADD COMPONENTS TO 'POWERMETERS' PANEL
    button .rat.power.transmit -text "Push to Talk"
global sending
bind .rat.power.transmit <ButtonPress-1> "set sending 1 ; checkmute2"
bind .rat.power.transmit <ButtonRelease-1> {set sending 0 ;cb_send "U" "toggle_send"}
    pack .rat.power.transmit -fill x -side top -ipady 5m
    button .rat.power.logo -bitmap "ucl"
    pack .rat.power.logo -fill x -side bottom


    #TO HOLD THE POWERMETERS

    frame .rat.power.in -height 5c -width 15m 
    pack propagate .rat.power.in 0
    frame .rat.power.out -height 5c -width 15m 
    pack propagate .rat.power.out 0
    pack .rat.power.in .rat.power.out -side left
    pack .rat.power.in -anchor n
    pack .rat.power.out -anchor n

    button .rat.power.in.inicon -command toggle_in -bitmap "head"
    button .rat.power.out.outicon -command toggle_out -bitmap "mic"
    pack .rat.power.in.inicon -expand 1 -fill x -anchor n
    pack .rat.power.out.outicon -expand 1 -fill x -anchor n
    ##########################################################################
    proc toggle_in {} {
    cb_send "U" "toggle_output_port"
    
}
############################################################################
proc toggle_out {} {
cb_send "U" "toggle_input_port"
}

#############################################################################
button .rat.power.in.rec -text Mute -relief raised -command checkmute1 

button .rat.power.out.send -text UnMute -command checkmute2 -relief sunken -bg seashell4 -fg white
pack .rat.power.in.rec -side top
pack .rat.power.out.send -side top
###################################################################
proc checkmute1 {} {
    cb_send "U" "toggle_play"
    if {[string compare [.rat.power.in.rec cget -relief] raised] == 0} {
	.rat.power.in.rec configure -relief sunken -text UnMute -bg seashell4 -fg white
	

    } else {
	.rat.power.in.rec configure -relief raised -text Mute -bg gray80 -fg black
	
}
}
##################################################################
proc checkmute2 {} { 
global sending
global raised
global stop
cb_send "U" "toggle_send"
    if {[string compare [.rat.power.out.send cget -relief] raised] == 0} {
        .rat.power.out.send configure -relief sunken -text UnMute -bg seashell4 -fg white

	    if {$sending !=1} {
.rat.power.transmit configure -text "Push to Talk"
set raised 0
}

    } else {
        .rat.power.out.send configure -relief raised -text Mute -bg grey80 -fg black
	if {$sending !=1} {
.rat.power.transmit configure -text "Push to Stop"
set stop 1
set raised 1
}
}   

}
##################################################################


#############################################################################
#ADD THE CANVAS' AND SCALE
scale .rat.power.in.sin -from 100 -to 0 -length 3c -width 1.8m -orient vertical  -command set_vol
.rat.power.in.sin set 0
pack .rat.power.in.sin -side bottom


bargraphCreate .rat.power.in.cin

pack .rat.power.in.cin .rat.power.in.sin -side left
pack .rat.power.in.cin -side bottom
pack .rat.power.in.sin -side right
pack .rat.power.in.cin .rat.power.in.sin -anchor s

bargraphCreate .rat.power.out.cout
pack .rat.power.out.cout -side bottom
pack .rat.power.out.cout -side left
pack .rat.power.out.cout -anchor s

scale .rat.power.out.sout -from 100 -to 0 -length 3c -width 1.8m -orient vertical -command set_gain
.rat.power.out.sout set 0
pack .rat.power.out.sout -side bottom
##################################################################
proc agchelp {} {
    toplevel .agch
    wm geometry .agch +450+400
    wm title .agch "AGC"
    message .agch.agchelp -width 4c -justify left -relief raised -bd 2\
	    -font -Adobe-Helvetica-Medium-R-Normal--*-140-* -text "Automatic Gain Control.\n\Buffers incoming sound to volume as set by this scale"
    pack .agch.agchelp
}
###################################################################


#ADD TO RECEIVED PANEL
global mode_int
if {$mode_int == 2} {

    listbox .rat.group.names -yscrollcommand [list .rat.group.namescroll set] -width 23
    scrollbar .rat.group.namescroll -command [list BindYview [list .rat.group.names .rat.group.tree]]
    canvas .rat.group.tree -bg black -width 0.7c -height 4c -yscrollcommand [list .rat.group.namescroll set]
    pack .rat.group.namescroll -side left -fill y
    pack .rat.group.tree -side left
    pack .rat.group.names -side left 
} else {
    listbox .rat.group.names -yscrollcommand ".rat.group.namescroll set" -width 27
    scrollbar .rat.group.namescroll -command ".rat.group.names yview"
    pack .rat.group.namescroll -side left -fill y
    pack .rat.group.names -side left -fill x
}
##############################################################################
proc BindYview { lists args } {
foreach l $lists {
    eval {$l yview} $args
}
}
##############################################################################
button .rat.group.userdetails -text User\ndetails -command userwindow
menubutton .rat.group.useroptions -text options -relief raised -menu .rat.group.useroptions.menu

pack .rat.group.userdetails -side right -anchor n
pack .rat.group.userdetails .rat.group.useroptions -side top -anchor w
menu .rat.group.useroptions.menu
global globalhelp
.rat.group.useroptions.menu add checkbutton -label "Show Startup Window" -variable again
.rat.group.useroptions.menu add checkbutton -label "Show Help" -variable showhelp -command {togglehelp $showhelp}

.rat.group.useroptions.menu add separator

proc warn {} {
toplevel .warn 
wm geometry .warn +400+300
wm title .warn "Invalid Click"
message .warn.whelp -width 4c -justify left -relief raised -bd 2 -font -Adobe-Helvetica-Medium-R-Normal--*-140-* -text "Push to Stop Mode.\nYou cannot display audio loss statistics on yourself in this mode."
pack .warn.whelp
button .warn.ok -text ok -command "destroy .warn"
pack .warn.ok -side bottom -anchor e
}
#Cannot bring up stats on oneself due to listbox selection problem
#Cannot get the selection when push to stop is running
#Toggle back to bring up warning window
#Not enough time to toggle? Therefore works only 50% of time
bind .rat.group.names <Double-1> {if {$stop == 1} {
puts "Toggle send from binding"
cb_send "U" "toggle_send"
global NAME CNAME myssrc
  if {[selection get] != $NAME($myssrc) | [selection get] != $CNAME($myssrc)} {
puts [selection get]
showloss [selection get] [lindex $SSRC [.rat.group.names curselection]]
  } else {
warn
return
    }

} else {
    showloss [selection get] [lindex $SSRC [.rat.group.names curselection]]
}
}


#ALLOW THE USER TO CHANGE PURPOSE FROM MAIN WINDOW
.rat.group.useroptions.menu add cascade -label Purpose -menu .rat.group.useroptions.menu.purposechange
menu .rat.group.useroptions.menu.purposechange
.rat.group.useroptions.menu add separator
global choice 
global purpose
.rat.group.useroptions.menu.purposechange add radiobutton -label Meeting -variable choice -value Meeting -command changep
.rat.group.useroptions.menu.purposechange add radiobutton -label "Receive lecture" -variable choice -value "Receive lecture" -command changep
.rat.group.useroptions.menu.purposechange add radiobutton -label "Give lecture" -variable choice -value "Give lecture" -command changep
.rat.group.useroptions.menu add command -label "Show Advanced" -command MainAdvanced
###################################################################
proc changep {} {
    global title
    global choice
    wm title .rat RAT:$choice:$title
switch $choice {
Meeting {cb_send "U" "primary DVI"
cb_send "U" "lecture 1"
}
"Receive lecture" {cb_send "U" "primary DVI"
cb_send "U" "lecture 0"
}
"Give lecture" {cb_send "U" "primary DVI"
cb_send "U" "lecture 1"
}
}
}
###################################################################

#############################################################################
#Not possible to put the flash command in because of interrupt dynamic 
#message passing on conference bus
proc update_loss {ssrc} {
	global adjs SSRC
	global LOSS_FROM_ME 
	global red 
	global orange
        global attendance av_loss
    set count 0
    set av_loss 0
    for {set i 0} {$i < $attendance} {incr i} {
       if [winfo exists .rat.group.names] {
        set rec_loss [lindex $SSRC $i]
	   if {$rec_loss == ""} {
                break
	   }
        
        incr count $LOSS_FROM_ME($rec_loss)
        incr i
       }	
}
      
        set av_loss [expr $count / $attendance]
       
###############################################################################
	if {$av_loss >=20} {
    		if {$adjs !=1} {
			.rat.sent.adjs configure -bg red
        		set red 1
        		set orange 0 
    		} else {
			if {$av_loss >10} {
           			if  {$adjs !=1} {
           				set orange 1
           				set red 0
            				.rat.sent.adjs configure -bg orange
       				}   
			} else {
          			.rat.sent.adjs configure -bg grey
           			set red 0
           			set orange 0
			}
    		}
	}
}
##############################################################################
#If redundancy is selected no red configuration
proc selectcolour {adjs} {
global mode_int
global sname
global red
global orange
global some_coding
if {$mode_int == 2} {
    if {$adjs == 1} {
         cb_send "U" "redundancy $some_coding"
	.rat.sent.adjs configure -bg grey
	# Because of the .rat.sent.adjs configure -textvariable sname, any
	# change we make to sname will automagically be updated on the screen! :-(
	set sname "Redundancy: $some_coding"
    } else {
            puts "Entered off statements sname org= $sname"
            set sname "Redundancy: $some_coding"
            cb_send "U" "redundancy NONE"
	if {$red == 1} {
           .rat.sent.adjs configure -bg red
	}
        if {$orange == 1} {
           .rat.sent.adjs configure -bg orange
	}
           
            
    }
###############BASIC######################################################    
} else {
    if {$adjs == 1} {
     .rat.sent.adjs configure -bg grey
     cb_send "U" "redundancy DVI"
    } else {
        cb_send "U" "redundancy NONE"
	if {$red == 1} {
           .rat.sent.adjs configure -bg red
	}
        if {$orange == 1} {
           .rat.sent.adjs configure -bg orange
	}
    }
}
}
###############################################################################
proc nameshelp {} {
    toplevel .nh
    wm geometry .nh +450+300
    wm title .nh Participants
    message .nh.nhhelp -width 4c -justify left -relief raised -bd 2 -font -Adobe-Helvetica-Medium-R-Normal--*-140-* -text "Participants.\n\Single click name to mute.\nDouble click name to display information and \
	    audio loss statistics for that participant."
    pack .nh.nhhelp
}
##############################################################################
proc showloss {name ssrc} {
puts "Entered Showloss"
global stop
if {$stop == 1} {
cb_send "U" "toggle_send"
}
global SSRC
set select [lsearch -exact $SSRC $ssrc]
    .rat.group.names selection clear $select
    global EMAIL PHONE LOC TOOL ENCODING CNAME DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME NAME
    global mode_int
    if {$mode_int == 2} {
	if [winfo exists .advsl$ssrc] {
	    updateshowloss $ssrc
	} else {
	    toplevel .advsl$ssrc
	    wm title .advsl$ssrc $name
	     
	    label .advsl$ssrc.name -text "Name:   $NAME($ssrc)" 
	    label .advsl$ssrc.email -text "Email:   $EMAIL($ssrc)" 
	    label .advsl$ssrc.phone -text "Phone:   $PHONE($ssrc)" 
	    label .advsl$ssrc.location -text "Location:  $LOC($ssrc)" 
	    label .advsl$ssrc.tool -text "Tool:   $TOOL($ssrc)" 
	    label .advsl$ssrc.cname -text "CNAME:  $CNAME($ssrc)" 
	    label .advsl$ssrc.enc -text "Audio Encoding: $ENCODING($ssrc)" 
	    label .advsl$ssrc.length -text "Audio Length: $DURATION($ssrc)" 
	    label .advsl$ssrc.rec -text "Packets Received: $PCKTS_RECV($ssrc)" 
	    label .advsl$ssrc.lost -text "Packets Lost: $PCKTS_LOST($ssrc)" 
	    label .advsl$ssrc.mis -text "Packets Misordered: $PCKTS_MISO($ssrc)" 
	    label .advsl$ssrc.drop -text "Units Dropped: $JITTER_DROP($ssrc)" 
	    label .advsl$ssrc.jitter -text "Jitter: $JITTER($ssrc)" 
	    label .advsl$ssrc.inst_loss -text "Instantaneous Loss Rate: $LOSS_TO_ME($ssrc)" 
	    button .advsl$ssrc.ok -text ok -command "destroy .advsl$ssrc"

	    pack .advsl$ssrc.name .advsl$ssrc.email .advsl$ssrc.phone .advsl$ssrc.location .advsl$ssrc.tool .advsl$ssrc.cname .advsl$ssrc.enc .advsl$ssrc.length .advsl$ssrc.rec .advsl$ssrc.lost .advsl$ssrc.lost .advsl$ssrc.mis .advsl$ssrc.drop .advsl$ssrc.jitter .advsl$ssrc.inst_loss .advsl$ssrc.ok -side top -anchor w
	    pack .advsl$ssrc.ok -side right
	}
    } else {
	if [winfo exists .sl$ssrc] {
	    updateshowloss $ssrc
	} else {
	    toplevel .sl$ssrc
	    wm title .sl$ssrc $name
	    frame .sl$ssrc.left
	    frame .sl$ssrc.right 
	    pack .sl$ssrc.left -side left
	    pack .sl$ssrc.right -side right
	    label .sl$ssrc.left.quality -text "Audio Quality From: " 
	    label .sl$ssrc.right.qualityl -width 1 -height 1 
	    label .sl$ssrc.left.pname -text Name: 
	    label .sl$ssrc.left.pemail -text Email:
	    label .sl$ssrc.right.realname -text $NAME($ssrc)
	    label .sl$ssrc.right.realmail -text "$EMAIL($ssrc)"
	    label .sl$ssrc.left.lossrate -text "Loss Rate:"

	    if { [.rat.group.names curselection] == 0 } {
		set x $LOSS_FROM_ME($ssrc)
		label .sl$ssrc.right.lossstat -text $x
		.sl$ssrc.left.quality configure -text "Avr. Audio Quality From: "
	    } else {
		set x $LOSS_TO_ME($ssrc)
		label .sl$ssrc.right.lossstat -text $x
	    }

	    button .sl$ssrc.right.ok -text ok -width 1 -height 1 -command "destroy .sl$ssrc"
         

	    if {$x <= 10} {
		.sl$ssrc.right.qualityl configure -bg green
	    } else {
		if {$x <=20} {
		    .sl$ssrc.right.qualityl configure -bg orange
		} else {
		    .sl$ssrc.right.qualityl configure -bg red
	    }   }
	    label .sl$ssrc.right.dummy
	    pack .sl$ssrc.left.quality .sl$ssrc.left.pname .sl$ssrc.left.pemail  .sl$ssrc.left.lossrate -side top  
	    pack .sl$ssrc.left.quality -pady 1
	    pack .sl$ssrc.right.qualityl .sl$ssrc.right.realname .sl$ssrc.right.realmail .sl$ssrc.right.dummy .sl$ssrc.right.lossstat -side top -anchor s -padx 10
	    pack .sl$ssrc.right.ok -side bottom -anchor e

	    #ADJUST FOR BUTTON SPACE 
	    pack .sl$ssrc.left.lossrate -pady 25
	    pack .sl$ssrc.right.realmail -pady 8
	    pack .sl$ssrc.left.pemail -pady 7

	}
    }
}

##############################################################################
proc updateshowloss {ssrc} { 
    global EMAIL PHONE LOC TOOL ENCODING CNAME DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME NAME
    global mode_int av_loss
    if {$mode_int == 2} {
	
	.advsl$ssrc.name configure -text "Name:   $NAME($ssrc)" 
	.advsl$ssrc.email configure -text "Email:   $EMAIL($ssrc)" 
	.advsl$ssrc.phone configure -text "Phone:   $PHONE($ssrc)" 
	.advsl$ssrc.location configure -text "Location:  $LOC($ssrc)" 
	.advsl$ssrc.tool configure -text "Tool:   $TOOL($ssrc)" 
	.advsl$ssrc.cname configure -text "CNAME:  $CNAME($ssrc)" 
	.advsl$ssrc.enc configure -text "Audio Encoding: $ENCODING($ssrc)" 
	.advsl$ssrc.length configure -text "Audio Length: $DURATION($ssrc)" 
	.advsl$ssrc.rec configure -text "Packets Received: $PCKTS_RECV($ssrc)"
	.advsl$ssrc.lost configure -text "Packets Lost: $PCKTS_LOST($ssrc)" 
	.advsl$ssrc.mis configure -text "Packets Misordered: $PCKTS_MISO($ssrc)" 
	.advsl$ssrc.drop configure -text "Units Dropped: $JITTER_DROP($ssrc)" 
	.advsl$ssrc.jitter configure -text "Jitter: $JITTER($ssrc)" 
	.advsl$ssrc.inst_loss configure -text "Instantaneous Loss Rate: $LOSS_TO_ME($ssrc)" 

	#ELSE CONFIGURE BASIC INFO.
    } else {
	.sl$ssrc.right.realmail configure -text "$EMAIL($ssrc)"
	.sl$ssrc.left.lossrate configure -text "Loss Rate: $LOSS_TO_ME($ssrc)"

	if { [.rat.group.names curselection] == 0 } {
	    set x $av_loss
	    .sl$ssrc.left.quality configure -text "Avr. Audio Quality From: "
	} else {
	    set x $LOSS_TO_ME($ssrc)
	    .sl$ssrc.right.lossstat configure -text $x
	}

	if {$x <= 10} {
	    .sl$ssrc.right.qualityl configure -bg green
	} else {
	    if {$x <=20} {
		.sl$ssrc.right.qualityl configure -bg orange
	    } else {
		.sl$ssrc.right.qualityl configure -bg red
	}   }
    }

}

    #Add 'Top-level' buttons
    button .rat.right.mainquit -text "   Quit   " -command exit
    bind .rat.right.mainquit <Button-1> exit
    pack .rat.right.mainquit -side bottom
    button .rat.left.about -text "   About   " -command showabout
    pack .rat.left.about -side bottom -expand 1 -fill x
    pack .rat.left.about -side left
    pack .rat.right.mainquit -side right -expand 1 -fill both



    # This is the end of the MainBasic procedure. Last thing we do, now we've
    # created the window, is update the list of participants...
    ssrc_update_all
    read_details


}

#Command for reading in from the file########################################
#Called when myssrc has been received
proc read_details {} {
global NAME EMAIL PHONE LOC 
global myssrc win32 rtpfname
puts "Email is $EMAIL($myssrc)"
puts "myssrc is $myssrc"
set $NAME($myssrc)  [option get . rtpName  rat]
set $EMAIL($myssrc) [option get . rtpEmail rat]
set $PHONE($myssrc) [option get . rtpPhone rat]
set $LOC($myssrc)   [option get . rtpLoc   rat]

if {$win32 == 0} {
    if {$NAME($myssrc) == "" && [file readable $rtpfname] == 1} {
	set f [open $rtpfname]
	while {[eof $f] == 0} {
	    gets $f line
	    if {[string compare "*rtpName:"  [lindex $line 0]] == 0} {set $NAME($myssrc)  [lrange $line 1 end]}
	    if {[string compare "*rtpEmail:" [lindex $line 0]] == 0} {set $EMAIL($myssrc) [lrange $line 1 end]}
	    if {[string compare "*rtpPhone:" [lindex $line 0]] == 0} {set $PHONE($myssrc) [lrange $line 1 end]}
	    if {[string compare "*rtpLoc:"   [lindex $line 0]] == 0} {set $LOC($myssrc)   [lrange $line 1 end]}
	}
	close $f
    }
} else {
    if {$NAME($myssrc) == ""} {
	catch {set $NAME($myssrc)  [getregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpName"]  } 
	catch {set $EMAIL($myssrc) [getregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpEmail"] } 
	catch {set $PHONE($myssrc) [getregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpPhone"] } 
	catch {set $LOC($myssrc)   [getregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpLoc"  ] } 
    }
}

if {$NAME($myssrc) == ""} {
    toplevel .name
    wm title .name "RAT User Information"
    message .name.m -width 1000 -text {
	Please enter the following details, for transmission
	to other conference participants.
    }
	frame  .name.b
    label  .name.b.res -text "Name:"
    entry  .name.b.e -highlightthickness 0 -width 20 -relief sunken -textvariable NAME($myssrc)
    button .name.d -highlightthickness 0 -padx 0 -pady 0 -text Done -command {NAME($myssrc $NAME($myssrc); saving; destroy .name}
    bind   .name.b.e <Return> {NAME($myssrc) $NAME($myssrc); saving; destroy .name}
    
    pack .name.m -side top -fill x -expand 1
    pack .name.b -side top -fill x -expand 1
    pack .name.b.res -side left
    pack .name.b.e -side right -fill x -expand 1
    pack .name.d -side bottom -fill x -expand 1
    wm resizable .name 0 0
    update
    raise .name .
}
}




#HANDLE BANDWIDTH CLICK########################################################
proc showCodecs {name scaleset} {
    frame .rat.sent.packetframe -width 5c -height 2c
    pack .rat.sent.packetframe -side right
    scale .rat.sent.packetframe.packetscale -from 0 -to 4 -length 3c -orient horizontal -command changec
    .rat.sent.packetframe.packetscale set $scaleset
    label .rat.sent.packetframe.packetlabel -textvariable $name

    button .rat.sent.packetframe.packetok -text ok -width 1 -height 1 -command "destroy .rat.sent.packetframe"
    bind .rat.sent.packetframe.packetok <Button-1> {cb_send "U" "primary $name"}


    pack .rat.sent.packetframe.packetlabel .rat.sent.packetframe.packetscale .rat.sent.packetframe.packetok -side left
    pack .rat.sent.packetframe.packetscale .rat.sent.packetframe.packetok -side top
}
###############################################################################
proc changec value {
    global name
    global scaleset
    set psize [.rat.sent.packetframe.packetscale get]
    switch $psize {
	0 {set name LPC
	set scaleset 0
    }
    1 {set name GSM
    set scaleset 1
}
2 {set name DVI
set scaleset 2
}
3 {set name PCM
set scaleset 3
}
4 {set name "16-bit linear"
set scaleset 4
}
}
}

###############################################################################



#ADJUST AUTOMATIC GAIN CONTROL
proc agc {} {
}

#COPYRIGHT INFO
proc showabout {} {
    toplevel .about
    wm title .about "About RAT"
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
button .about.c -highlightthickness 0 -padx 0 -pady 0 -text Copyright -command copyright
button .about.d -highlightthickness 0 -padx 0 -pady 0 -text Quit   -command "destroy .about"
pack .about.a .about.b .about.c .about.d -side top -fill x
pack .about.a.b -side left -fill y
pack .about.a.m -side right -fill both -expand 1
wm resizable .about 0 0
}
############################################################################
proc copyright {} {
toplevel .copyright
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
button .copyright.d -highlightthickness 0 -padx 0 -pady 0 -text Dismiss   -command "destroy .copyright"
pack   .copyright.m .copyright.d -side top -fill x -expand 1
wm title     .copyright "RAT Copyright"
wm resizable .copyright 0 0
}

#SHOW USER INFO################################################################
proc userwindow {} {
global CNAME NAME EMAIL LOC PHONE TOOL SSRC
global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME 
global myssrc

    toplevel .uw
    wm title .uw "User Details"
    frame .uw.left
    frame .uw.right
    pack .uw.left -side left
    pack .uw.right -side right
    label .uw.left.name -text Name:
    label .uw.left.email -text Email:
    label .uw.left.phone -text Phone:
    label .uw.left.location -text Location:
    entry .uw.right.namee -relief sunken -bd 1 -textvariable NAME($myssrc) -width 20
#Need to change where the insertion curser is?
    entry .uw.right.emaile -relief sunken -bd 1 -textvariable EMAIL($myssrc) -width 20
    entry .uw.right.phonee -relief sunken -bd 1 -textvariable PHONE($myssrc) -width 20
    entry .uw.right.locatione -relief sunken -bd 1 -textvariable LOC($myssrc) -width 20

    pack .uw.right.namee .uw.right.emaile .uw.right.phonee .uw.right.locatione -side top -pady 2m
    focus .uw.right.namee
    button .uw.right.ok -text "OK" -height 1 -command "update_names; saving; destroy .uw"



    pack .uw.right.ok -anchor se
    pack .uw.left.name .uw.left.email .uw.left.phone .uw.left.location -side top -anchor w -pady 5

    #ADJUST FOR BUTTON SPACE
    pack .uw.left.location -pady 20
    pack .uw.left.email -pady 12
    pack .uw.left.name -pady 1

    bind .uw.right.ok <Button-1> {
	.rat.group.names delete 0
	.rat.group.names insert 0 $NAME($myssrc)
    }
}

###############################################################################
proc togglehelp showhelp {
    global globalhelp
    #eval showhelp
    if {$showhelp == 1} {
	bind .rat.sent.bandwidth <Enter> {codechelp}
	bind .rat.sent.bandwidth <Leave> "destroy .ch"
	bind .rat.power.in.sin <Enter> {agchelp}
	bind .rat.power.in.sin <Leave> "destroy .agch"
	bind .rat.group.names <Enter> {nameshelp}
	bind .rat.group.names <Leave> "destroy .nh"
    } else {
	bind .rat.sent.bandwidth <Enter> ""
	bind .rat.sent.bandwidth <Leave> ""
	bind .rat.power.in.sin <Enter> ""
	bind .rat.power.in.sin <Leave> ""
	bind .rat.group.names <Enter> ""
	bind .rat.group.names <Leave> ""
    }
}

##############################################################################
#CREATE THE ADVANCED RAT
proc MainAdvanced {} {
global SSRC
global raised


    global mode_int
    set mode_int 2
    global basic_access
    if {$basic_access == 1} {
	destroy .rat
        foreach i $SSRC {
          if [winfo exists .sl$i ] {
             destroy .sl$i
          }
        }
    }
    {MainBasic}
    if {$raised == 1} {
      .rat.power.transmit configure -text "Push to Stop"
     
    }
    #Add to the advanced RAT
    global globalhelp
    global name
    global attendance
    .rat.received.attendance configure -text Attendance:$attendance
    global scaleset
    global sname
    global Dest Port TTL
    .rat.received.adjr configure -text "Packet Repetition"
    .rat.sent.adjs configure -textvariable sname
    .rat.group.useroptions.menu delete end
    destroy .rat.sent.bandwidth
    button .rat.sent.bandwidth

    pack .rat.received.adjr -anchor n
    pack .rat.received.attendance -anchor n

    label .rat.received.dlabel -text Duration
    tk_optionMenu .rat.received.dmenu size 20ms 40ms 80ms 160ms
   # bind .rat.received.dmenu <Button-1> "getrate"
    bind .rat.received.dmenu <ButtonRelease> "getrate"
    proc getrate {} {
    set size [.rat.received.dmenu cget -text]
    rate $size
    }

    pack .rat.received.adjr .rat.received.dlabel .rat.received.dmenu -side top -anchor w
    pack .rat.received.attendance -side right
    pack .rat.received.dlabel .rat.received.dmenu -side left -anchor w



    proc draw_enc_frame {} {
	frame .rat.sent.encoding -width 7.5c -height 1.5c -bd 1m -relief groove

	pack propagate .rat.sent.encoding 0
	pack .rat.sent.adjs .rat.sent.encoding .rat.sent.rtp -side top
	pack .rat.sent.rtp -side bottom

	button .rat.sent.encoding.encoding -text Encoding -command advanced_enc
	pack .rat.sent.encoding.encoding -side left

    }
    label .rat.sent.rtp -text "  Dest: $Dest     Port: $Port     TTL: $TTL" 
    pack .rat.sent.adjs .rat.sent.rtp -side top -anchor w
    pack .rat.sent.rtp -side left -anchor s
    draw_enc_frame






    proc advanced_enc {} {
	global name sname scaleset
	#REDRAW THE FRAME FOR SCALES
	destroy .rat.sent.encoding
	frame .rat.sent.encoding -width 7.5c -height 1.5c -bd 1m -relief groove
	pack propagate .rat.sent.encoding 0

	pack .rat.sent.adjs .rat.sent.encoding .rat.sent.rtp -side top
	pack .rat.sent.rtp -side bottom

	scale .rat.sent.encoding.pscale -from 0 -to 4 -length 3c -orient horizontal -command changeadvc -showvalue 0
	.rat.sent.encoding.pscale set $scaleset
	label .rat.sent.encoding.plabel -textvariable name -width 15
	button .rat.sent.encoding.pbutton -text ok -width 1 -height 1 -command "destroy .rat.sent.encoding ; draw_enc_frame"
	pack .rat.sent.encoding.plabel .rat.sent.encoding.pscale .rat.sent.encoding.pbutton -side left
	pack .rat.sent.encoding.pbutton -side right


	bind .rat.sent.encoding.pbutton <Button-1> {
        cb_send "U" "redundancy $short"
	#Rat will automatically reconfigure the primary to the appropriate bdwdth
        global prim
        cb_send "U" "primary $prim"       
}



}

proc changeadvc value {
    global short
    global prim
    set short DVI
    global name
    global scaleset
    global sname
    set psize [.rat.sent.encoding.pscale get]
    switch $psize {
	0 {set name Primary:LPC
        set prim LPC
	set sname Redundancy:LPC
        set short LPC
	set scaleset 0
    }
    1 {set name Primary:GSM
    set prim GSM
    set sname Redundancy:GSM
    set short GSM
    set scaleset 1
}
2 {set name Primary:DVI
set prim DVI
set scaleset 2
set sname Redundancy:DVI
set short DVI
}
3 {set name Primary:PCM
set prim PCM
set sname Redundancy:DVI
set scaleset 3
set short DVI
}
4 {set name Primary:16-bit\nlinear
set prim 16-bitlinear
set scaleset 4
set sname Redundancy:DVI
set short DVI
}
}
}


}

proc ssrc_update {ssrc} {
    global mode_int
    global CNAME NAME EMAIL LOC PHONE TOOL SSRC
    global ENCODING DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO JITTER_DROP JITTER LOSS_TO_ME LOSS_FROM_ME

    # Only try to update the list if the window has been created. This
    # prevents problems occuring whilst the initial window is active.
    if [winfo exists .rat.group.names] {
	set pos [lsearch -exact $SSRC $ssrc]
	if {$pos == -1} {
	    # This is a new name, so add to the end of the list...
	    .rat.group.names insert end $NAME($ssrc)
            set pos [expr [.rat.group.names index end] - 1]
	    lappend SSRC $ssrc
	} else {
	    # It's someone we've seen before, who's changed their name...
	    .rat.group.names delete $pos
	    .rat.group.names insert $pos $NAME($ssrc)
	}
    }
    #UPDATE THE PARTICIPANT STATS
   
    if {$mode_int ==2} {
	if [winfo exists .advsl$ssrc] {
	    updateshowloss $ssrc
	} else {
	    if [winfo exists .sl$ssrc] {
		updateshowloss $ssrc
	    }
	}



    }
    update_loss $ssrc

    if {[winfo exists .rat.group.tree]} {
       global treeht
       set i [expr $pos + 1]
#Set the appropriate position for drawing the polygon
       set ht [expr $treeht * [expr $i + $pos]]
       set top [expr $ht - $treeht]
       set bottom [expr $ht + $treeht]
#Create the polygon to hold the loss data, identified by ssrc
      .rat.group.tree create poly 0 $ht $treeht $top $treeht $bottom -outline white -fill grey50 -tag To$ssrc
      .rat.group.tree create poly 16 $ht $treeht $top $treeht $bottom -outline white -fill grey -tag From$ssrc

	if {$LOSS_TO_ME($ssrc) < 5} {
catch [.rat.group.tree itemconfigure To$ssrc -fill green]
} elseif {$LOSS_TO_ME($ssrc) < 10} {
catch [.rat.group.tree itemconfigure To$ssrc -fill orange]
} elseif {$LOSS_TO_ME($ssrc) <=100} {
catch [.rat.group.tree itemconfigure To$ssrc -fill red]
} else {
catch [.rat.group.tree itemconfigure To$ssrc -fill grey50]
}

if {$LOSS_FROM_ME($ssrc) < 10} {
catch [.rat.group.tree itemconfigure From$ssrc -fill green]
} elseif {$LOSS_FROM_ME($ssrc) < 20} {
catch [.rat.group.tree itemconfigure From$ssrc -fill orange]
} elseif {$LOSS_FROM_ME($ssrc) <= 100} {
catch [.rat.group.tree itemconfigure From$ssrc -fill red]
} else {
catch [.rat.group.tree itemconfigure From$ssrc -fill grey]
} 
}
}
proc ssrc_update_all {} {
    global CNAME SSRC

    set SSRC ""
    foreach ssrc [array names CNAME] {
	ssrc_update $ssrc
    }
}

if {[glob ~] == "/"} {
   set rtpfname /.RTPdefaults
} else {
   set rtpfname ~/.RTPdefaults
}
###############################################################################
proc saving {} {
global NAME EMAIL PHONE LOC
puts "Entered saving"
global rtpfname V win32 
global myssrc
    if {$win32} {

	putregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpName"  "$NAME($myssrc)"
	putregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpEmail"  "$EMAIL($myssrc)"
	putregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpPhone"  "$PHONE($myssrc)"
	putregistry "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*rtpLoc"  "$LOC($myssrc)"
    } else {
	set f [open $rtpfname w]
	if {$NAME($myssrc)  != ""} {puts $f "*rtpName:  $NAME($myssrc)"}
	if {$EMAIL($myssrc) != ""} {puts $f "*rtpEmail: $EMAIL($myssrc)"}
	if {$PHONE($myssrc) != ""} {puts $f "*rtpPhone: $PHONE($myssrc)"}
	if {$LOC($myssrc)   != ""} {puts $f "*rtpLoc:   $LOC($myssrc)"}
	close $f
      }
}
