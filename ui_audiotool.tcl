#
# Copyright (c) 1995-98 University College London
# All rights reserved.
#
# $Revision$
# 
# Full terms and conditions of the copyright appear below.
#

wm withdraw .

if {[string compare [info commands registry] "registry"] == 0} {
	set win32 1
} else {
	set win32 0
	option add *Menu*selectColor 		forestgreen
	option add *Radiobutton*selectColor 	forestgreen
	option add *Checkbutton*selectColor 	forestgreen
	option add *Entry.background 		gray70
}

set statsfont     {helvetica 10}
set titlefont     {helvetica 10}
set infofont      {helvetica 10}
set smallfont     {helvetica  8}
set verysmallfont {helvetica  8}

option add *Entry.relief	sunken 
option add *borderWidth 	1
option add *highlightThickness	0
option add *padx		0
option add *pady		0
option add *font 		$infofont

set V(class) "Mbone Applications"
set V(app)   "rat"

set iht			16
set iwd 		250
set cancel_info_timer 	0
set num_cname		0
set fw			.l.t.list.f

proc init_source {cname} {
	global CNAME NAME EMAIL LOC PHONE TOOL NOTE num_cname 
	global CODEC DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP JITTER LOSS_TO_ME LOSS_FROM_ME INDEX JIT_TOGED BUFFER_SIZE

	if {[array names INDEX $cname] != [list $cname]} {
		# This is a source we've not seen before...
		set        CNAME($cname) $cname
		set         NAME($cname) $cname
		set        EMAIL($cname) ""
		set        PHONE($cname) ""
		set          LOC($cname) ""
		set         TOOL($cname) ""
		set	    NOTE($cname) ""
		set        CODEC($cname) unknown
		set     DURATION($cname) ""
                set  BUFFER_SIZE($cname) 0
		set   PCKTS_RECV($cname) 0
		set   PCKTS_LOST($cname) 0
		set   PCKTS_MISO($cname) 0
		set   PCKTS_DUP($cname)  0
		set       JITTER($cname) 0
		set    JIT_TOGED($cname) 0
		set   LOSS_TO_ME($cname) 101
		set LOSS_FROM_ME($cname) 101
		set        INDEX($cname) $num_cname
		incr num_cname
		chart_enlarge $num_cname 
		chart_label   $cname
	}
}

proc window_plist {cname} {
    	global fw
	regsub -all {@|\.} $cname {-} foo
	return $fw.source-$foo
}

proc window_stats {cname} {
	regsub -all {[\. ]} $cname {-} foo
	return .stats$foo
}

# Commands to send message over the conference bus...
proc output_mute {state} {
    mbus_send "R" "audio.output.mute" "$state"
    if {$state} {
	.r.c.vol.t1 configure -relief sunken
	pack forget .r.c.vol.b1 .r.c.vol.s1
	pack .r.c.vol.ml -side top -fill both -expand 1
    } else {
	.r.c.vol.t1 configure -relief raised
	pack forget .r.c.vol.ml
	pack .r.c.vol.b1 -side top  -fill x -expand 1
	pack .r.c.vol.s1 -side top  -fill x -expand 1
    }
}

proc input_mute {state} {
    mbus_send "R" "audio.input.mute" "$state"
    if {$state} {
	.r.c.gain.t2 configure -relief sunken
	pack forget .r.c.gain.b2 .r.c.gain.s2
	pack .r.c.gain.ml -side top -fill both -expand 1
    } else {
	.r.c.gain.t2 configure -relief raised
	pack forget .r.c.gain.ml
	pack .r.c.gain.b2 -side top  -fill x -expand 1
	pack .r.c.gain.s2 -side top  -fill x -expand 1
    }
}

proc set_vol {new_vol} {
    global volume
    set volume $new_vol
    mbus_send "R" "audio.output.gain" $volume
}

proc set_gain {new_gain} {
    global gain
    set gain $new_gain
    mbus_send "R" "audio.input.gain" $gain
}

proc toggle_input_port {} {
  mbus_send "R" "tool.rat.toggle.input.port" ""
}

proc toggle_output_port {} {
  mbus_send "R" "tool.rat.toggle.output.port" ""
}

proc mbus_heartbeat {} {
}

#############################################################################################################
# Reception of Mbus messages...

proc mbus_recv {cmnd args} {
	# This is not the most efficient way of doing this, since we could call mbus_recv_... 
	# directly from the C code. It does, however, make it explicit which Mbus commands we
	# understand.
	switch $cmnd {
		mbus.waiting			{eval mbus_recv_mbus.waiting $args}
		mbus.go				{eval mbus_recv_mbus.go $args}
		mbus.hello			{eval mbus_recv_mbus.hello $args}
		mbus.quit  			{eval mbus_recv_mbus.quit $args}
		tool.rat.load.settings 		{eval mbus_recv_tool.rat.load.settings $args}
		tool.rat.sampling.supported 	{eval mbus_recv_tool.rat.sampling.supported $args}
		tool.rat.codec.supported  	{eval mbus_recv_tool.rat.codec.supported $args}
		tool.rat.redundancy.supported  	{eval mbus_recv_tool.rat.redundancy.supported $args}
		tool.rat.converter.supported	{eval mbus_recv_tool.rat.converter.supported $args}
		tool.rat.repair.supported	{eval mbus_recv_tool.rat.repair.supported $args}
		tool.rat.agc  			{eval mbus_recv_tool.rat.agc $args}
		tool.rat.sync  			{eval mbus_recv_tool.rat.sync $args}
		tool.rat.frequency  		{eval mbus_recv_tool.rat.frequency $args}
		tool.rat.channels  		{eval mbus_recv_tool.rat.channels $args}
		tool.rat.codec  		{eval mbus_recv_tool.rat.codec $args}
		tool.rat.rate  			{eval mbus_recv_tool.rat.rate $args}
		tool.rat.lecture.mode  		{eval mbus_recv_tool.rat.lecture.mode $args}
		tool.rat.disable.audio.ctls  	{eval mbus_recv_tool.rat.disable.audio.ctls $args}
		tool.rat.enable.audio.ctls  	{eval mbus_recv_tool.rat.enable.audio.ctls $args}
		tool.rat.audio.buffered  	{eval mbus_recv_tool.rat.audio.buffered $args}
		tool.rat.3d.enabled  		{eval mbus_recv_tool.rat.3d.enabled $args}
		tool.rat.3d.azimuth.min         {eval mbus_recv_tool.rat.3d.azimuth.min $args}
		tool.rat.3d.azimuth.max         {eval mbus_recv_tool.rat.3d.azimuth.max $args}
		tool.rat.3d.filter.types        {eval mbus_recv_tool.rat.3d.filter.types   $args}
		tool.rat.3d.filter.lengths      {eval mbus_recv_tool.rat.3d.filter.lengths $args}
		tool.rat.3d.user.settings       {eval mbus_recv_tool.rat.3d.user.settings  $args}
		audio.suppress.silence  	{eval mbus_recv_audio.suppress.silence $args}
		audio.channel.coding  		{eval mbus_recv_audio.channel.coding $args}
		audio.channel.repair 		{eval mbus_recv_audio.channel.repair $args}
		audio.input.gain  		{eval mbus_recv_audio.input.gain $args}
		audio.input.port  		{eval mbus_recv_audio.input.port $args}
		audio.input.mute  		{eval mbus_recv_audio.input.mute $args}
		audio.input.powermeter  	{eval mbus_recv_audio.input.powermeter $args}
		audio.output.gain  		{eval mbus_recv_audio.output.gain $args}
		audio.output.port  		{eval mbus_recv_audio.output.port $args}
		audio.output.mute  		{eval mbus_recv_audio.output.mute $args}
		audio.output.powermeter  	{eval mbus_recv_audio.output.powermeter $args}
		audio.file.play.ready   	{eval mbus_recv_audio.file.play.ready   $args}
		audio.file.play.alive   	{eval mbus_recv_audio.file.play.alive $args}
		audio.file.record.ready 	{eval mbus_recv_audio.file.record.ready $args}
		audio.file.record.alive 	{eval mbus_recv_audio.file.record.alive $args}
		audio.devices               {eval mbus_recv_audio_devices $args}
		audio.device                {eval mbus_recv_audio_device $args}
		session.title  			{eval mbus_recv_session.title $args}
		session.address  		{eval mbus_recv_session.address $args}
		rtp.cname  			{eval mbus_recv_rtp.cname $args}
		rtp.source.exists  		{eval mbus_recv_rtp.source.exists $args}
		rtp.source.remove  		{eval mbus_recv_rtp.source.remove $args}
		rtp.source.name  		{eval mbus_recv_rtp.source.name $args}
		rtp.source.email  		{eval mbus_recv_rtp.source.email $args}
		rtp.source.phone  		{eval mbus_recv_rtp.source.phone $args}
		rtp.source.loc  		{eval mbus_recv_rtp.source.loc $args}
		rtp.source.tool  		{eval mbus_recv_rtp.source.tool $args}
		rtp.source.note  		{eval mbus_recv_rtp.source.note $args}
		rtp.source.codec  		{eval mbus_recv_rtp.source.codec $args}
		rtp.source.packet.duration  	{eval mbus_recv_rtp.source.packet.duration $args}
		rtp.source.packet.loss  	{eval mbus_recv_rtp.source.packet.loss $args}
		rtp.source.reception  		{eval mbus_recv_rtp.source.reception $args}
		rtp.source.active  		{eval mbus_recv_rtp.source.active $args}
		rtp.source.inactive  		{eval mbus_recv_rtp.source.inactive $args}
		rtp.source.mute  		{eval mbus_recv_rtp.source.mute $args}
		security.encryption.key 	{eval mbus_recv_security.encryption.key $args}
		default				{puts "Unknown mbus command $cmnd"}
	}
}

proc mbus_recv_mbus.waiting {condition} {
	if {$condition == "rat.ui.init"} {
		mbus_send "U" "mbus.go" [mbus_encode_str rat.ui.init]
	}
}

proc mbus_recv_mbus.go {condition} {
}

proc mbus_recv_mbus.hello {} {
	# Ignore...
}

proc mbus_recv_tool.rat.load.settings {} {
    load_settings
    check_rtcp_name
    sync_engine_to_ui
    chart_show
    file_show
    toggle_plist
}

proc update_channels_displayed {} {
    global freq channel_support

    set m .prefs.pane.audio.dd.sampling.mchannels.menu
    $m delete 0 last
    
    set s [lsearch -glob $channel_support *$freq*]
    
    foreach i [lrange [split [lindex $channel_support $s] ","] 1 2] {
	$m add command -label "$i" -command "set channels $i; change_sampling"
    }
}

proc change_sampling { } {
    global freq channels

    update_channels_displayed

    mbus_send "R" "tool.rat.sampling" "[mbus_encode_str $freq] [mbus_encode_str $channels]"
}

proc mbus_recv_tool.rat.sampling.supported {arg} {
    global freq channel_support

    #clear away old state of channel support
    if [info exists channel_support] {
	unset channel_support
    }

    set freqs [list]
    set channel_support [list]

    .prefs.pane.audio.dd.sampling.mfreq.menu delete 0 last

    set mode [split $arg]
    foreach m $mode {
	lappend channel_support $m
	set support [split $m ","]
	set f [lindex $support 0]
	lappend freqs $f
	.prefs.pane.audio.dd.sampling.mfreq.menu add command -label $f -command "set freq $f; change_sampling"
    }
    set freq [lindex $freqs 0]
    update_channels_displayed
}

proc mbus_recv_tool.rat.codec.supported {arg} {
    # We now have a list of codecs which this RAT supports...
    global prenc

    .prefs.pane.transmission.dd.pri.m.menu delete 0 last

    set codecs [split $arg]
    foreach c $codecs {
	.prefs.pane.transmission.dd.pri.m.menu    add command -label $c -command "set prenc $c; validate_red_codecs"
    }    

    set prenc [lindex $codecs 0]
}

proc mbus_recv_tool.rat.redundancy.supported {arg} {
    global secenc

    .prefs.pane.transmission.cc.red.fc.m.menu delete 0 last

    set codecs [split $arg]
    foreach c $codecs {
	.prefs.pane.transmission.cc.red.fc.m.menu add command -label $c -command "set secenc \"$c\""
    }

    set secenc [lindex $codecs 0]
}

proc mbus_recv_tool.rat.converter.supported {arg} {
    global convert_var
    
    .prefs.pane.reception.r.ms.menu delete 0 last
    
    set converters [split $arg ","]
    foreach c $converters {
	.prefs.pane.reception.r.ms.menu add command -label $c -command "set convert_var \"$c\""
    }
}

proc mbus_recv_tool.rat.repair.supported {arg} {
    .prefs.pane.reception.r.m.menu delete 0 last
    
    set schemes [split $arg ","]
    foreach rep $schemes {
	.prefs.pane.reception.r.m.menu add command -label $rep -command "set repair_var \"$rep\""
    }
}

proc mbus_recv_audio_devices {arg} {
	global audio_device
	
	set m .prefs.pane.audio.dd.device.mdev
	set max_len 0
	
	$m.menu delete 0 last
	set devices [split $arg ","]
	foreach d $devices {
		$m.menu add command -label "$d" -command "set audio_device \"$d\""
		set len [string length "$d"]
		if [expr $len > $max_len] {
			set max_len $len
		}
	}
	$m configure -width $max_len
}

proc mbus_recv_audio_device {arg} {
	global audio_device

	set audio_device $arg
}

proc mbus_recv_tool.rat.agc {arg} {
  global agc_var
  set agc_var $arg
}

proc mbus_recv_tool.rat.sync {arg} {
  global sync_var
  set sync_var $arg
}

proc mbus_recv_security.encryption.key {new_key} {
	global key_var key
	set key_var 1
	set key     $new_key
}

proc mbus_recv_tool.rat.frequency {arg} {
  global freq
  set freq $arg
}

proc mbus_recv_tool.rat.channels {arg} {
  global channels
  set channels $arg
}

proc mbus_recv_tool.rat.codec {arg} {
  global prenc
  set prenc $arg
}

proc mbus_recv_tool.rat.rate {arg} {
    global upp
    set upp $arg
}

proc mbus_recv_audio.channel.coding {channel args} {
    global channel_var secenc red_off int_units int_gap
    set channel_var $channel
    switch $channel {
    	redundant {
		set secenc  [lindex $args 0]
		set red_off [lindex $args 1]
	}
	interleaved {
		set int_units [lindex $args 0]
		set int_gap   [lindex $args 1]
	}
    }
}

proc mbus_recv_audio.channel.repair {arg} {
  global repair_var
  set repair_var $arg
}

proc mbus_recv_audio.input.powermeter {level} {
	global bargraphTotalHeight
	bargraphSetHeight .r.c.gain.b2 [expr ($level * $bargraphTotalHeight) / 100]
}

proc mbus_recv_audio.output.powermeter {level} {
	global bargraphTotalHeight
	bargraphSetHeight .r.c.vol.b1  [expr ($level * $bargraphTotalHeight) / 100]
}

proc mbus_recv_audio.input.gain {new_gain} {
    global gain
    set gain $new_gain
    .r.c.gain.s2 set $gain
}

proc mbus_recv_audio.input.port {device} {
	global input_port
	set input_port $device
	.r.c.gain.l2 configure -bitmap $device
}

proc mbus_recv_audio.input.mute {val} {
    global in_mute_var
    set in_mute_var $val
    if {$val} {
	.r.c.gain.t2 configure -relief sunken
	pack forget .r.c.gain.b2 .r.c.gain.s2
	pack .r.c.gain.ml -side top -fill both -expand 1
    } else {
	.r.c.gain.t2 configure -relief raised
	pack forget .r.c.gain.ml
	pack .r.c.gain.b2 -side top  -fill x -expand 1
	pack .r.c.gain.s2 -side top  -fill x -expand 1
    }
}

proc mbus_recv_audio.output.gain {gain} {
	.r.c.vol.s1 set $gain
}

proc mbus_recv_audio.output.port {device} {
	global output_port
	set output_port $device
	.r.c.vol.l1 configure -bitmap $device
}

proc mbus_recv_audio.output.mute {val} {
    global out_mute_var
    set out_mute_var $val
    if {$val} {
	.r.c.vol.t1 configure -relief sunken
    } else {
	.r.c.vol.t1 configure -relief raised
    }
}

proc mbus_recv_session.title {title} {
    global session_title
    set session_title $title
    wm title . "[wm title .]: $title"
}

proc mbus_recv_session.address {addr port ttl} {
    global session_address
    set session_address "Address: $addr Port: $port TTL: $ttl"
}

proc mbus_recv_tool.rat.lecture.mode {mode} {
	global lecture_var
	set lecture_var $mode
}

proc mbus_recv_audio.suppress.silence {mode} {
	global silence_var
	set silence_var $mode
}

proc mbus_recv_rtp.cname {cname} {
	global my_cname rtcp_name rtcp_email rtcp_phone rtcp_loc num_cname

	set my_cname $cname
	init_source  $cname
	cname_update $cname
}

proc mbus_recv_rtp.source.exists {cname} {
	init_source $cname
	chart_label $cname
	cname_update $cname
}

proc mbus_recv_rtp.source.name {cname name} {
	global NAME
	init_source $cname
	set NAME($cname) $name
	chart_label $cname
	cname_update $cname
}

proc mbus_recv_rtp.source.email {cname email} {
	global EMAIL
	init_source $cname
	set EMAIL($cname) $email
}

proc mbus_recv_rtp.source.phone {cname phone} {
	global PHONE
	init_source $cname
	set PHONE($cname) $phone
}

proc mbus_recv_rtp.source.loc {cname loc} {
	global LOC
	init_source $cname
	set LOC($cname) $loc
}

proc mbus_recv_rtp.source.tool {cname tool} {
	global TOOL my_cname
	init_source $cname
	set TOOL($cname) $tool
	if {[string compare $cname $my_cname] == 0} {
	    wm title . "UCL $tool"
	}
}

proc mbus_recv_rtp.source.note {cname note} {
	global NOTE
	init_source $cname
	set NOTE($cname) $note
}

proc mbus_recv_rtp.source.codec {cname codec} {
	global CODEC
	init_source $cname
	set CODEC($cname) $codec
}

proc mbus_recv_rtp.source.packet.duration {cname packet_duration} {
	global DURATION
	init_source $cname
	set DURATION($cname) $packet_duration
}

proc mbus_recv_tool.rat.audio.buffered {cname buffered} {
        global BUFFER_SIZE
        init_source $cname
        set BUFFER_SIZE($cname) $buffered 
# we don't update cname as source.packet.duration always follows 
}

proc mbus_recv_tool.rat.3d.enabled {mode} {
	global 3d_audio_var
	set 3d_audio_var $mode
}

proc mbus_recv_tool.rat.3d.azimuth.min {min} {
    global 3d_azimuth
    set 3d_azimuth(min) $min
}

proc mbus_recv_tool.rat.3d.azimuth.max {max} {
    global 3d_azimuth
    set 3d_azimuth(max) $max
}

proc mbus_recv_tool.rat.3d.filter.types {args} {
    global 3d_filters
    set 3d_filters [split $args ","]
}

proc mbus_recv_tool.rat.3d.filter.lengths {args} {
    global 3d_filter_lengths
    puts "$args"
    set 3d_filter_lengths [split $args ","]
}

proc mbus_recv_tool.rat.3d.user.settings {args} {
    global filter_type filter_length azimuth
    set cname                 [lindex $args 0]
    set filter_type($cname)   [lindex $args 1]
    set filter_length($cname) [lindex $args 2]
    set azimuth($cname)       [lindex $args 3]
}

proc mbus_recv_rtp.source.packet.loss {dest srce loss} {
	global losstimers my_cname LOSS_FROM_ME LOSS_TO_ME
	init_source $srce
	init_source $dest
	catch {after cancel $losstimers($srce,$dest)}
	chart_set $srce $dest $loss
	set losstimers($srce,$dest) [after 7500 chart_set \"$srce\" \"$dest\" 101]
	if {[string compare $dest $my_cname] == 0} {
		set LOSS_TO_ME($srce) $loss
	}
	if {[string compare $srce $my_cname] == 0} {
		set LOSS_FROM_ME($dest) $loss
	}
	cname_update $srce
	cname_update $dest
}

proc mbus_recv_rtp.source.reception {cname packets_recv packets_lost packets_miso packets_dup jitter jit_tog} {
	global PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP JITTER JIT_TOGED
	init_source $cname 
	set PCKTS_RECV($cname) $packets_recv
	set PCKTS_LOST($cname) $packets_lost
	set PCKTS_MISO($cname) $packets_miso
	set PCKTS_DUP($cname)  $packets_dup
	set JITTER($cname) $jitter
	set JIT_TOGED($cname) $jit_tog
}

proc mbus_recv_rtp.source.active {cname} {
    catch [[window_plist $cname] configure -background white]
    cname_update $cname
}

proc mbus_recv_rtp.source.inactive {cname} {
    catch [[window_plist $cname] configure -background grey90]
    after 60 "catch {[window_plist $cname] configure -background grey88}"
    after 120 "catch {[window_plist $cname] configure -background [.l.t.list cget -background]}"
    cname_update $cname
}

proc mbus_recv_rtp.source.remove {cname} {
	global CNAME NAME EMAIL LOC PHONE TOOL NOTE CODEC DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP JITTER BUFFER_SIZE
	global LOSS_TO_ME LOSS_FROM_ME INDEX JIT_TOGED num_cname loss_to_me_timer loss_from_me_timer

	# Disable updating of loss diamonds. This has to be done before we destroy the
	# window representing the participant, else the background update may try to 
	# access a window which has been destroyed...
	catch {after cancel $loss_to_me_timer($cname)}
	catch {after cancel $loss_from_me_timer($cname)}

	catch [destroy [window_plist $cname]]

	unset CNAME($cname) NAME($cname) EMAIL($cname) PHONE($cname) LOC($cname) TOOL($cname) NOTE($cname)
	unset CODEC($cname) DURATION($cname) PCKTS_RECV($cname) PCKTS_LOST($cname) PCKTS_MISO($cname) PCKTS_DUP($cname)
	unset JITTER($cname) LOSS_TO_ME($cname) LOSS_FROM_ME($cname) INDEX($cname) JIT_TOGED($cname) BUFFER_SIZE($cname)

	incr num_cname -1
	chart_redraw $num_cname
}

proc mbus_recv_rtp.source.mute {cname val} {
	global iht

	if {$val} {
		[window_plist $cname] create line [expr $iht + 2] [expr $iht / 2] 500 [expr $iht / 2] -tags a -width 2.0 -fill gray95
	} else {
		catch [[window_plist $cname] delete a]
	}
}

proc mbus_recv_audio.file.play.ready {name} {
	global play_file
	set    play_file(name) $name
	file_enable_play
}

proc mbus_recv_audio.file.play.alive {alive} {
	global play_file
	
	puts "file_play_live"
	if {$alive} {
		after 200 file_play_live
	} else {
		set play_file(state) end
		file_enable_play
	}
}

proc mbus_recv_audio.file.record.ready {name} {
	global record_file
	set    record_file(name) $name
	file_enable_record
}

proc mbus_recv_audio.file.record.alive {alive} {
	global rec_file
	if {$alive} {
		after 200 file_rec_live
	} else {
		set rec_file(state) end
		file_enable_record                                          
	}
}

proc mbus_recv_mbus.quit {} {
	save_settings 
	destroy .
}

#############################################################################################################

proc set_loss_to_me {cname loss} {
	global prev_loss_to_me loss_to_me_timer

	catch {after cancel $loss_to_me_timer($cname)}
	set loss_to_me_timer($cname) [after 7500 catch \"[window_plist $cname] itemconfigure h -fill grey\"]

	if {$loss < 5} {
		catch [[window_plist $cname] itemconfigure m -fill green]
	} elseif {$loss < 10} {
		catch [[window_plist $cname] itemconfigure m -fill orange]
	} elseif {$loss <= 100} {
		catch [[window_plist $cname] itemconfigure m -fill red]
	} else {
		catch [[window_plist $cname] itemconfigure m -fill grey]
	}
}

proc set_loss_from_me {cname loss} {
	global prev_loss_from_me loss_from_me_timer

	catch {after cancel $loss_from_me_timer($cname)}
	set loss_from_me_timer($cname) [after 7500 catch \"[window_plist $cname] itemconfigure h -fill grey\"]

	if {$loss < 5} {
		catch [[window_plist $cname] itemconfigure h -fill green]
	} elseif {$loss < 10} {
		catch [[window_plist $cname] itemconfigure h -fill orange]
	} elseif {$loss <= 100} {
		catch [[window_plist $cname] itemconfigure h -fill red]
	} else {
		catch [[window_plist $cname] itemconfigure h -fill grey]
	}
}

proc cname_update {cname} {
	# This procedure updates the on-screen representation of
	# a participant. 
	global NAME LOSS_TO_ME LOSS_FROM_ME
	global fw iht iwd my_cname 

	set cw [window_plist $cname]

	if {[winfo exists $cw]} {
		$cw itemconfigure t -text $NAME($cname)
	} else {
		# Add this participant to the list...
		set thick 0
		set l $thick
		set h [expr $iht / 2 + $thick]
		set f [expr $iht + $thick]
		canvas $cw -width $iwd -height $f -highlightthickness $thick
		$cw create text [expr $f + 2] $h -anchor w -text $NAME($cname) -fill black -tag t
		$cw create polygon $l $h $h $l $h $f -outline black -fill grey50 -tag m
		$cw create polygon $f $h $h $l $h $f -outline black -fill grey50 -tag h

		bind $cw <Button-1>         "toggle_stats \"$cname\""
		bind $cw <Button-2>         "toggle_mute $cw \"$cname\""
		bind $cw <Control-Button-1> "toggle_mute $cw \"$cname\""

		if {[info exists my_cname] && ([string compare $cname $my_cname] == 0) && ([pack slaves $fw] != "")} {
			pack $cw -before [lindex [pack slaves $fw] 0] -fill x
		}
		pack $cw -fill x
		fix_scrollbar
	}

	set_loss_to_me $cname $LOSS_TO_ME($cname)
	set_loss_from_me $cname $LOSS_FROM_ME($cname)
}

#power meters

# number of elements in the bargraphs...
set bargraphTotalHeight 24
set bargraphRedHeight [expr $bargraphTotalHeight * 3 / 4] 

proc bargraphCreate {bgraph} {
	global oh$bgraph bargraphTotalHeight

	frame $bgraph -bg black
	frame $bgraph.inner0 -width 8 -height 6 -bg green
	pack $bgraph.inner0 -side left -padx 0 -fill both -expand true
	for {set i 1} {$i < $bargraphTotalHeight} {incr i} {
		frame $bgraph.inner$i -width 8 -height 8 -bg black
		pack $bgraph.inner$i -side left -padx 0 -fill both -expand true
	}
	set oh$bgraph 0
}

proc bargraphSetHeight {bgraph height} {
	upvar #0 oh$bgraph oh 
	global bargraphTotalHeight bargraphRedHeight

	if {$oh > $height} {
		for {set i [expr $height + 1]} {$i <= $oh} {incr i} {
			$bgraph.inner$i config -bg black
		}
	} else {
		if {$bargraphRedHeight > $height} {
			for {set i [expr $oh + 1]} {$i <= $height} {incr i} {
				$bgraph.inner$i config -bg green
			}
		} else {
			for {set i [expr $oh + 1]} {$i <= $bargraphRedHeight} {incr i} {
				$bgraph.inner$i config -bg green
			}
			for {set i $bargraphRedHeight} {$i <= $height} {incr i} {
				$bgraph.inner$i config -bg red
			}
		}
	}
	set oh $height
}

proc toggle {varname} {
    upvar 1 $varname local
    set local [expr !$local]
}

proc toggle_plist {} {
	global plist_on
	if {$plist_on} {
		pack .l.t  -side top -fill both -expand 1
	} else {
		pack forget .l.t
	}
	update
	wm deiconify .
}

proc toggle_mute {cw cname} {
	global iht
	if {[$cw gettags a] == ""} {
		mbus_send "R" "rtp.source.mute" "[mbus_encode_str $cname] 1"
	} else {
		mbus_send "R" "rtp.source.mute" "[mbus_encode_str $cname] 0"
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

proc stats_add_field {widget label watchVar} {
    global statsfont
    frame $widget -relief sunk 
    label $widget.l -text $label -font $statsfont -anchor w
    label $widget.w -textvariable $watchVar -font $statsfont
    pack $widget -side top -fill x -expand 1 
    pack $widget.l -side left  -fill x -expand 1
    pack $widget.w -side right 
}

set 3d_azimuth(min) 0
set 3d_azimuth(max) 0
set 3d_filters        [list "Not Available"]
set 3d_filter_lengths [list "0"]

proc toggle_stats {cname} {
    global statsfont
    set win [window_stats $cname]
    if {[winfo exists $win]} {
	destroy $win
    } else {
	global stats_pane
	# Window does not exist so create it
	toplevel $win 
	frame $win.mf
	pack $win.mf -padx 0 -pady 0
	label $win.mf.l -text "Category:"
	
	menubutton $win.mf.mb -menu $win.mf.mb.menu -indicatoron 1 -textvariable stats_pane($win) -relief raised -width 16
	pack $win.mf.l $win.mf.mb -side left
	menu $win.mf.mb.menu -tearoff 0
	$win.mf.mb.menu add command -label "Personal Details" -command "set_pane stats_pane($win) $win.df \"Personal Details\""
	$win.mf.mb.menu add command -label "Reception"        -command "set_pane stats_pane($win) $win.df Reception"
	$win.mf.mb.menu add command -label "3D Positioning"   -command "set_pane stats_pane($win) $win.df \"3D Positioning\""

	set stats_pane($win) "Personal Details"
	frame $win.df 
	frame $win.df.personal
	pack  $win.df $win.df.personal -fill x 
	global NAME EMAIL PHONE LOC NOTE CNAME TOOL
	stats_add_field $win.df.personal.1 "Name: "     NAME($cname)
	stats_add_field $win.df.personal.2 "Email: "    EMAIL($cname)
	stats_add_field $win.df.personal.3 "Phone: "    PHONE($cname)
	stats_add_field $win.df.personal.4 "Location: " LOC($cname)
	stats_add_field $win.df.personal.5 "Note: "     NOTE($cname)
	stats_add_field $win.df.personal.6 "Tool: "     TOOL($cname)
	stats_add_field $win.df.personal.7 "CNAME: "    CNAME($cname)

	frame $win.df.reception
	global CODEC DURATION BUFFER_SIZE PCKTS_RECV PCKTS_LOST PCKTS_MISO \
	       PCKTS_DUP LOSS_FROM_ME LOSS_TO_ME JITTER JIT_TOGED
	stats_add_field $win.df.reception.1 "Audio encoding: "         CODEC($cname)
	stats_add_field $win.df.reception.2 "Packet duration (ms): "   DURATION($cname)
	stats_add_field $win.df.reception.3 "Buffer length (ms): "     BUFFER_SIZE($cname)
	stats_add_field $win.df.reception.4 "Arrival jitter (ms): "    JITTER($cname)
	stats_add_field $win.df.reception.5 "Loss from me (%): "       LOSS_FROM_ME($cname)
	stats_add_field $win.df.reception.6 "Loss to me (%): "         LOSS_TO_ME($cname)
	stats_add_field $win.df.reception.7 "Packets received: "       PCKTS_RECV($cname)
	stats_add_field $win.df.reception.8 "Packets lost: "           PCKTS_LOST($cname)
	stats_add_field $win.df.reception.9 "Packets misordered: "     PCKTS_MISO($cname)
	stats_add_field $win.df.reception.a "Packets duplicated: "     PCKTS_DUP($cname)
	stats_add_field $win.df.reception.b "Units dropped (jitter): " JIT_TOGED($cname)

# 3D settings
	# Trigger engine to send details for this participant
	mbus_send "R" "tool.rat.3d.user.settings.request" [mbus_encode_str $cname]

	frame $win.df.3d -relief sunk
	label $win.df.3d.advice -text "These options allow the rendering of the\nparticipant to be altered when 3D\nrendering is enabled."
	checkbutton $win.df.3d.ext -text "3D Audio Rendering" -variable 3d_audio_var
	pack $win.df.3d.advice 
	pack $win.df.3d.ext 

	frame $win.df.3d.opts 
	pack $win.df.3d.opts -side top 

	frame $win.df.3d.opts.filters
	label $win.df.3d.opts.filters.l -text "Filter Type:"
	pack $win.df.3d.opts.filters.l -side top -fill x -expand 1 -anchor w
	global 3d_filters 3d_filter_lengths
	
	global filter_type
	set filter_type($cname) [lindex $3d_filters 0]

	set cnt 0
	foreach i $3d_filters {
	    radiobutton $win.df.3d.opts.filters.$cnt \
		    -value "$i" -variable filter_type($cname) \
		    -text "$i"
 		pack $win.df.3d.opts.filters.$cnt -side top -anchor w
	    incr cnt
	}

	frame $win.df.3d.opts.lengths 
	label $win.df.3d.opts.lengths.l -text "Filter Length:" -width 16
	pack $win.df.3d.opts.lengths.l -side top -fill x -expand 1
	
	global filter_length
	set filter_length($cname) [lindex $3d_filter_lengths 0]
	
	set cnt 0
	foreach i $3d_filter_lengths {
	    radiobutton $win.df.3d.opts.lengths.$cnt \
		    -value "$i" -variable filter_length($cname) \
		    -text "$i"
	    pack $win.df.3d.opts.lengths.$cnt -side top -anchor w
	    incr cnt
	}
	pack $win.df.3d.opts.filters -side left -expand 1 -anchor n
	pack $win.df.3d.opts.lengths -side left -expand 1 -anchor n
	
	global 3d_azimuth azimuth
	scale $win.df.3d.azimuth -from $3d_azimuth(min) -to $3d_azimuth(max) \
		-orient horizontal -label "Azimuth" -variable azimuth($cname)
	pack  $win.df.3d.azimuth -fill x -expand 1 

	button $win.df.3d.apply -text "Apply" -command "3d_send_parameters $cname"
	pack   $win.df.3d.apply -side left -fill x -expand 1 -anchor s

# Window Magic 
	button $win.d -highlightthickness 0 -padx 0 -pady 0 -text "Dismiss" -command "destroy $win; 3d_delete_parameters $cname" 
	pack $win.d -side bottom -fill x
	wm title $win "Participant $NAME($cname)"
	wm resizable $win 1 0
	constrain_window $win 0 250 20 0
    }
}

proc 3d_send_parameters {cname} {
    global azimuth filter_type filter_length 3d_audio_var

    mbus_send "R" "tool.rat.3d.enabled"   $3d_audio_var
    mbus_send "R" "tool.rat.3d.user.settings" "[mbus_encode_str $cname] [mbus_encode_str $filter_type($cname)] $filter_length($cname) $azimuth($cname)"
}

proc 3d_delete_parameters {cname} {
    global filter_type filter_length azimuth
    
# None of these should ever fail, but you can't be too defensive...
    catch {
	unset filter_type($cname)
	unset filter_length($cname)
	unset azimuth($cname)
    }
}

proc do_quit {} {
	catch {
		profile off pdat
		profrep pdat cpu
	}
	save_settings 
	destroy .
	mbus_send "R" "mbus.quit" ""
}

# Initialise RAT MAIN window
frame .r 
frame .l 
frame .l.t -relief raised
scrollbar .l.t.scr -relief flat -highlightthickness 0 -command ".l.t.list yview"
canvas .l.t.list -highlightthickness 0 -bd 0 -relief raised -width $iwd -height 160 -yscrollcommand ".l.t.scr set" -yscrollincrement $iht
frame .l.t.list.f -highlightthickness 0 -bd 0
.l.t.list create window 0 0 -anchor nw -window .l.t.list.f

frame .l.f -relief raised
label .l.f.title -font $infofont  -textvariable session_title
label .l.f.addr  -font $smallfont -textvariable session_address

frame  .l.s1 -bd 0
button .l.s1.opts  -highlightthickness 0 -padx 0 -pady 0 -text "Options"   -command {wm deiconify .prefs; raise .prefs}
button .l.s1.about -highlightthickness 0 -padx 0 -pady 0 -text "About"     -command {jiggle_credits; wm deiconify .about}
button .l.s1.quit  -highlightthickness 0 -padx 0 -pady 0 -text "Quit"      -command do_quit

frame .r.c
frame .r.c.vol 
frame .r.c.gain 

pack .r -side top -fill x
pack .r.c -side top -fill x -expand 1
pack .r.c.vol  -side top -fill x
pack .r.c.gain -side top -fill x

pack .l -side top -fill both -expand 1
pack .l.f -side top -fill x
pack .l.f.title .l.f.addr -side top -fill x
pack .l.s1 -side bottom -fill x
pack .l.s1.opts .l.s1.about .l.s1.quit -side left -fill x -expand 1
pack .l.t  -side top -fill both -expand 1
pack .l.t.scr -side left -fill y
pack .l.t.list -side left -fill both -expand 1
bind .l.t.list <Configure> {fix_scrollbar}

# Device output controls
set out_mute_var 0
button .r.c.vol.t1 -highlightthickness 0 -pady 0 -padx 0 -text mute -command {toggle out_mute_var; output_mute $out_mute_var}
set output_port "speaker"
button .r.c.vol.l1 -highlightthickness 0 -command toggle_output_port
bargraphCreate .r.c.vol.b1
scale .r.c.vol.s1 -highlightthickness 0 -from 0 -to 99 -command set_vol -orient horizontal -relief raised -showvalue false -width 10 -variable volume
label .r.c.vol.ml -text "Reception is muted" -relief sunken

pack .r.c.vol.l1 -side left -fill y
pack .r.c.vol.t1 -side left -fill y
pack .r.c.vol.b1 -side top  -fill x -expand 1
pack .r.c.vol.s1 -side top  -fill x -expand 1

# Device input controls
set in_mute_var 1
button .r.c.gain.t2 -highlightthickness 0 -pady 0 -padx 0 -text mute -command {toggle in_mute_var; input_mute $in_mute_var}
set input_port "microphone"
button .r.c.gain.l2 -highlightthickness 0 -command toggle_input_port 
bargraphCreate .r.c.gain.b2
scale .r.c.gain.s2 -highlightthickness 0 -from 0 -to 99 -command set_gain -orient horizontal -relief raised -showvalue false -width 10 -variable gain
label .r.c.gain.ml -text "Transmission is muted" -relief sunken

pack .r.c.gain.l2 -side left -fill y
pack .r.c.gain.t2 -side left -fill y
pack .r.c.gain.ml -side top  -fill both -expand 1

proc mbus_recv_tool.rat.disable.audio.ctls {} {
	.r.c.vol.t1 configure -state disabled
	.r.c.vol.l1 configure -state disabled
	.r.c.vol.s1 configure -state disabled
	.r.c.gain.t2 configure -state disabled
	.r.c.gain.l2 configure -state disabled
	.r.c.gain.s2 configure -state disabled
}

proc mbus_recv_tool.rat.enable.audio.ctls {} {
	.r.c.vol.t1 configure -state normal
	.r.c.vol.l1 configure -state normal
	.r.c.vol.s1 configure -state normal
	.r.c.gain.t2 configure -state normal
	.r.c.gain.l2 configure -state normal
	.r.c.gain.s2 configure -state normal
}
bind all <ButtonPress-3>   {toggle in_mute_var; input_mute $in_mute_var}
bind all <ButtonRelease-3> {toggle in_mute_var; input_mute $in_mute_var}
bind all <q>               "do_quit"

# Override default tk behaviour
wm protocol . WM_DELETE_WINDOW do_quit

if {$win32 == 0} {
	wm iconbitmap . rat_small
}
wm resizable . 0 1
if ([info exists geometry]) {
        wm geometry . $geometry
}

proc constrain_window {win maxstr xpad ylines ypad} {
    set fn [.prefs.buttons.apply cget -font]
    set w  [expr [font measure $fn $maxstr] + $xpad]
    set h  [expr $ylines * [font metrics $fn -linespace] + $ypad]
    wm geometry $win [format "%sx%s" $w $h]
}

proc tk_optionCmdMenu {w varName firstValue args} {
    upvar #0 $varName var
 
    if ![info exists var] {
        set var $firstValue
    }
    menubutton $w -textvariable $varName -indicatoron 1 -menu $w.menu \
            -relief raised -bd 2 -highlightthickness 2 -anchor c 

    menu $w.menu -tearoff 0
    $w.menu add command -label $firstValue -command "set $varName \"$firstValue\""
    foreach i $args {
        $w.menu add command -label $i -command "set $varName \"$i\""
    }
    return $w.menu
}


###############################################################################
# Preferences Panel 
#

set prefs_pane "Personal"
toplevel .prefs
wm title .prefs "Preferences"
wm resizable .prefs 0 0
wm withdraw  .prefs

frame .prefs.m
pack .prefs.m -side top -fill x -expand 0 -padx 2 -pady 2
frame .prefs.m.f 
pack .prefs.m.f -padx 0 -pady 0 
label .prefs.m.f.t -text "Category: "
pack .prefs.m.f.t -pady 2 -side left
menubutton .prefs.m.f.m -menu .prefs.m.f.m.menu -indicatoron 1 -textvariable prefs_pane -relief raised -width 14
pack .prefs.m.f.m -side top 
menu .prefs.m.f.m.menu -tearoff 0
.prefs.m.f.m.menu add command -label "Personal"     -command {set_pane prefs_pane .prefs.pane "Personal"}
.prefs.m.f.m.menu add command -label "Transmission" -command {set_pane prefs_pane .prefs.pane "Transmission"}
.prefs.m.f.m.menu add command -label "Reception"    -command {set_pane prefs_pane .prefs.pane "Reception"}
.prefs.m.f.m.menu add command -label "Audio"        -command {set_pane prefs_pane .prefs.pane "Audio"}
.prefs.m.f.m.menu add command -label "Security"     -command {set_pane prefs_pane .prefs.pane "Security"}
.prefs.m.f.m.menu add command -label "Interface"    -command {set_pane prefs_pane .prefs.pane "Interface"}

frame  .prefs.buttons
pack   .prefs.buttons       -side bottom -fill x 
button .prefs.buttons.bye   -text "Cancel"                   -command {sync_ui_to_engine; wm withdraw .prefs} -width 10
button .prefs.buttons.apply -text "Apply Preferences"        -command {wm withdraw .prefs; sync_engine_to_ui}
button .prefs.buttons.save  -text "Save & Apply Preferences" -command {save_settings; wm withdraw .prefs; sync_engine_to_ui}
pack   .prefs.buttons.bye .prefs.buttons.apply .prefs.buttons.save -side left -fill x -expand 1

frame .prefs.pane -relief sunken
pack  .prefs.pane -side left -fill both -expand 1 -padx 4 -pady 2

# setup width of prefs panel
constrain_window .prefs "XXXXXXXXXXXXXX48-kHzXXXStereoXXXLinear-16XXXUnitsXPerXPcktXXXXXXXXXXXXX" 0 12 128

# Personal Info Pane
set i .prefs.pane.personal
frame $i
pack $i -fill both -expand 1 -pady 2 -padx 2

frame $i.a -relief sunken 
frame $i.a.f 
pack $i.a -side top -fill both -expand 1 
pack $i.a.f -side left -fill x -expand 1

frame $i.a.f.f 
pack $i.a.f.f

label $i.a.f.f.l -width 40 -height 2 -text "The personal details below are conveyed\nto the other conference participants." -justify left -anchor w
pack $i.a.f.f.l -side top -anchor w -fill x

frame $i.a.f.f.lbls
frame $i.a.f.f.ents
pack  $i.a.f.f.lbls -side left -fill y
pack  $i.a.f.f.ents -side right

label $i.a.f.f.lbls.name  -text "Name:"     -anchor w
label $i.a.f.f.lbls.email -text "Email:"    -anchor w
label $i.a.f.f.lbls.phone -text "Phone:"    -anchor w
label $i.a.f.f.lbls.loc   -text "Location:" -anchor w
pack $i.a.f.f.lbls.name $i.a.f.f.lbls.email $i.a.f.f.lbls.phone $i.a.f.f.lbls.loc -fill x -anchor w -side top

entry $i.a.f.f.ents.name  -width 28 -highlightthickness 0 -textvariable rtcp_name
entry $i.a.f.f.ents.email -width 28 -highlightthickness 0 -textvariable rtcp_email
entry $i.a.f.f.ents.phone -width 28 -highlightthickness 0 -textvariable rtcp_phone
entry $i.a.f.f.ents.loc   -width 28 -highlightthickness 0 -textvariable rtcp_loc
pack $i.a.f.f.ents.name $i.a.f.f.ents.email $i.a.f.f.ents.phone $i.a.f.f.ents.loc -anchor n -expand 0 

# Transmission Pane ###########################################################
set i .prefs.pane.transmission
frame $i 
frame $i.dd  -relief sunken
frame $i.cc  -relief sunken
frame $i.cc.van 
frame $i.cc.red 
frame $i.cc.int 

pack $i.dd -fill x
pack $i.cc $i.cc.van $i.cc.red -fill x -anchor w -pady 1
pack $i.cc.int -fill x -anchor w -pady 0
frame $i.dd.units
frame $i.dd.pri

pack $i.dd.units $i.dd.pri -side right -fill x 

label $i.dd.pri.l -text "Encoding:"
menubutton $i.dd.pri.m -menu $i.dd.pri.m.menu -indicatoron 1 -textvariable prenc -relief raised -width 13
pack $i.dd.pri.l $i.dd.pri.m -side top
# fill in codecs 
menu $i.dd.pri.m.menu -tearoff 0

label $i.dd.units.l -text "Units:"
tk_optionCmdMenu $i.dd.units.m upp 1 2 4 8
$i.dd.units.m configure -width 13 -highlightthickness 0 -bd 1
pack $i.dd.units.l $i.dd.units.m -side top -fill x

radiobutton $i.cc.van.rb -text "No Loss Protection" -justify right -value none        -variable channel_var
radiobutton $i.cc.red.rb -text "Redundancy"         -justify right -value redundant   -variable channel_var 
radiobutton $i.cc.int.rb -text "Interleaving"       -justify right -value interleaved -variable channel_var
pack $i.cc.van.rb $i.cc.red.rb $i.cc.int.rb -side left -anchor nw -padx 2

frame $i.cc.red.fc 
label $i.cc.red.fc.l -text "Encoding:"
menubutton $i.cc.red.fc.m -textvariable secenc -indicatoron 1 -menu $i.cc.red.fc.m.menu -relief raised -width 13
menu $i.cc.red.fc.m.menu -tearoff 0

frame $i.cc.red.u 
label $i.cc.red.u.l -text "Offset in Pkts:"
tk_optionCmdMenu $i.cc.red.u.m red_off "1" "2" "4" "8" 
$i.cc.red.u.m configure -width 13 -highlightthickness 0 -bd 1 
pack $i.cc.red.u -side right -anchor e -fill y 
pack $i.cc.red.u.l $i.cc.red.u.m -fill x 
pack $i.cc.red.fc -side right
pack $i.cc.red.fc.l $i.cc.red.fc.m 

frame $i.cc.int.zz
label $i.cc.int.zz.l -text "Units:"
tk_optionCmdMenu $i.cc.int.zz.m int_units 2 4 6 8 
$i.cc.int.zz.m configure -width 13 -highlightthickness 0 -bd 1

frame $i.cc.int.fc
label $i.cc.int.fc.l -text "Separation:"
tk_optionCmdMenu $i.cc.int.fc.m int_gap 2 4 6 8 
$i.cc.int.fc.m configure -width 13 -highlightthickness 0 -bd 1

pack $i.cc.int.fc $i.cc.int.zz -side right
pack $i.cc.int.fc.l $i.cc.int.fc.m -fill x -expand 1
pack $i.cc.int.zz.l $i.cc.int.zz.m -fill x -expand 1

# Reception Pane ##############################################################
set i .prefs.pane.reception
frame $i 
frame $i.r -relief sunken
frame $i.o -relief sunken
frame $i.c -relief sunken
pack $i.r -side top -fill x -pady 0 -ipady 1
pack $i.o -side top -fill both  -pady 1
pack $i.c -side top -fill both  -pady 1 -expand 1
label $i.r.l -text "Repair Scheme:"
tk_optionCmdMenu $i.r.m repair_var {None}

label $i.r.ls -text "Sample Rate Conversion"
tk_optionCmdMenu $i.r.ms convert_var {None}

$i.r.m  configure -width 20 -bd 1
$i.r.ms configure -width 20 -bd 1
pack $i.r.l $i.r.m $i.r.ls $i.r.ms -side top 

frame $i.o.f
checkbutton $i.o.f.cb -text "Limit Playout Delay" -variable limit_var
frame $i.o.f.fl
label $i.o.f.fl.l1 -text "Minimum Delay (ms)" 
scale $i.o.f.fl.scmin -orient horizontal -from 0 -to 1000    -variable min_var -font $smallfont
frame $i.o.f.fr 
label $i.o.f.fr.l2 -text "Maximum Delay (ms)"            
scale $i.o.f.fr.scmax -orient horizontal -from 1000 -to 2000 -variable max_var -font $smallfont
pack $i.o.f
pack $i.o.f.cb -side top -fill x
pack $i.o.f.fl $i.o.f.fr -side left
pack $i.o.f.fl.l1 $i.o.f.fl.scmin $i.o.f.fr.l2 $i.o.f.fr.scmax -side top -fill x -expand 1

frame $i.c.f 
frame $i.c.f.f 
checkbutton $i.c.f.f.lec -text "Lecture Mode"       -variable lecture_var
checkbutton $i.c.f.f.ext -text "3D Audio Rendering" -variable 3d_audio_var

pack $i.c.f -fill x -side left -expand 1
pack $i.c.f.f 
pack $i.c.f.f.lec -side top  -anchor w
pack $i.c.f.f.ext -side top  -anchor w

# Audio #######################################################################
set i .prefs.pane.audio
frame $i 
frame $i.dd -relief sunken 
pack $i.dd -fill both -expand 1 -anchor w -pady 1

frame $i.dd.device
pack $i.dd.device -side top -fill x

label $i.dd.device.l -text "Audio Device:"
pack  $i.dd.device.l -side top -fill x
menubutton $i.dd.device.mdev -menu $i.dd.device.mdev.menu -indicatoron 1 \
                                -textvariable audio_device -relief raised -width 32
pack $i.dd.device.mdev 
menu $i.dd.device.mdev.menu -tearoff 0

frame $i.dd.sampling  
pack  $i.dd.sampling 

label $i.dd.sampling.l -text "Sampling:"
pack  $i.dd.sampling.l -side top -fill x

menubutton $i.dd.sampling.mfreq -menu $i.dd.sampling.mfreq.menu -indicatoron 1 \
                                -textvariable freq -relief raised -width 6
pack $i.dd.sampling.mfreq -side left
menu $i.dd.sampling.mfreq.menu -tearoff 0

menubutton $i.dd.sampling.mchannels -menu $i.dd.sampling.mchannels.menu -indicatoron 1 \
                                    -textvariable channels -relief raised -width 7
pack $i.dd.sampling.mchannels -side left
menu $i.dd.sampling.mchannels.menu -tearoff 0
set channels Mono

frame $i.cks -relief sunken
pack $i.cks -fill both -expand 1 -anchor w -pady 1
frame $i.cks.f 
frame $i.cks.f.f
checkbutton $i.cks.f.f.silence  -text "Silence Suppression"    -variable silence_var 
checkbutton $i.cks.f.f.agc      -text "Automatic Gain Control" -variable agc_var 
checkbutton $i.cks.f.f.loop     -text "Audio Loopback"         -variable audio_loop_var
checkbutton $i.cks.f.f.suppress -text "Echo Suppression"       -variable echo_var
pack $i.cks.f -fill x -side top -expand 1
pack $i.cks.f.f
pack $i.cks.f.f.silence $i.cks.f.f.agc $i.cks.f.f.loop $i.cks.f.f.suppress -side top -anchor w

# Security Pane ###############################################################
set i .prefs.pane.security
frame $i 
frame $i.a -relief sunken
frame $i.a.f 
frame $i.a.f.f
label $i.a.f.f.l -anchor w -justify left -text "Your communication can be secured with\nDES encryption.  Only conference participants\nwith the same key can receive audio data when\nencryption is enabled."
pack $i.a.f.f.l
pack $i.a -side top -fill both -expand 1 
label $i.a.f.f.lbl -text "Key:"
entry $i.a.f.f.e -width 28 -textvariable key
checkbutton $i.a.f.f.cb -text "Enabled" -variable key_var
pack $i.a.f -fill x -side left -expand 1
pack $i.a.f.f
pack $i.a.f.f.lbl $i.a.f.f.e $i.a.f.f.cb -side left -pady 4 -padx 2 -fill x

# Interface Pane ##############################################################
set i .prefs.pane.interface
frame $i 
frame $i.a -relief sunken 
frame $i.a.f 
frame $i.a.f.f
label $i.a.f.f.l -anchor w -justify left -text "The following features may be\ndisabled to conserve processing\npower."
pack $i.a -side top -fill both -expand 1 
pack $i.a.f -fill x -side left -expand 1
checkbutton $i.a.f.f.power   -text "Powermeters active"       -variable meter_var
checkbutton $i.a.f.f.video   -text "Video synchronization"    -variable sync_var
checkbutton $i.a.f.f.balloon -text "Balloon help"             -variable help_on
checkbutton $i.a.f.f.matrix  -text "Reception quality matrix" -variable matrix_on -command chart_show
checkbutton $i.a.f.f.plist   -text "Participant list"         -variable plist_on  -command toggle_plist
checkbutton $i.a.f.f.fwin    -text "File Control Window"      -variable files_on  -command file_show
pack $i.a.f.f $i.a.f.f.l
pack $i.a.f.f.power $i.a.f.f.video $i.a.f.f.balloon $i.a.f.f.matrix $i.a.f.f.plist $i.a.f.f.fwin -side top -anchor w 

proc set_pane {p base desc} {
    upvar 1 $p pane
    set tpane [string tolower [lindex [split $pane] 0]]
    pack forget $base.$tpane
    set tpane [string tolower [lindex [split $desc] 0]]
    pack $base.$tpane -fill both -expand 1 -padx 2 -pady 2
    set pane $desc
}

proc validate_red_codecs {} {
    # logic assumes codecs are sorted by b/w in menus
    global prenc secenc

    set pri [.prefs.pane.transmission.dd.pri.m.menu index $prenc]
    if { [catch {set sec [.prefs.pane.transmission.cc.red.fc.m.menu index $secenc]}] } {
	set secenc $prienc
    }

    if {$sec <= $pri} {
	for {set i 0} {$i < $pri} {incr i} {
	    .prefs.pane.transmission.cc.red.fc.m.menu entryconfigure $i -state disabled
	}
	set secenc $prenc
    } else {
	for {set i $pri} {$i < $sec} {incr i} {
	    .prefs.pane.transmission.cc.red.fc.m.menu entryconfigure $i -state normal
	}
    }
}

# Initialise "About..." toplevel window
toplevel   .about
frame      .about.rim -relief sunken
frame      .about.m
frame      .about.rim.d
pack .about.m -fill x
pack .about.rim -padx 4 -pady 2 -side top -fill both -expand 1
pack .about.rim.d -side top -fill both -expand 1

frame .about.m.f
label .about.m.f.l -text "Category:"
menubutton .about.m.f.mb -menu .about.m.f.mb.menu -indicatoron 1 -textvariable about_pane -relief raised -width 10
menu .about.m.f.mb.menu -tearoff 0
.about.m.f.mb.menu add command -label "Credits"   -command {set_pane about_pane .about.rim.d "Credits"   }
.about.m.f.mb.menu add command -label "Feedback"  -command {set_pane about_pane .about.rim.d "Feedback"  }
.about.m.f.mb.menu add command -label "Copyright" -command {set_pane about_pane .about.rim.d "Copyright" }

pack .about.m.f 
pack .about.m.f.l .about.m.f.mb -side left

#label     .about.rim.t.d.logo  -highlightthickness 0 -bitmap rat_med 

frame     .about.rim.d.copyright
frame     .about.rim.d.copyright.f
frame     .about.rim.d.copyright.f.f
text      .about.rim.d.copyright.f.f.blurb -height 17 -yscrollcommand ".about.rim.d.copyright.f.f.scroll set"
scrollbar .about.rim.d.copyright.f.f.scroll -command ".about.rim.d.copyright.f.f.blurb yview"

pack      .about.rim.d.copyright.f -expand 1 -fill both
pack      .about.rim.d.copyright.f.f 
pack      .about.rim.d.copyright.f.f.scroll -side right -fill y -expand 1
pack      .about.rim.d.copyright.f.f.blurb -side left -expand 1

frame     .about.rim.d.credits 
frame     .about.rim.d.credits.f -relief sunken 
frame     .about.rim.d.credits.f.f 
pack      .about.rim.d.credits.f -fill both -expand 1
pack      .about.rim.d.credits.f.f -side left -fill x -expand 1
label     .about.rim.d.credits.f.f.1                  -text "The Robust-Audio Tool was developed in the Department of\nComputer Science, University College London.\n\nProject Supervision:"
label     .about.rim.d.credits.f.f.2 -foreground blue -text Good
label     .about.rim.d.credits.f.f.3                  -text "\nCore Development Team:"
label     .about.rim.d.credits.f.f.4 -foreground blue -text Bad
label     .about.rim.d.credits.f.f.5                  -text "Additional Contributions:"
label     .about.rim.d.credits.f.f.6 -foreground blue -text Ugly
for {set i 1} {$i<=6} {incr i} {
    pack  .about.rim.d.credits.f.f.$i -side top -fill x
}

button    .about.dismiss -text Dismiss -command "wm withdraw .about" -highlightthickness 0 -padx 0 -pady 0
pack      .about.dismiss -side bottom -fill x

frame     .about.rim.d.feedback 
frame     .about.rim.d.feedback.f -relief sunken 
frame     .about.rim.d.feedback.f.f
pack      .about.rim.d.feedback.f -fill both -expand 1
pack      .about.rim.d.feedback.f.f -side left -fill x -expand 1
label     .about.rim.d.feedback.f.f.1                  -text "Comments, suggestions, and bug-reports should be sent to:"
label     .about.rim.d.feedback.f.f.2 -foreground blue -text "rat-trap@cs.ucl.ac.uk\n"
label     .about.rim.d.feedback.f.f.3                  -text "Further information is available on the world-wide web at:"
label     .about.rim.d.feedback.f.f.4 -foreground blue -text "http://www-mice.cs.ucl.ac.uk/multimedia/software/rat/\n"
for {set i 1} {$i<=4} {incr i} {
    pack  .about.rim.d.feedback.f.f.$i -side top -fill x
}
#pack .about.rim.t.logo .about.rim.t.blurb .about.rim.t.scroll -side right -fill y

wm withdraw  .about
wm title     .about "About RAT"
wm resizable .about 0 0
set about_pane Copyright
set_pane about_pane .about.rim.d "Credits" 
constrain_window .about "XANDXFITNESSXFORXAXPARTICULARXPURPOSEXAREXDISCLAIMED.XINXNOXEVENTX" 0 20 28 

.about.rim.d.copyright.f.f.blurb insert end {
Copyright (C) 1995-1998 University College London
All rights reserved.

Redistribution and use in source and binary forms, with or
without modification, is permitted, for non-commercial use
only, provided that the following conditions are met:
1. Redistributions of source code must retain the above
   copyright notice, this list of conditions and the
   following disclaimer.
2. Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the
   following disclaimer in the documentation and/or other
   materials provided with the distribution.
3. All advertising materials mentioning features or use of
   this software must display the following acknowledgement:
     "This product includes software developed by the
     Computer Science Department at University College
     London."
4. Neither the name of the University nor of the Department
   may be used to endorse or promote products derived from
   this software without specific prior written permission.
Use of this software for commercial purposes is explicitly
forbidden unless prior written permission is obtained from
the authors. 

THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE.

This software is derived, in part, from publically available
source code with the following copyright:

Copyright (C) 1991-1993 Regents of the University of California
Copyright (C) 1992 Stichting Mathematisch Centrum, Amsterdam
Copyright (C) 1991-1992 RSA Data Security, Inc
Copyright (C) 1992 Jutta Degener and Carsten Bormann, TU Berlin
Copyright (C) 1994 Paul Stewart
Copyright (C) 1996 Regents of the University of California

This product includes software developed by the Computer
Systems Engineering Group and by the Network Research Group
at Lawrence Berkeley Laboratory.

The WB-ADPCM algorithm was developed by British Telecommunications
plc.  Permission has been granted to use it for non-commercial
research and development projects.  BT retain the intellectual
property rights to this algorithm.

Encryption features of this software use the RSA Data
Security, Inc. MD5 Message-Digest Algorithm.
}

proc shuffle_rats {args} {
    # This should really animate the movement and play fruit-machine sounds.... :-)
    set r ""
    set end [llength $args]
    set l 0 
    while { $l < $end } {
	set toget [expr abs([clock clicks]) % [llength $args]]
	set r [format "%s%s  " $r [lindex $args $toget]]
	set args [lreplace $args $toget $toget]
	lappend used $toget
	if {$l >0 && [expr ($l + 1) % 3] == 0} {
	    set r "$r\n"
	}
	incr l
    }
    return $r
}

proc jiggle_credits {} {
# Software really developed by the Socialist Department of Computer Science
    .about.rim.d.credits.f.f.2 configure -text [shuffle_rats "Angela Sasse" "Vicky Hardman"]
    .about.rim.d.credits.f.f.4 configure -text [shuffle_rats "Colin Perkins" "Isidor Kouvelas" "Orion Hodson"]
    .about.rim.d.credits.f.f.6 configure -text [shuffle_rats "Darren Harris"  "Anna Watson" "Mark Handley" "Jon Crowcroft" "Anna Bouch" "Marcus Iken" "Kris Hasler"]
}

proc sync_ui_to_engine {} {
    # the next time the display is shown, it needs to reflect the
    # state of the audio engine.
    mbus_send "R" "tool.rat.settings" ""
}

proc sync_engine_to_ui {} {
    # make audio engine concur with ui
    global my_cname rtcp_name rtcp_email rtcp_phone rtcp_loc 
    global prenc upp channel_var secenc red_off int_gap int_units
    global silence_var agc_var audio_loop_var echo_var
    global repair_var limit_var min_var max_var lecture_var 3d_audio_var convert_var  
    global meter_var sync_var gain volume input_port output_port 
    global in_mute_var out_mute_var channels freq key key_var
    global audio_device

    set my_cname_enc [mbus_encode_str $my_cname]
    #rtcp details
    mbus_send "R" "rtp.source.name"  "$my_cname_enc [mbus_encode_str $rtcp_name]"
    mbus_send "R" "rtp.source.email" "$my_cname_enc [mbus_encode_str $rtcp_email]"
    mbus_send "R" "rtp.source.phone" "$my_cname_enc [mbus_encode_str $rtcp_phone]"
    mbus_send "R" "rtp.source.loc"   "$my_cname_enc [mbus_encode_str $rtcp_loc]"
    
    #transmission details
    mbus_send "R" "tool.rat.codec"      "[mbus_encode_str $prenc] [mbus_encode_str $channels] [mbus_encode_str $freq]"
    mbus_send "R" "tool.rat.rate"         $upp

    switch $channel_var {
    	none        {mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var]"}
	redundant   {mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var] [mbus_encode_str $secenc] $red_off"}
	interleaved {mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var] $int_gap $int_units"}
    	*           {error "unknown channel coding scheme $channel_var"}
    }

    mbus_send "R" "tool.rat.silence"       $silence_var
    mbus_send "R" "tool.rat.agc"           $agc_var
    mbus_send "R" "tool.rat.loopback"      $audio_loop_var
    mbus_send "R" "tool.rat.echo.suppress" $echo_var

    #Reception Options
    mbus_send "R" "audio.channel.repair"   [mbus_encode_str $repair_var]
    mbus_send "R" "tool.rat.playout.limit" $limit_var
    mbus_send "R" "tool.rat.playout.min"   $min_var
    mbus_send "R" "tool.rat.playout.max"   $max_var
    mbus_send "R" "tool.rat.lecture"       $lecture_var
    mbus_send "R" "tool.rat.3d.enabled"   $3d_audio_var
    mbus_send "R" "tool.rat.converter"     [mbus_encode_str $convert_var]

    #Security
    if {$key_var==1 && [string length $key]!=0} {
	mbus_send "R" "security.encryption.key" [mbus_encode_str $key]
    } else {
	mbus_send "R" "security.encryption.key" [mbus_encode_str ""]
    }

    #Interface
    mbus_send "R" "tool.rat.powermeter"   $meter_var
    mbus_send "R" "tool.rat.sync"         $sync_var

    #device 
    mbus_send "R" "audio.device"        [mbus_encode_str "$audio_device"]
    mbus_send "R" "audio.input.gain"    $gain
    mbus_send "R" "audio.output.gain"   $volume
    mbus_send "R" "audio.input.port"    [mbus_encode_str $input_port]
    mbus_send "R" "audio.output.port"   [mbus_encode_str $output_port]
    mbus_send "R" "audio.input.mute"    $in_mute_var
    mbus_send "R" "audio.output.mute"   $out_mute_var
}

if {$win32 == 0 && [glob ~] == "/"} {
	set rtpfname /.RTPdefaults
} else {
	set rtpfname ~/.RTPdefaults
}

proc save_setting {f field var} {
    global win32 V rtpfname
    upvar #0 $var value
    if {$win32 == 0} {
		catch { puts $f "*$field: $value" }
    } else {
		if {[string first "rtp" "$field"] == -1} {
			set fail [catch {registry set "HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app)" "*$field" "$value"} errmsg ]
		} else {
			set fail [catch {registry set "HKEY_CURRENT_USER\\Software\\$V(class)\\common" "*$field" "$value"} errmsg ]
		}
		if {$fail} {
			puts "registry set failed:\n$errmsg"
		}
	}
}

proc save_settings {} {
    global rtpfname win32 TOOL my_cname

    set f 0
    if {$win32 == 0} {
		set fail [catch {set f [open $rtpfname w]}]
		if {$fail} {
			return
		}
    }

    # personal
    save_setting $f rtpName     rtcp_name
    save_setting $f rtpEmail    rtcp_email
    save_setting $f rtpPhone    rtcp_phone
    save_setting $f rtpLoc      rtcp_loc
    save_setting $f audioTool   TOOL($my_cname)

    # transmission
    save_setting $f audioFrequency         freq
    save_setting $f audioChannels          channels
    save_setting $f audioPrimary           prenc
    save_setting $f audioUnits             upp
    save_setting $f audioChannelCoding     channel_var
    save_setting $f audioRedundancy        secenc
    save_setting $f audioRedundancyOffset  red_off
    save_setting $f audioInterleavingGap   int_gap
    save_setting $f audioInterleavingUnits int_units 
    save_setting $f audioSilence           silence_var
    save_setting $f audioAGC               agc_var
    save_setting $f audioLoopback          audio_loop_var
    save_setting $f audioEchoSuppress      echo_var
    # reception
    save_setting $f audioRepair           repair_var
    save_setting $f audioLimitPlayout     limit_var
    save_setting $f audioMinPlayout       min_var
    save_setting $f audioMaxPlayout       max_var
    save_setting $f audioLecture          lecture_var
    save_setting $f audio3dRendering      3d_audio_var
    save_setting $f audioAutoConvert      convert_var
    #security
   
    # ui bits
    save_setting $f audioPowermeters meter_var
    save_setting $f audioLipSync     sync_var
    save_setting $f audioHelpOn      help_on
    save_setting $f audioMatrixOn    matrix_on
    save_setting $f audioPlistOn     plist_on
    save_setting $f audioFilesOn     files_on

    # device 
	save_setting $f  audioDevice       audio_device
    save_setting $f  audioOutputGain   volume
    save_setting $f  audioInputGain    gain
    save_setting $f  audioOutputPort   output_port
    save_setting $f  audioInputPort    input_port

    if {$win32 == 0} {
	    close $f
    }
}

proc load_setting {attrname field var default} {
    global V win32 $var
    upvar 1 $attrname attr 
	set fail 0
    # who has the tcl manual? is the only way to pass arrays thru upvar...
	if {$win32} {
		if {[string first "rtp" "$field"] == -1} {
			set fail [ catch { set tmp "[registry get HKEY_CURRENT_USER\\Software\\$V(class)\\$V(app) *$field]" } msg ]
		} else {
			set fail [ catch { set tmp "[registry get HKEY_CURRENT_USER\\Software\\$V(class)\\common  *$field]" } msg ]
		}
		if {$fail} {
			puts "Failed to get $field reason $msg\n";
			set tmp ""
		} else {
			puts "$field $var $tmp"
		}
    } else {
		set tmp [option get . $field rat]
		if {$tmp == ""} {
	    # not in xresources...
			if {[info exists attr($field)]} {
			set tmp $attr($field)
			}
		}
    }
    if {$tmp == ""} {
	# either not in rtp defaults, or registry...
        set tmp $default
    }
    set $var $tmp
}

proc tool_version {tool} {
	if {![regexp {RAT v[0-9]+\.[0-9]+\.[0-9]+ [a-zA-Z]+} $tool]} {
		# Unknown tool version, so put out something which won't
		# match with the current version string to force a reset
		# of the saved parameters.
		return "RAT v0.0.0"
	}
	regsub {(RAT v[0-9]+\.[0-9]+\.[0-9]+) [a-zA-Z]+} $tool {\1} v
	return $v
}

proc load_settings {} {
    global rtpfname win32 my_cname TOOL

    set attr(zero) ""
    if {$win32 == 0} {
		if {[file readable $rtpfname]} {
			set f [open $rtpfname]
			while {[eof $f] == 0} {
			gets $f line
			set field [string trim [lindex $line 0] "*:"]
			set value [lrange $line 1 end]
			set attr($field) "$value"
			}	
			close $f
		}
    }

    # personal
    load_setting attr rtpName   rtcp_name     "unknown"
    load_setting attr rtpEmail  rtcp_email    "unknown"
    load_setting attr rtpPhone  rtcp_phone    ""
    load_setting attr rtpLoc    rtcp_loc      ""
    load_setting attr audioTool audio_tool         ""

    # If the version of the saved settings is different 
    # from those the current version use defaults.
    global audio_tool
    if {[tool_version $audio_tool] != [tool_version $TOOL($my_cname)]} {
	foreach i [array names attr audio*] {
	    unset attr($i)
	}
    }
    unset audio_tool

    # reception
    load_setting attr audioRepair       repair_var    "Repetition"

    load_setting attr audioLimitPlayout limit_var     "0"
    load_setting attr audioMinPlayout   min_var       "0"
    load_setting attr audioMaxPlayout   max_var       "2000"
    load_setting attr audioLecture      lecture_var   "0"
    load_setting attr audio3dRendering  3d_audio_var   "0"
    load_setting attr audioAutoConvert  convert_var   "None"
    #security
   
    # ui bits
    load_setting attr audioPowermeters  meter_var     "1"
    load_setting attr audioLipSync      sync_var      "0"
    load_setting attr audioHelpOn       help_on       "1"
    load_setting attr audioMatrixOn     matrix_on     "0"
    load_setting attr audioPlistOn      plist_on      "1"
    load_setting attr audioFilesOn      files_on      "0"

    # device config
    load_setting attr audioOutputGain   volume       "50"
    load_setting attr audioInputGain    gain         "50"
    load_setting attr audioOutputPort   output_port  "speaker"
    load_setting attr audioInputPort    input_port   "microphone"
    # we don't save the following but we can set them so if people
    # want to start with mic open then they add following attributes
    load_setting attr audioOutputMute   out_mute_var "0"
    load_setting attr audioInputMute    in_mute_var  "1"
    # transmission
    load_setting attr audioSilence           silence_var   "1"
    load_setting attr audioAGC               agc_var       "0"
    load_setting attr audioLoopback          audio_loop_var "0"
    load_setting attr audioEchoSuppress      echo_var      "0"
    load_setting attr audioFrequency         freq          "8-kHz"
    load_setting attr audioChannels          channels      "Mono"
    load_setting attr audioPrimary           prenc         "GSM"
    load_setting attr audioUnits             upp           "2"
    load_setting attr audioChannelCoding     channel_var   "none"
    load_setting attr audioRedundancy        secenc        "GSM"
    load_setting attr audioRedundancyOffset  red_off       "1"
    load_setting attr audioInterleavingGap   int_gap       "4"
    load_setting attr audioInterleavingUnits int_units     "4"
	#device
	load_setting attr audioDevice            audio_device  "Unknown"
	
	global prenc channels freq
	mbus_send "R" "tool.rat.codec" "[mbus_encode_str $prenc] [mbus_encode_str $channels] [mbus_encode_str $freq]"
	
	global      in_mute_var   out_mute_var
	input_mute  $in_mute_var
	output_mute $out_mute_var
}

proc check_rtcp_name {} {
    global rtcp_name
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
	button .name.d -highlightthickness 0 -padx 0 -pady 0 -text Done -command {save_settings; sync_engine_to_ui; destroy .name}
	bind   .name.b.e <Return> {save_settings; sync_engine_to_ui; destroy .name}
	
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
button    .chart.d  -text "Dismiss"    -command {set matrix_on 0; chart_show} -padx 0 -pady 0

pack .chart.d  -side bottom -fill x    -expand 0 -anchor s
pack .chart.sb -side bottom -fill x    -expand 0 -anchor s
pack .chart.sr -side right  -fill y    -expand 0 -anchor e
pack .chart.c  -side left   -fill both -expand 1 -anchor n

# Add a few labels to the chart...
.chart.c create text 2 [expr ($chart_boxsize / 2) + 2] -anchor w -text "Sender:" -font $chart_font 
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

proc chart_set {dest srce val} {
  global INDEX chart_size chart_boxsize chart_xoffset chart_yoffset

  if {[array names INDEX $srce] != [list $srce]} {
    return
  }
  if {[array names INDEX $dest] != [list $dest]} {
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

proc chart_label {cname} {
  global CNAME NAME INDEX chart_size chart_boxsize chart_xoffset chart_yoffset chart_font

  set pos $INDEX($cname)
  set val $NAME($cname)
  if {[string length $val] == 0} {
    set val $CNAME($cname)
  }

  if {($pos > $chart_size) || ($pos < 0)} return

  set ypos [expr $chart_yoffset + ($chart_boxsize * $pos) + ($chart_boxsize / 2)]
  .chart.c delete cname_$cname
  .chart.c create text 2 $ypos -text [string range $val 0 25] -anchor w -font $chart_font -tag cname_$cname
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

proc chart_show {} {
	global matrix_on
	if {$matrix_on} {
		wm deiconify .chart
	} else {
		wm withdraw .chart
	}
}

wm withdraw .chart
wm title    .chart "Reception quality matrix"
wm geometry .chart 320x200

chart_enlarge 1

#
# End of RTCP RR chart routines
#

#
# File Control Window 
#

catch {
	toplevel .file

	frame .file.play
	frame .file.rec
	pack  .file.play -side left
	pack  .file.rec  -side right

	label .file.play.l -text "Playback"
	pack  .file.play.l -side top -fill x
	label .file.rec.l -text "Record"
	pack  .file.rec.l -side top -fill x

	wm withdraw .file
	wm title	.file "RAT File Control"

	foreach action { play rec } {
		frame  .file.$action.buttons
		pack   .file.$action.buttons
		button .file.$action.buttons.disk -bitmap disk -command "fileDialog $action"
		pack   .file.$action.buttons.disk -side left
		foreach cmd "$action pause stop" {
			button .file.$action.buttons.$cmd -bitmap $cmd -state disabled -command file_$action\_$cmd
			pack   .file.$action.buttons.$cmd -side left
		}
	}
} fwinerr

if {$fwinerr != {}} {
	puts stderr $fwinerr
}

proc fileDialog {cmdbox} {
    global win32 tcl_platform
    
	set defaultExtension au
	set defaultLocation  .

    switch -glob $tcl_platform(os) {
	SunOS    { 
		if [file exists /usr/demo/SOUND/sounds] { set defaultLocation /usr/demo/SOUND/sounds }
		}
	Windows* { 
		if [file exists C:/Windows/Media]       { set defaultLocation C:/Windows/Media }
		set defaultExtension wav
		}
	}
    
    set types {
		{"NeXT/Sun Audio files"	"au"}
		{"Microsoft RIFF files"	"wav"}
		{"All files"		"*"}
    }
    
    if {![string compare $cmdbox "play"]} {
		catch { asFileBox .playfilebox  -defaultextension $defaultExtension -command file_open_$cmdbox -directory $defaultLocation -extensions $types } asferror
    } else {
		catch { asFileBox .recfilebox   -defaultextension $defaultExtension -command file_open_$cmdbox  -extensions $types -force_extension 1 } asferror
    }
	
	if {$asferror != ""} {
		puts stderr asferror
	}
}

proc file_show {} {
    global files_on
    
    if {$files_on} {
	 	wm deiconify .file
    } else {
 		wm withdraw .file
    }
}

proc file_play_live {} {
# Request heart beat to determine if file is valid
	mbus_send "R" audio.file.play.live ""
}

proc file_rec_live {} {
# Request heart beat to determine if file is valid
	mbus_send "R" audio.file.record.live ""
}

proc file_open_play {path} {
    global play_file

    mbus_send "R" "audio.file.play.open" [mbus_encode_str $path]
    mbus_send "R" "audio.file.play.pause" 1
    set play_file(state) paused
    set play_file(name) $path
    
    # Test whether file is still playing/valid
    after 200 file_play_live
}

proc file_open_rec {path} {
    global rec_file

    mbus_send "R" "audio.file.record.open" [mbus_encode_str $path]
    mbus_send "R" "audio.file.record.pause" 1

    set rec_file(state) paused
    set rec_file(name)  $path

    # Test whether file is still recording/valid
    after 200 file_rec_live
}

proc file_enable_play { } {
	.file.play.buttons.play   configure -state normal
	.file.play.buttons.pause  configure -state disabled
	.file.play.buttons.stop   configure -state disabled
}	

proc file_enable_record { } {
	.file.rec.buttons.rec configure -state normal
	.file.rec.buttons.pause  configure -state disabled
	.file.rec.buttons.stop   configure -state disabled
}

proc file_play_play {} {
	global play_file
	
	catch {
		puts stderr $play_file(state)
		if {$play_file(state) == "paused"} {
			mbus_send "R" "audio.file.play.pause" 0
			puts stderr "unpaused"
		} else {
			mbus_send "R" "audio.file.play.open" [mbus_encode_str $play_file(name)]
			puts stderr "re-opening"
		}
		set play_file(state) play
	} pferr

	if { $pferr != "play" } { puts stderr "pferr: $pferr" }

	.file.play.buttons.play   configure -state disabled
	.file.play.buttons.pause  configure -state normal
	.file.play.buttons.stop   configure -state normal
	
	after 200 file_play_live
}

proc file_play_pause {} {
	global play_file
	
	.file.play.buttons.play   configure -state normal
	.file.play.buttons.pause  configure -state disabled
	.file.play.buttons.stop   configure -state normal

	set play_file(state) paused
	mbus_send "R" "audio.file.play.pause" 1
}

proc file_play_stop {} {
	global play_file
	
	set play_file(state) end
	file_enable_play
	mbus_send "R" "audio.file.play.stop" ""
}

proc file_rec_rec {} {
	global rec_file
	
	catch {
		puts stderr $rec_file(state)
		if {$rec_file(state) == "paused"} {
			mbus_send "R" "audio.file.record.pause" 0
		} else {
			mbus_send "R" "audio.file.record.open" [mbus_encode_str $rec_file(name)]
		}
		set rec_file(state) record
	} prerr

	if { $prerr != "record" } { puts stderr "prerr: $prerr" }

	.file.rec.buttons.rec    configure -state disabled
	.file.rec.buttons.pause  configure -state normal
	.file.rec.buttons.stop   configure -state normal

	after 200 file_rec_live
}

proc file_rec_pause {} {
	global rec_file

	.file.rec.buttons.rec    configure -state normal
	.file.rec.buttons.pause  configure -state disabled
	.file.rec.buttons.stop   configure -state normal

	set rec_file(state) paused
	mbus_send "R" "audio.file.rec.pause" 1
}

proc file_rec_stop {} {
	global rec_file
	
	set rec_file(state) end
	file_enable_record
	mbus_send "R" "audio.file.record.stop" ""
}

#
# End of File Window routines
#

bind . <t> {mbus_send "R" "toggle.input.port" ""} 

proc show_help {window} {
	global help_text help_on help_id
	if {$help_on} {
		.help.text  configure -text $help_text($window)
		# Beware! Don't put the popup under the cursor! Else we get window enter
		# for .help and leave for $window, making us hide_help which removes the
		# help window, giving us a window enter for $window making us popup the
		# help again.....
		if {[winfo width $window] > [winfo height $window]} {
		    set xpos [expr [winfo pointerx $window] + 10]
		    set ypos [expr [winfo rooty    $window] + [winfo height $window] + 4]
		} else {
		    set xpos [expr [winfo rootx    $window] + [winfo width $window] + 4]
		    set ypos [expr [winfo pointery $window] + 10]
		}
		
		wm geometry  .help +$xpos+$ypos
		set help_id [after 100 wm deiconify .help]
		raise .help $window
	}
}

proc hide_help {window} {
	global help_id help_on
	if {[info exists help_id]} { 
		after cancel $help_id
	}
	wm withdraw .help
}

proc add_help {window text} {
	global help_text 
	set help_text($window)  $text
	bind $window <Enter>    "+show_help $window"
	bind $window <Leave>    "+hide_help $window"
}

bind Entry   <KeyPress> "+hide_help %W"
toplevel .help       -bg black
label    .help.text  -bg lavender -justify left
pack .help.text -side top -anchor w -fill x
wm transient        .help .
wm withdraw         .help
wm overrideredirect .help true

add_help .r.c.gain.s2 	"This slider controls the volume\nof the sound you send."
add_help .r.c.gain.l2 	"Click to change input device."
add_help .r.c.gain.t2 	"If this button is not pushed in, you are are transmitting,\nand may be\
                         heard by other participants. Holding down the\nright mouse button in\
			 any RAT window will temporarily\ntoggle the state of this button,\
			 allowing for easy\npush-to-talk operation."
add_help .r.c.gain.b2 	"Indicates the loudness of the\nsound you are sending. If this\nis\
                         moving, you may be heard by\nthe other participants."

add_help .r.c.vol.s1  	"This slider controls the volume\nof the sound you hear."
add_help .r.c.vol.l1  	"Click to change output device."
add_help .r.c.vol.t1  	"If pushed in, reception is muted."
add_help .r.c.vol.b1  	"Indicates the loudness of the\nsound you are hearing."

add_help .l.f		"Name of the session, and the IP address, port\n&\
		 	 TTL used to transmit the audio data."
add_help .l.t		"The participants in this session with you at the top.\nClick on a name\
                         with the left mouse button to display\ninformation on that participant,\
			 and with the middle\nbutton to mute that participant (the right button\nwill\
			 toggle the transmission mute button, as usual)."

add_help .l.s1.opts   	"Brings up another window allowing\nthe control of various options."
add_help .l.s1.about  	"Brings up another window displaying\ncopyright & author information."
add_help .l.s1.quit   	"Press to leave the session."

# preferences help
add_help .prefs.m.f.m  "Click here to change the preference\ncategory."
set i .prefs.buttons
add_help $i.bye         "Cancel changes."
add_help $i.apply       "Apply changes."
add_help $i.save        "Save and apply changes."

# user help
set i .prefs.pane.personal.a.f.f.ents
add_help $i.name      	"Enter your name for transmission\nto other participants."
add_help $i.email      	"Enter your email address for transmission\nto other participants."
add_help $i.phone     	"Enter your phone number for transmission\nto other participants."
add_help $i.loc      	"Enter your location for transmission\nto other participants."

#audio help
set i .prefs.pane.audio
add_help $i.dd.device.mdev "Selects preferred audio device."
add_help $i.dd.sampling.mfreq \
                        "Sets the sampling rate of the audio device.\nThis changes the available codecs."
add_help $i.dd.sampling.mchannels \
                        "Changes between mono and stereo sampling."
add_help $i.cks.f.f.silence\
			 "Prevents silence from being transmitted when the speaker is silent\n\
                          and the input is unmuted."
add_help $i.cks.f.f.agc	 "Enables automatic control of the volume\nof the sound you send."
add_help $i.cks.f.f.loop "Enables hardware for loopback of audio input."
add_help $i.cks.f.f.suppress \
                         "Mutes microphone when playing audio."

# transmission help
set i .prefs.pane.transmission

add_help $i.dd.units.m	"Sets the duration of each packet sent.\nThere is a fixed per-packet\
                         overhead, so\nmaking this larger will reduce the total\noverhead.\
			 The effects of packet loss are\nmore noticable with large packets."
add_help $i.dd.pri.m	"Changes the primary audio compression\nscheme. The list is arranged\
                         with high-\nquality, high-bandwidth choices at the\ntop, and\
			 poor-quality, lower-bandwidth\nchoices at the bottom."
add_help $i.cc.van.rb	"Sets no channel coding."
add_help $i.cc.red.rb	"Piggybacks earlier units of audio into packets\n\
			 to protect against packet loss. Some audio\n\
			 tools (eg: vat-4.0) are not able to receive\n\
			 audio sent with this option."
add_help $i.cc.red.fc.m \
			"Sets the format of the piggybacked data."
add_help $i.cc.red.u.m \
			"Sets the offset of the piggybacked data."
add_help $i.cc.int.fc.m \
			"Sets the separation of adjacent units within\neach packet. Larger values correspond\
			 to longer\ndelays."
add_help $i.cc.int.zz.m "Number of compound units per packet."
add_help $i.cc.int.rb	"Enables interleaving which exchanges latency\n\
			 for protection against burst losses.  No other\n\
			 audio tools can decode this format (experimental)."

# Reception Help
set i .prefs.pane.reception
add_help $i.r.m 	"Sets the type of repair applied when packets are\nlost. The schemes\
			 are listed in order of increasing\ncomplexity and quality of repair."
add_help $i.r.ms        "Sets the type of sample rate conversion algorithm\n\
			 that will be applied to streams that differ in rate\n\
			 to the audio device rate."
add_help $i.o.f.cb      "Enforce playout delay limits set below.\nThis is not usually desirable."
add_help $i.o.f.fl.scmin   "Sets the minimum playout delay that will be\napplied to incoming\
			 audio streams."
add_help $i.o.f.fr.scmax   "Sets the maximum playout delay that will be\napplied to incoming\
			 audio streams."
add_help $i.c.f.f.lec  	"If enabled, extra delay is added at both sender and receiver.\nThis allows\
                         the receiver to better cope with host scheduling\nproblems, and the sender\
			 to perform better silence suppression.\nAs the name suggests, this option\
			 is intended for scenarios such\nas transmitting a lecture, where interactivity\
			 is less important\nthan quality."

# security...too obvious for balloon help!
add_help .prefs.pane.security.a.f.f.e "Due to government export restrictions\nhelp\
				       for this option is not available."

# interface...ditto!
set i .prefs.pane.interface
add_help $i.a.f.f.power "Disable display of audio powermeters. This\nis only\
		 	 useful if you have a slow machine"
add_help $i.a.f.f.video	"Enable lip-synchronisation, if\nyou\
			 have a compatible video tool"
add_help $i.a.f.f.balloon "If you can see this, balloon help\nis enabled. If not, it isn't."
add_help $i.a.f.f.matrix  "Displays a chart showing the reception\nquality reported by all participants"
add_help $i.a.f.f.plist   "Hides the list of participants"

add_help .chart		"This chart displays the reception quality reported\n by all session\
                         participants. Looking along a row\n gives the quality with which that\
			 participant was\n received by all other participants in the session:\n green\
			 is good quality, orange medium quality, and\n red poor quality audio."

