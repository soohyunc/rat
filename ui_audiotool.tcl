catch {
#
# Copyright (c) 1995-99 University College London
# All rights reserved.
#
# $Revision$
# 
# Full terms and conditions of the copyright appear below.
#

#wm withdraw .

if {[string compare [info commands registry] "registry"] == 0} {
	set win32 1
} else {
	set win32 0
	option add *Menu*selectColor 		forestgreen
	option add *Radiobutton*selectColor 	forestgreen
	option add *Checkbutton*selectColor 	forestgreen
	option add *Entry.background 		gray70
}

set statsfont     [font actual {helvetica 10}]
set titlefont     [font actual {helvetica 10}]
set infofont      [font actual {helvetica 10}]
set smallfont     [font actual {helvetica  8}]
set verysmallfont [font actual {helvetica  8}]

set speaker_highlight white

option add *Entry.relief       sunken 
option add *borderWidth        1
option add *highlightThickness 0
#option add *Button*padX        4          
#option add *Button*padY        0          
option add *font               $infofont
option add *Menu*tearOff       0

set V(class) "Mbone Applications"
set V(app)   "rat"

set iht			16
set iwd 		250
set cancel_info_timer 	0
set num_ssrc		0
set fw			.l.t.list.f
set input_ports         [list]
set output_ports        [list]

proc init_source {ssrc} {
	global CNAME NAME EMAIL LOC PHONE TOOL NOTE SSRC num_ssrc 
	global CODEC DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP JITTER \
		LOSS_TO_ME LOSS_FROM_ME INDEX JIT_TOGED BUFFER_SIZE PLAYOUT_DELAY \
		GAIN MUTE

	# This is a debugging test -- old versions of the mbus used the
	# cname to identify participants, whilst the newer version uses 
	# the ssrc.  This check detects if old style commands are being
	# used and raises an error if so.
	if [regexp {.*@[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+} "$ssrc"] {
		error "ssrc $ssrc invalid"
	}

	if {[array names INDEX $ssrc] != [list $ssrc]} {
		# This is a source we've not seen before...
		set         CNAME($ssrc) ""
		set          NAME($ssrc) $ssrc
		set         EMAIL($ssrc) ""
		set         PHONE($ssrc) ""
		set           LOC($ssrc) ""
		set          TOOL($ssrc) ""
		set 	     NOTE($ssrc) ""
		set         CODEC($ssrc) unknown
		set          GAIN($ssrc) 1.0
		set          MUTE($ssrc) 0
		set      DURATION($ssrc) ""
                set   BUFFER_SIZE($ssrc) 0
                set PLAYOUT_DELAY($ssrc) 0
		set    PCKTS_RECV($ssrc) 0
		set    PCKTS_LOST($ssrc) 0
		set    PCKTS_MISO($ssrc) 0
		set     PCKTS_DUP($ssrc) 0
		set        JITTER($ssrc) 0
		set     JIT_TOGED($ssrc) 0
		set    LOSS_TO_ME($ssrc) 0
		set  LOSS_FROM_ME($ssrc) 0
		set  HEARD_LOSS_TO_ME($ssrc) 0
		set  HEARD_LOSS_FROM_ME($ssrc) 0
		set         INDEX($ssrc) $num_ssrc
		set          SSRC($ssrc) $ssrc
		incr num_ssrc
		chart_enlarge $num_ssrc 
		chart_label   $ssrc
	}
}

proc window_plist {ssrc} {
    	global fw
	regsub -all {@|\.} $ssrc {-} foo
	return $fw.source-$foo
}

proc window_stats {ssrc} {
	regsub -all {[\. ]} $ssrc {-} foo
	return .stats$foo
}

# Commands to send message over the conference bus...
proc output_mute {state} {
    mbus_send "R" "audio.output.mute" "$state"
    bargraphState .r.c.vol.gra.b1 [expr ! $state]
}

proc input_mute {state} {
    mbus_send "R" "audio.input.mute" "$state"
    bargraphState .r.c.gain.gra.b2 [expr ! $state]
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
    global input_port input_ports

    set len [llength $input_ports]
# lsearch returns -1 if not found, index otherwise
    set idx [lsearch -exact $input_ports $input_port] 

    if {$idx != -1} {
	incr idx
	set idx [expr $idx % $len]
	set port [lindex $input_ports $idx]
	mbus_send "R" "audio.input.port" [mbus_encode_str $port]
    }
}

proc toggle_output_port {} {
    global output_port output_ports

    set len [llength $output_ports]
# lsearch returns -1 if not found, index otherwise
    set idx [lsearch -exact $output_ports $output_port] 
    
    if {$idx != -1} {
	incr idx
	set idx [expr $idx % $len]
	set port [lindex $output_ports $idx]
	mbus_send "R" "audio.output.port" [mbus_encode_str $port]
    }
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
		tool.rat.load.setting           {eval mbus_recv_tool.rat.load.setting $args}
		tool.rat.sampling.supported 	{eval mbus_recv_tool.rat.sampling.supported $args}
		tool.rat.converter  		{eval mbus_recv_tool.rat.converter $args}
		tool.rat.converters.flush 	{eval mbus_recv_tool.rat.converters.flush $args}
		tool.rat.converters.add 	{eval mbus_recv_tool.rat.converters.add $args}
		tool.rat.repair			{eval mbus_recv_tool.rat.repair $args}
		tool.rat.repairs.flush		{eval mbus_recv_tool.rat.repairs.flush $args}
		tool.rat.repairs.add		{eval mbus_recv_tool.rat.repairs.add $args}
		tool.rat.powermeter		{eval mbus_recv_tool.rat.powermeter $args}
		tool.rat.agc  			{eval mbus_recv_tool.rat.agc $args}
		tool.rat.sync  			{eval mbus_recv_tool.rat.sync $args}
		tool.rat.format.in              {eval mbus_recv_tool.rat.format.in $args}
		tool.rat.codec  		{eval mbus_recv_tool.rat.codec $args}
		tool.rat.codec.details          {eval mbus_recv_tool.rat.codec.details $args}
		tool.rat.rate  			{eval mbus_recv_tool.rat.rate $args}
		tool.rat.lecture.mode  		{eval mbus_recv_tool.rat.lecture.mode $args}
		tool.rat.disable.audio.ctls  	{eval mbus_recv_tool.rat.disable.audio.ctls $args}
		tool.rat.enable.audio.ctls  	{eval mbus_recv_tool.rat.enable.audio.ctls $args}
		tool.rat.audio.buffered  	{eval mbus_recv_tool.rat.audio.buffered $args}
		tool.rat.audio.delay    	{eval mbus_recv_tool.rat.audio.delay $args}
		tool.rat.3d.enabled  		{eval mbus_recv_tool.rat.3d.enabled $args}
		tool.rat.3d.azimuth.min         {eval mbus_recv_tool.rat.3d.azimuth.min $args}
		tool.rat.3d.azimuth.max         {eval mbus_recv_tool.rat.3d.azimuth.max $args}
		tool.rat.3d.filter.types        {eval mbus_recv_tool.rat.3d.filter.types   $args}
		tool.rat.3d.filter.lengths      {eval mbus_recv_tool.rat.3d.filter.lengths $args}
		tool.rat.3d.user.settings       {eval mbus_recv_tool.rat.3d.user.settings  $args}
		audio.suppress.silence  	{eval mbus_recv_audio.suppress.silence $args}
		audio.channel.coding  		{eval mbus_recv_audio.channel.coding $args}
		audio.channel.repair 		{eval mbus_recv_audio.channel.repair $args}
		audio.devices.flush             {eval mbus_recv_audio_devices_flush $args}
		audio.devices.add               {eval mbus_recv_audio_devices_add $args}
		audio.device                    {eval mbus_recv_audio_device $args}
		audio.input.gain  		{eval mbus_recv_audio.input.gain $args}
		audio.input.port  		{eval mbus_recv_audio.input.port $args}
		audio.input.ports.add           {eval mbus_recv_audio.input.ports.add $args}
		audio.input.ports.flush         {eval mbus_recv_audio.input.ports.flush $args}
		audio.input.mute  		{eval mbus_recv_audio.input.mute $args}
		audio.input.powermeter  	{eval mbus_recv_audio.input.powermeter $args}
		audio.output.gain  		{eval mbus_recv_audio.output.gain $args}
		audio.output.port  		{eval mbus_recv_audio.output.port $args}
		audio.output.ports.add          {eval mbus_recv_audio.output.ports.add $args}
		audio.output.ports.flush        {eval mbus_recv_audio.output.ports.flush $args}
		audio.output.mute  		{eval mbus_recv_audio.output.mute $args}
		audio.output.powermeter  	{eval mbus_recv_audio.output.powermeter $args}
		audio.file.play.ready   	{eval mbus_recv_audio.file.play.ready   $args}
		audio.file.play.alive   	{eval mbus_recv_audio.file.play.alive $args}
		audio.file.record.ready 	{eval mbus_recv_audio.file.record.ready $args}
		audio.file.record.alive 	{eval mbus_recv_audio.file.record.alive $args}
		session.title  			{eval mbus_recv_session.title $args}
		session.address  		{eval mbus_recv_session.address $args}
		rtp.ssrc  			{eval mbus_recv_rtp.ssrc $args}
		rtp.source.exists  		{eval mbus_recv_rtp.source.exists $args}
		rtp.source.remove  		{eval mbus_recv_rtp.source.remove $args}
		rtp.source.cname  		{eval mbus_recv_rtp.source.cname $args}
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
		rtp.source.gain  		{eval mbus_recv_rtp.source.gain $args}
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

proc mbus_recv_tool.rat.load.setting {sname} {
    global attr
# Note when settings get loaded the get set in attr and not updated.  So we
# use this as a cache for desired values.  This is necessary as when the
# settings first get loaded we have null audio device and can't set 
# anything on it meaningfully - i.e. it only has 1 port for input and 1 for output.
#
    switch $sname {
	audio.input.mute  { mbus_send "R" $sname $attr(audioInputMute) }
	audio.input.gain  { mbus_send "R" $sname $attr(audioInputGain) }
	audio.input.port  { mbus_send "R" $sname [mbus_encode_str $attr(audioInputPort)] }
	audio.output.mute { mbus_send "R" $sname $attr(audioOutputMute) }
	audio.output.gain { mbus_send "R" $sname $attr(audioOutputGain) }
	audio.output.port { mbus_send "R" $sname [mbus_encode_str $attr(audioOutputPort)] }
	default           { puts "setting requested has no handler"}
    }
}

proc change_freq {new_freq} {
    global freq

    if {$freq != $new_freq} {
	set freq $new_freq
	update_channels_displayed
	update_codecs_displayed
	reset_encodings
    }
}

proc change_channels {new_channels} {
    global ichannels
    if {$ichannels != $new_channels} {
	set ichannels $new_channels
	update_codecs_displayed
	reset_encodings
    }
}

proc update_channels_displayed {} {
    global freq channel_support

    set m1 .prefs.pane.audio.dd.sampling.ch_in.mb.m 
    $m1 delete 0 last
    set s [lsearch -glob $channel_support *$freq*]
    
    foreach i [lrange [split [lindex $channel_support $s] ","] 1 2] {
	 $m1 add command -label "$i" -command "change_channels $i"
    }
}

proc mbus_recv_tool.rat.sampling.supported {arg} {
    global freq channel_support

    #clear away old state of channel support
    if [info exists channel_support] {
	unset channel_support
    }

    set freqs [list]
    set channel_support [list]

    .prefs.pane.audio.dd.sampling.freq.mb.m delete 0 last

    set mode [split $arg]
    foreach m $mode {
	lappend channel_support $m
	set support [split $m ","]
	set f [lindex $support 0]
	lappend freqs $f
	.prefs.pane.audio.dd.sampling.freq.mb.m add command -label $f -command "change_freq $f"
    }
    set freq [lindex $freqs 0]
    update_channels_displayed
}

# CODEC HANDLING ##############################################################

set codecs {}
set prencs  {}
set secencs {}
set layerencs {}

proc codec_get_name {nickname freq channels} {
    global codecs codec_nick_name codec_rate codec_channels

    foreach {c} $codecs {
	if {$codec_nick_name($c)    == $nickname && \
		$codec_rate($c)     == $freq && \
		$codec_channels($c) == $channels} {
	    return $c
	} 
    }
}

proc codecs_loosely_matching {freq channels} {
    global codecs codec_nick_name codec_channels codec_rate codec_pt codec_state_size codec_data_size codec_block_size codec_desc
    
    set x {}

    foreach {c} $codecs {
	if {$codec_channels($c) == $channels && \
	$codec_rate($c) == $freq && \
	$codec_pt($c) != "-" } {
	    lappend x $c
	}
    }

    return $x
}

proc codecs_matching {freq channels blocksize} {
    global codec_block_size
    set codecs [codecs_loosely_matching $freq $channels]

    set x {}

    foreach {c} $codecs {
	if {$codec_block_size($c) == $blocksize} {
	    lappend x $c
	}
    }
    return $x
}

proc mbus_recv_tool.rat.codec.details {args} {
    catch {
	global codecs codec_nick_name codec_channels codec_rate codec_pt codec_state_size codec_data_size codec_block_size codec_desc codec_caps codec_layers
	
	set name [lindex $args 1]
	if {[lsearch $codecs $name] == -1} {
	    lappend codecs $name
	}
	set codec_pt($name)         [lindex $args 0]
	set codec_nick_name($name)  [lindex $args 2]
	set codec_channels($name)   [lindex $args 3]
	set codec_rate($name)       [lindex $args 4]
	set codec_block_size($name) [lindex $args 5]
	set codec_state_size($name) [lindex $args 6]
	set codec_data_size($name)  [lindex $args 7]
	set codec_desc($name)       [lindex $args 8]
	set codec_caps($name)       [lindex $args 9]
	set codec_layers($name)		[lindex $args 10]
	set stackup ""
    } details_error

    if { $details_error != "" } {
	puts "Error: $details_error"
	destroy .
	exit -1
    }
}

proc update_primary_list {arg} {
    # We now have a list of codecs which this RAT supports...
    global prenc prencs

    .prefs.pane.transmission.dd.pri.m.menu delete 0 last
    set prencs {}

    set codecs [split $arg]
    foreach c $codecs {
	.prefs.pane.transmission.dd.pri.m.menu    add command -label $c -command "set prenc $c; update_codecs_displayed"
	lappend prencs $c
    }

    if {[lsearch $codecs $prenc] == -1} {
	#primary is not on list
	set prenc [lindex $codecs 0]
    }
}

proc update_redundancy_list {arg} {
    global secenc secencs

    .prefs.pane.transmission.cc.red.fc.m.menu delete 0 last
    set secencs {}

    set codecs [split $arg]
    foreach c $codecs {
	.prefs.pane.transmission.cc.red.fc.m.menu add command -label $c -command "set secenc \"$c\""
	lappend secencs $c
    }
    if {[lsearch $codecs $secenc] == -1} {
	#primary is not on list
	set secenc [lindex $codecs 0]
    }
}

proc update_layer_list {arg} {
    global layerenc layerencs

    .prefs.pane.transmission.cc.layer.fc.m.menu delete 0 last
    set layerencs {}

    for {set i 1} {$i <= $arg} {incr i} {
	.prefs.pane.transmission.cc.layer.fc.m.menu add command -label $i -command "set layerenc \"$i\""
	lappend layerencs $i
	}
	if {$layerenc > $arg} {
	#new codec doesn't support as many layers
	set layerenc $arg
	}
}

proc reset_encodings {} {
    global prenc prencs secenc secencs layerenc layerencs
    set prenc  [lindex $prencs 0]
    set secenc [lindex $secencs 0]
	set layerenc [lindex $layerencs 0]
}

proc update_codecs_displayed { } {
    global freq ichannels codec_nick_name prenc codec_block_size codec_caps codec_layers

    if {[string match $ichannels Mono]} {
	set sample_channels 1
    } else {
	set sample_channels 2
    }

    set sample_rate [string trimright $freq -kHz]
    set sample_rate [expr $sample_rate * 1000]

    set long_names [codecs_loosely_matching $sample_rate $sample_channels]

    set friendly_names {}
    foreach {n} $long_names {
	# only interested in codecs that can (e)ncode
	if {[string first $codec_caps($n) ncode]} {
	    lappend friendly_names $codec_nick_name($n)
	}
    }

    update_primary_list $friendly_names

    set long_name [codec_get_name $prenc $sample_rate $sample_channels]
    set long_names [codecs_matching $sample_rate $sample_channels $codec_block_size($long_name)]

    set friendly_names {}
    set found 0
    foreach {n} $long_names {
	# Only display codecs of same or lower order as primary in primary list
	if {$codec_nick_name($n) == $prenc} {
	    set found 1
	}
	if {$found} {
	    if {[string first $codec_caps($n) ncode]} {
		lappend friendly_names $codec_nick_name($n)
	    }
	}
    }
    
    update_redundancy_list $friendly_names

	# Assume that all codecs of one type support the same number of layers
	foreach {n} $long_names {
	if {$codec_nick_name($n) == $prenc} {
	break
	}
	}
	update_layer_list $codec_layers($n)
}

proc change_sampling { } {
    update_channels_displayed
    update_codecs_displayed
}

###############################################################################

proc mbus_recv_tool.rat.converters.flush {} {
    .prefs.pane.reception.r.ms.menu delete 0 last
}

proc mbus_recv_tool.rat.converters.add {arg} {
    global convert_var
    .prefs.pane.reception.r.ms.menu add command -label "$arg" -command "set convert_var \"$arg\""
}

proc mbus_recv_tool.rat.converter {arg} {
    global convert_var
    set convert_var $arg
}

proc mbus_recv_tool.rat.repairs.flush {} {
    .prefs.pane.reception.r.m.menu delete 0 last
}


proc mbus_recv_tool.rat.repairs.add {arg} {
    global repair_var
    .prefs.pane.reception.r.m.menu add command -label "$arg" -command "set repair_var \"$arg\""
}

proc mbus_recv_tool.rat.repair {arg} {
    global repair_var
    set repair_var $arg
}

proc mbus_recv_audio_devices_flush {} {
    .prefs.pane.audio.dd.device.mdev.menu delete 0 last
}

proc mbus_recv_audio_devices_add {arg} {
    global audio_device

    .prefs.pane.audio.dd.device.mdev.menu add command -label "$arg" -command "set audio_device \"$arg\""

    set len [string length "$arg"]
    set curr [.prefs.pane.audio.dd.device.mdev cget -width]

    if {$len > $curr} {
	.prefs.pane.audio.dd.device.mdev configure -width $len
    }
}

proc mbus_recv_audio_device {arg} {
	global audio_device
	set audio_device $arg
}

proc mbus_recv_tool.rat.powermeter {arg} {
	global meter_var
	set meter_var $arg
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

proc mbus_recv_tool.rat.format.in {arg} {
    global freq ichannels
#expect arg to be <sample_type>,<sample rate>,<mono/stereo>
    set e [split $arg ","]
    
    set freq      [lindex $e 1]
    set ichannels [lindex $e 2]
}

proc mbus_recv_tool.rat.codec {arg} {
  global prenc
  set prenc $arg
}

proc mbus_recv_tool.rat.rate {arg} {
    global upp
    set upp $arg
}

proc mbus_recv_audio.channel.coding {args} {
    global channel_var secenc red_off int_units int_gap prenc layerenc

    set channel_var [lindex $args 0]

    switch [string tolower $channel_var] {
    	redundancy {
		set secenc  [lindex $args 1]
		set red_off [lindex $args 2]
	}
	interleaved {
		set int_units [lindex $args 1]
		set int_gap   [lindex $args 2]
	}
	layering {
#		should we be playing with primary encoding?
#		set prenc	  [lindex $args 1]
		set layerenc  [lindex $args 2]
	}
    }
}

proc mbus_recv_audio.channel.repair {arg} {
  global repair_var
  set repair_var $arg
}

proc mbus_recv_audio.input.powermeter {level} {
	global bargraphTotalHeight
	bargraphSetHeight .r.c.gain.gra.b2 [expr ($level * $bargraphTotalHeight) / 100]
}

proc mbus_recv_audio.output.powermeter {level} {
	global bargraphTotalHeight
	bargraphSetHeight .r.c.vol.gra.b1  [expr ($level * $bargraphTotalHeight) / 100]
}

proc mbus_recv_audio.input.gain {new_gain} {
    global gain
    set gain $new_gain
    .r.c.gain.gra.s2 set $gain
}

proc mbus_recv_audio.input.ports.flush {} {
    global input_ports 
    set input_ports [list]
}

proc mbus_recv_audio.input.ports.add {port} {
    global input_ports
    lappend input_ports "$port"
}

proc mbus_recv_audio.input.port {device} {
    set err ""
    catch {
	configure_input_port $device
	set tmp ""
    } err
	if {$err != ""} {
	    puts "error: $err"
	}
}

proc mbus_recv_audio.input.mute {val} {
    global in_mute_var
    set in_mute_var $val
    bargraphState .r.c.gain.gra.b2 [expr ! $val]
}

proc mbus_recv_audio.output.gain {gain} {
	.r.c.vol.gra.s1 set $gain
}

proc mbus_recv_audio.output.port {device} {
	global output_port
    set err ""
    catch {
	configure_output_port $device
	set a ""
    } err
	if {$err != ""} {
	    puts "Output port error: $err"
	}
}

proc mbus_recv_audio.output.ports.flush {} {
    global output_ports
    set output_ports [list]
}

proc mbus_recv_audio.output.ports.add {port} {
    global output_ports
    lappend output_ports "$port"
}

proc mbus_recv_audio.output.mute {val} {
    global out_mute_var
    set out_mute_var $val
}

proc mbus_recv_session.title {title} {
    global session_title
    set session_title $title
    wm title . "RAT: $title"
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

proc mbus_recv_rtp.ssrc {ssrc} {
	global my_ssrc 

	set my_ssrc $ssrc
	init_source $ssrc
	ssrc_update $ssrc
}

proc mbus_recv_rtp.source.exists {ssrc} {
	init_source $ssrc
	chart_label $ssrc
	ssrc_update $ssrc
}

proc mbus_recv_rtp.source.cname {ssrc cname} {
	global CNAME NAME SSRC
	init_source $ssrc
	set CNAME($ssrc) $cname
	if {[string compare $NAME($ssrc) $SSRC($ssrc)] == 0} {
		set NAME($ssrc) $cname
	}
	chart_label $ssrc
	ssrc_update $ssrc
}

proc mbus_recv_rtp.source.name {ssrc name} {
	global NAME rtcp_name my_ssrc
	init_source $ssrc
	set NAME($ssrc) $name
	chart_label $ssrc
	ssrc_update $ssrc
	if {[string compare $ssrc $my_ssrc] == 0} {
		set rtcp_name $name
	}
}

proc mbus_recv_rtp.source.email {ssrc email} {
	global EMAIL rtcp_email my_ssrc
	init_source $ssrc
	set EMAIL($ssrc) $email
	if {[string compare $ssrc $my_ssrc] == 0} {
		set rtcp_email $email
	}
}

proc mbus_recv_rtp.source.phone {ssrc phone} {
	global PHONE rtcp_phone my_ssrc
	init_source $ssrc
	set PHONE($ssrc) $phone
	if {[string compare $ssrc $my_ssrc] == 0} {
		set rtcp_phone $phone
	}
}

proc mbus_recv_rtp.source.loc {ssrc loc} {
	global LOC rtcp_loc my_ssrc
	init_source $ssrc
	set LOC($ssrc) $loc
	if {[string compare $ssrc $my_ssrc] == 0} {
		set rtcp_loc $loc
	}
}

proc mbus_recv_rtp.source.tool {ssrc tool} {
	global TOOL my_ssrc
	init_source $ssrc
	set TOOL($ssrc) $tool
	if {[string compare $ssrc $my_ssrc] == 0} {
	    global tool_name
	    # tool name looks like RAT x.x.x platform ....
	    # lose the platform stuff
	    set tool_frag [split $tool]
	    set tool_name "UCL [lindex $tool_frag 0] [lindex $tool_frag 1]"
	}
}

proc mbus_recv_rtp.source.note {ssrc note} {
	global NOTE
	init_source $ssrc
	set NOTE($ssrc) $note
}

proc mbus_recv_rtp.source.codec {ssrc codec} {
	global CODEC
	init_source $ssrc
	set CODEC($ssrc) $codec
}

proc mbus_recv_rtp.source.gain {ssrc gain} {
	global GAIN
	init_source $ssrc
	set GAIN($ssrc) $gain
}

proc mbus_recv_rtp.source.packet.duration {ssrc packet_duration} {
	global DURATION
	init_source $ssrc
	set DURATION($ssrc) $packet_duration
}

proc mbus_recv_tool.rat.audio.buffered {ssrc buffered} {
        global BUFFER_SIZE
        init_source $ssrc
        set BUFFER_SIZE($ssrc) $buffered 
# we don't update cname as source.packet.duration always follows 
}

proc mbus_recv_tool.rat.audio.delay {ssrc len} {
        global PLAYOUT_DELAY
        init_source $ssrc
        set PLAYOUT_DELAY($ssrc) $len 
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
    set 3d_filter_lengths [split $args ","]
}

proc mbus_recv_tool.rat.3d.user.settings {args} {
    global filter_type filter_length azimuth
    set ssrc                 [lindex $args 0]
    set filter_type($ssrc)   [lindex $args 1]
    set filter_length($ssrc) [lindex $args 2]
    set azimuth($ssrc)       [lindex $args 3]
}

proc mbus_recv_rtp.source.packet.loss {dest srce loss} {
	global losstimers my_ssrc LOSS_FROM_ME LOSS_TO_ME HEARD_LOSS_FROM_ME HEARD_LOSS_TO_ME
	init_source $srce
	init_source $dest
	catch {after cancel $losstimers($srce,$dest)}
	chart_set $srce $dest $loss
	set losstimers($srce,$dest) [after 7500 chart_set \"$srce\" \"$dest\" 101]
	if {[string compare $dest $my_ssrc] == 0} {
		set LOSS_TO_ME($srce) $loss
    		set HEARD_LOSS_TO_ME($srce) 1
	}
	if {[string compare $srce $my_ssrc] == 0} {
		set LOSS_FROM_ME($dest) $loss
		set HEARD_LOSS_FROM_ME($dest) 1
	}
	ssrc_update $srce
	ssrc_update $dest
}

proc mbus_recv_rtp.source.reception {ssrc packets_recv packets_lost packets_miso packets_dup jitter jit_tog} {
	global PCKTS_RECV PCKTS_LOST PCKTS_MISO PCKTS_DUP JITTER JIT_TOGED
	init_source $ssrc 
	set PCKTS_RECV($ssrc) $packets_recv
	set PCKTS_LOST($ssrc) $packets_lost
	set PCKTS_MISO($ssrc) $packets_miso
	set PCKTS_DUP($ssrc)  $packets_dup
	set JITTER($ssrc) $jitter
	set JIT_TOGED($ssrc) $jit_tog
}

proc mbus_recv_rtp.source.active {ssrc} {
	global speaker_highlight
	init_source $ssrc 
	ssrc_update $ssrc
	[window_plist $ssrc] configure -background $speaker_highlight
}

proc mbus_recv_rtp.source.inactive {ssrc} {
	init_source $ssrc 
	ssrc_update $ssrc
	[window_plist $ssrc] configure -background [.l.t.list cget -bg]
}

proc mbus_recv_rtp.source.remove {ssrc} {
    global CNAME NAME EMAIL LOC PHONE TOOL NOTE CODEC DURATION PCKTS_RECV PCKTS_LOST PCKTS_MISO \
	    PCKTS_DUP JITTER BUFFER_SIZE PLAYOUT_DELAY LOSS_TO_ME LOSS_FROM_ME INDEX JIT_TOGED \
	    num_ssrc loss_to_me_timer loss_from_me_timer GAIN MUTE HEARD_LOSS_TO_ME HEARD_LOSS_FROM_ME

    # Disable updating of loss diamonds. This has to be done before we destroy the
    # window representing the participant, else the background update may try to 
    # access a window which has been destroyed...
    catch {after cancel $loss_to_me_timer($ssrc)}
    catch {after cancel $loss_from_me_timer($ssrc)}
    
    catch [destroy [window_plist $ssrc]]
    if { [info exists CNAME($ssrc)] } {
	unset CNAME($ssrc) NAME($ssrc) EMAIL($ssrc) PHONE($ssrc) LOC($ssrc) TOOL($ssrc) NOTE($ssrc)
	unset CODEC($ssrc) DURATION($ssrc) PCKTS_RECV($ssrc) PCKTS_LOST($ssrc) PCKTS_MISO($ssrc) PCKTS_DUP($ssrc)
	unset JITTER($ssrc) LOSS_TO_ME($ssrc) LOSS_FROM_ME($ssrc) INDEX($ssrc) JIT_TOGED($ssrc) BUFFER_SIZE($ssrc)
	unset PLAYOUT_DELAY($ssrc) GAIN($ssrc) MUTE($ssrc) HEARD_LOSS_TO_ME($ssrc) HEARD_LOSS_FROM_ME($ssrc)
	incr num_ssrc -1
	chart_redraw $num_ssrc
    }
}

proc mbus_recv_rtp.source.mute {ssrc val} {
	global iht MUTE
	set MUTE($ssrc) $val
	if {$val} {
		[window_plist $ssrc] create line [expr $iht + 2] [expr $iht / 2] 500 [expr $iht / 2] -tags a -width 2.0 -fill gray95
	} else {
		catch [[window_plist $ssrc] delete a]
	}
}

proc mbus_recv_audio.file.play.ready {name} {
    global play_file
    set    play_file(name) $name

    if {$play_file(state) != "play"} {
	file_enable_play
    }
}

proc mbus_recv_audio.file.play.alive {alive} {
    
    global play_file

    if {$alive} {
	after 200 file_play_live
    } else {
	set play_file(state) end
	file_enable_play
    }
}

proc mbus_recv_audio.file.record.ready {name} {
    global rec_file
    set    rec_file(name) $name
    if {$rec_file(state) != "record"} {
	file_enable_record
    }
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
	destroy .
}

#############################################################################################################

proc set_loss_to_me {ssrc loss} {
	global prev_loss_to_me loss_to_me_timer

	catch {after cancel $loss_to_me_timer($ssrc)}
	set loss_to_me_timer($ssrc) [after 7500 catch \"[window_plist $ssrc] itemconfigure h -fill grey\"]

	if {$loss < 5} {
		catch [[window_plist $ssrc] itemconfigure m -fill green]
	} elseif {$loss < 10} {
		catch [[window_plist $ssrc] itemconfigure m -fill orange]
	} elseif {$loss <= 100} {
		catch [[window_plist $ssrc] itemconfigure m -fill red]
	} else {
		catch [[window_plist $ssrc] itemconfigure m -fill grey]
	}
}

proc set_loss_from_me {ssrc loss} {
	global prev_loss_from_me loss_from_me_timer

	catch {after cancel $loss_from_me_timer($ssrc)}
	set loss_from_me_timer($ssrc) [after 7500 catch \"[window_plist $ssrc] itemconfigure h -fill grey\"]

	if {$loss < 5} {
		catch [[window_plist $ssrc] itemconfigure h -fill green]
	} elseif {$loss < 10} {
		catch [[window_plist $ssrc] itemconfigure h -fill orange]
	} elseif {$loss <= 100} {
		catch [[window_plist $ssrc] itemconfigure h -fill red]
	} else {
		catch [[window_plist $ssrc] itemconfigure h -fill grey]
	}
}

proc ssrc_update {ssrc} {
	# This procedure updates the on-screen representation of
	# a participant. 
	global NAME LOSS_TO_ME LOSS_FROM_ME HEARD_LOSS_FROM_ME HEARD_LOSS_TO_ME
	global fw iht iwd my_ssrc 

	set cw [window_plist $ssrc]

	if {[winfo exists $cw]} {
		$cw itemconfigure t -text $NAME($ssrc)
	} else {
		# Add this participant to the list...
		set thick 0
		set l $thick
		set h [expr $iht / 2 + $thick]
		set f [expr $iht + $thick]
		canvas $cw -width $iwd -height $f -highlightthickness $thick
		$cw create text [expr $f + 2] $h -anchor w -text $NAME($ssrc) -fill black -tag t
		$cw create polygon $l $h $h $l $h $f -outline black -fill grey -tag m
		$cw create polygon $f $h $h $l $h $f -outline black -fill grey -tag h

		bind $cw <Button-1>         "toggle_stats \"$ssrc\""
		bind $cw <Button-2>         "toggle_mute $cw \"$ssrc\""
		bind $cw <Control-Button-1> "toggle_mute $cw \"$ssrc\""

		if {[info exists my_ssrc] && ([string compare $ssrc $my_ssrc] == 0) && ([pack slaves $fw] != "")} {
			pack $cw -before [lindex [pack slaves $fw] 0] -fill x
		}
		pack $cw -fill x
		fix_scrollbar
	}

	if {[info exists HEARD_LOSS_TO_ME($ssrc)] && $HEARD_LOSS_TO_ME($ssrc) && $ssrc != $my_ssrc} {
	    set_loss_to_me $ssrc $LOSS_TO_ME($ssrc)
	}
	if {[info exists HEARD_LOSS_FROM_ME($ssrc)] && $HEARD_LOSS_FROM_ME($ssrc) && $ssrc != $my_ssrc} {
	    set_loss_from_me $ssrc $LOSS_FROM_ME($ssrc)
	}
}

#power meters

# number of elements in the bargraphs...
set bargraphTotalHeight 12
set bargraphRedHeight [expr $bargraphTotalHeight * 3 / 4] 

proc bargraphCreate {bgraph} {
	global oh$bgraph bargraphTotalHeight

	frame $bgraph -relief sunk -bg black
	for {set i 0} {$i < $bargraphTotalHeight} {incr i} {
		frame $bgraph.inner$i -bg black
		pack $bgraph.inner$i -side left -fill both -expand true
	}
	set oh$bgraph 0
}

proc bargraphSetHeight {bgraph height} {
	upvar #0 oh$bgraph oh 
	global bargraphTotalHeight bargraphRedHeight

	if {$oh > $height} {
		for {set i [expr $height]} {$i <= $oh} {incr i} {
			$bgraph.inner$i config -bg black
		}
	} else {
		if {$bargraphRedHeight > $height} {
			for {set i [expr $oh]} {$i <= $height} {incr i} {
				$bgraph.inner$i config -bg green
			}
		} else {
			for {set i [expr $oh]} {$i <= $bargraphRedHeight} {incr i} {
				$bgraph.inner$i config -bg green
			}
			for {set i $bargraphRedHeight} {$i <= $height} {incr i} {
				$bgraph.inner$i config -bg red
			}
		}
	}
	set oh $height
}

proc bargraphState {bgraph state} {
    upvar #0 oh$bgraph oh 
    if {[winfo exists $bgraph]} {
	global bargraphTotalHeight
	for { set i 0 } { $i < $bargraphTotalHeight} {incr i} {
	    $bgraph.inner$i config -bg black
	}
	if {$state} {
	    $bgraph.inner0 config -bg green
	}
    }
    set oh 0
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

proc toggle_mute {cw ssrc} {
	global iht
	if {[$cw gettags a] == ""} {
		mbus_send "R" "rtp.source.mute" "[mbus_encode_str $ssrc] 1"
	} else {
		mbus_send "R" "rtp.source.mute" "[mbus_encode_str $ssrc] 0"
	}
}

proc send_gain_and_mute {ssrc} {
	global GAIN MUTE
	mbus_send "R" "rtp.source.gain" "[mbus_encode_str $ssrc] $GAIN($ssrc)"
	mbus_send "R" "rtp.source.mute" "[mbus_encode_str $ssrc] $MUTE($ssrc)"
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

proc ssrc_set_gain {ssrc gain} {
    global GAIN
    set    GAIN($ssrc) [format "%.2f " [expr pow (2, $gain)]]
    send_gain_and_mute $ssrc
}

set 3d_azimuth(min) 0
set 3d_azimuth(max) 0
set 3d_filters        [list "Not Available"]
set 3d_filter_lengths [list "0"]

proc toggle_stats {ssrc} {
    global statsfont
    set win [window_stats $ssrc]
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
	$win.mf.mb.menu add command -label "Audio"            -command "set_pane stats_pane($win) $win.df Audio"
	$win.mf.mb.menu add command -label "3D Positioning"   -command "set_pane stats_pane($win) $win.df \"3D Positioning\""

	set stats_pane($win) "Personal Details"
	frame $win.df 
	frame $win.df.personal 
	pack  $win.df $win.df.personal -fill x

	global NAME EMAIL PHONE LOC NOTE CNAME TOOL SSRC
	stats_add_field $win.df.personal.1 "Name: "     NAME($ssrc)
	stats_add_field $win.df.personal.2 "Email: "    EMAIL($ssrc)
	stats_add_field $win.df.personal.3 "Phone: "    PHONE($ssrc)
	stats_add_field $win.df.personal.4 "Location: " LOC($ssrc)
	stats_add_field $win.df.personal.5 "Note: "     NOTE($ssrc)
	stats_add_field $win.df.personal.6 "Tool: "     TOOL($ssrc)
	stats_add_field $win.df.personal.7 "CNAME: "    CNAME($ssrc)
	stats_add_field $win.df.personal.8 "SSRC: "     SSRC($ssrc)

	frame $win.df.reception
	global CODEC DURATION BUFFER_SIZE PLAYOUT_DELAY PCKTS_RECV PCKTS_LOST PCKTS_MISO \
	       PCKTS_DUP LOSS_FROM_ME LOSS_TO_ME JITTER JIT_TOGED
	stats_add_field $win.df.reception.1 "Audio encoding: "         CODEC($ssrc)
	stats_add_field $win.df.reception.2 "Packet duration (ms): "   DURATION($ssrc)
	stats_add_field $win.df.reception.3 "Playout delay (ms): "     PLAYOUT_DELAY($ssrc)
	stats_add_field $win.df.reception.5 "Arrival jitter (ms): "    JITTER($ssrc)
	stats_add_field $win.df.reception.6 "Loss from me (%): "       LOSS_FROM_ME($ssrc)
	stats_add_field $win.df.reception.7 "Loss to me (%): "         LOSS_TO_ME($ssrc)
	stats_add_field $win.df.reception.8 "Packets received: "       PCKTS_RECV($ssrc)
	stats_add_field $win.df.reception.9 "Packets lost: "           PCKTS_LOST($ssrc)
	stats_add_field $win.df.reception.a "Packets misordered: "     PCKTS_MISO($ssrc)
	stats_add_field $win.df.reception.b "Packets duplicated: "     PCKTS_DUP($ssrc)
	stats_add_field $win.df.reception.c "Units dropped (jitter): " JIT_TOGED($ssrc)

# Audio settings
	global GAIN MUTE
	frame $win.df.audio -relief sunk	
	label $win.df.audio.advice -text "The signal from the participant can\nbe scaled and muted with the controls below."
	pack  $win.df.audio.advice

	checkbutton $win.df.audio.mute -text "Mute" -variable MUTE($ssrc) -command "send_gain_and_mute $ssrc"
	pack $win.df.audio.mute

	frame $win.df.audio.opts
	pack  $win.df.audio.opts -side top
	label $win.df.audio.opts.title -text "Gain"
	scale $win.df.audio.opts.gain_scale -showvalue 0 -orient h -from -3 -to +3 -resolution 0.25 -command "ssrc_set_gain $ssrc"
	label $win.df.audio.opts.gain_text -textvariable GAIN($ssrc) -width 4
	pack  $win.df.audio.opts.title $win.df.audio.opts.gain_scale $win.df.audio.opts.gain_text -side left

	$win.df.audio.opts.gain_scale set [expr log10($GAIN($ssrc)) / log10(2)] 

	button $win.df.audio.default -text "Default" -command "set MUTE($ssrc) 0; $win.df.audio.opts.gain_scale set 0.0; send_gain_and_mute $ssrc" 
	pack   $win.df.audio.default -side right  -anchor e -padx 2 -pady 2

# 3D settings
	# Trigger engine to send details for this participant
	mbus_send "R" "tool.rat.3d.user.settings.request" [mbus_encode_str $ssrc]

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
	set filter_type($ssrc) [lindex $3d_filters 0]

	set cnt 0
	foreach i $3d_filters {
	    radiobutton $win.df.3d.opts.filters.$cnt \
		    -value "$i" -variable filter_type($ssrc) \
		    -text "$i"
 		pack $win.df.3d.opts.filters.$cnt -side top -anchor w
	    incr cnt
	}

	frame $win.df.3d.opts.lengths 
	label $win.df.3d.opts.lengths.l -text "Filter Length:" -width 16
	pack $win.df.3d.opts.lengths.l -side top -fill x -expand 1
	
	global filter_length
	set filter_length($ssrc) [lindex $3d_filter_lengths 0]
	
	set cnt 0
	foreach i $3d_filter_lengths {
	    radiobutton $win.df.3d.opts.lengths.$cnt \
		    -value "$i" -variable filter_length($ssrc) \
		    -text "$i"
	    pack $win.df.3d.opts.lengths.$cnt -side top -anchor w
	    incr cnt
	}
	pack $win.df.3d.opts.filters -side left -expand 1 -anchor n
	pack $win.df.3d.opts.lengths -side left -expand 1 -anchor n
	
	global 3d_azimuth azimuth
	scale $win.df.3d.azimuth -from $3d_azimuth(min) -to $3d_azimuth(max) \
		-orient horizontal -label "Azimuth" -variable azimuth($ssrc)
	pack  $win.df.3d.azimuth -fill x -expand 1 

	button $win.df.3d.apply -text "Apply" -command "3d_send_parameters $ssrc"
	pack   $win.df.3d.apply -side bottom  -anchor e -padx 2 -pady 2

# Window Magic
	frame  $win.dis 
	button $win.dis.b -text "Dismiss" -command "destroy $win; 3d_delete_parameters $ssrc"
	pack   $win.dis   -side bottom -anchor s -fill x -expand 1
	pack   $win.dis.b -side right -anchor e -padx 2 -pady 2
	wm title $win "Participant $NAME($ssrc)"
	wm resizable $win 1 0
	constrain_window $win $statsfont 36 27
    }
}

proc 3d_send_parameters {ssrc} {
    global azimuth filter_type filter_length 3d_audio_var

    mbus_send "R" "tool.rat.3d.enabled"   $3d_audio_var
    mbus_send "R" "tool.rat.3d.user.settings" "[mbus_encode_str $ssrc] [mbus_encode_str $filter_type($ssrc)] $filter_length($ssrc) $azimuth($ssrc)"
}

proc 3d_delete_parameters {ssrc} {
    global filter_type filter_length azimuth
    
# None of these should ever fail, but you can't be too defensive...
    catch {
	unset filter_type($ssrc)
	unset filter_length($ssrc)
	unset azimuth($ssrc)
    }
}

proc bitmap_input_port {port} {
    set port [string tolower $port]
    return ""
	switch -glob $port {
	mic* {return "microphone"}
	lin* {return "line_in"}
	cd*  {return "cd"}
	default {return ""}
    }
}

proc bitmap_output_port {port} {
    set port [string tolower $port]
    return ""
	switch -glob $port {
	speak* {return "speaker"}
	lin*   {return "line_out"}
	head*  {return "headphone"}
	default {return ""}
    }
}

proc configure_input_port {port} {
    global input_port
    set bitmap [bitmap_input_port $port]
    if {$bitmap != ""} {
	.r.c.gain.but.l2 configure -bitmap $bitmap
    } else {
	.r.c.gain.but.l2 configure -bitmap ""
	.r.c.gain.but.l2 configure -text $port
    }
    set input_port $port
}

proc configure_output_port {port} {
    global output_port
    set bitmap [bitmap_output_port $port]

    if {$bitmap != ""} {
	.r.c.vol.but.l1 configure -bitmap $bitmap
    } else {
	.r.c.vol.but.l1 configure -bitmap ""
	.r.c.vol.but.l1 configure -text $port
    }
    set output_port $port
}

proc do_quit {} {
	catch {
		profile off pdat
		profrep pdat cpu
	}
	destroy .
	mbus_send "R" "mbus.quit" ""
}

# Initialise RAT MAIN window
frame .r 
frame .l
frame .l.t -relief sunken
scrollbar .l.t.scr -relief flat -highlightthickness 0 -command ".l.t.list yview"
canvas .l.t.list -highlightthickness 0 -bd 0 -relief sunk -width $iwd -height 160 -yscrollcommand ".l.t.scr set" -yscrollincrement $iht
frame .l.t.list.f -highlightthickness 0 -bd 0
.l.t.list create window 0 0 -anchor nw -window .l.t.list.f

frame .l.f -relief flat -bd 0
label .l.f.title -bd 0 -textvariable session_title
label .l.f.addr  -bd 0 -textvariable session_address

frame  .st -bd 0
label  .st.tool -textvariable tool_name 
button .st.opts  -text "Options"   -command {wm deiconify .prefs; raise .prefs}
button .st.about -text "About"     -command {jiggle_credits; wm deiconify .about}
button .st.quit  -text "Quit"      -command do_quit

frame .r.c -bd 0
frame .r.c.vol -bd 0
frame .r.c.gain -bd 0

pack .st -side bottom -fill x 
pack .st.tool -side left -anchor w
pack .st.quit .st.about .st.opts -side right -anchor w -padx 2 -pady 2

pack .r -side top -fill x 
pack .r.c -side top -fill x -expand 1
pack .r.c.vol  -side top -fill x
pack .r.c.gain -side top -fill x

pack .l -side top -fill both -expand 1
pack .l.f -side bottom -fill x -padx 2 -pady 2
pack .l.f.title .l.f.addr -side top -pady 2 -anchor w
pack .l.t  -side top -fill both -expand 1 -padx 2
pack .l.t.scr -side left -fill y
pack .l.t.list -side left -fill both -expand 1
bind .l.t.list <Configure> {fix_scrollbar}

# Device output controls
set out_mute_var 0
frame .r.c.vol.but
frame .r.c.vol.gra
checkbutton .r.c.vol.but.t1 -highlightthickness 0 -text "Receive" -onvalue 0 -offvalue 1 -variable out_mute_var -command {output_mute $out_mute_var} -font $infofont -width 8 -anchor w -relief raised
button .r.c.vol.but.l1 -highlightthickness 0 -command toggle_output_port -font $infofont -width 10
bargraphCreate .r.c.vol.gra.b1
scale .r.c.vol.gra.s1 -highlightthickness 0 -from 0 -to 99 -command set_vol -orient horizontal -showvalue false -width 8 -variable volume

pack .r.c.vol.but -side left -fill both 
pack .r.c.vol.but.t1 -side left -fill y
pack .r.c.vol.but.l1 -side left -fill y

pack .r.c.vol.gra -side left -fill both -expand 1
pack .r.c.vol.gra.b1 -side top  -fill both -expand 1 -padx 1 -pady 1
pack .r.c.vol.gra.s1 -side bottom  -fill x -anchor s

# Device input controls
set in_mute_var 1

frame .r.c.gain.but
checkbutton .r.c.gain.but.t2 -highlightthickness 0 -text "Transmit" -variable in_mute_var -onvalue 0 -offvalue 1 -command {input_mute $in_mute_var} -font $infofont -width 8 -anchor w -relief raised
button .r.c.gain.but.l2 -highlightthickness 0 -command toggle_input_port -font $infofont -width 10

frame .r.c.gain.gra
bargraphCreate .r.c.gain.gra.b2
scale .r.c.gain.gra.s2 -highlightthickness 0 -from 0 -to 99 -command set_gain -orient horizontal -showvalue false -width 8 -variable gain -font $smallfont

pack .r.c.gain.but    -side left 
pack .r.c.gain.but.t2 -side left -fill y
pack .r.c.gain.but.l2 -side left -fill y

pack .r.c.gain.gra -side left -fill both -expand 1
pack .r.c.gain.gra.b2 -side top  -fill both -expand 1 -padx 1 -pady 1
pack .r.c.gain.gra.s2 -side top  -fill x -anchor s

proc mbus_recv_tool.rat.disable.audio.ctls {} {
#	.r.c.vol.but.t1 configure -state disabled
	.r.c.vol.but.l1 configure -state disabled
	.r.c.vol.gra.s1 configure -state disabled
#	.r.c.gain.but.t2 configure -state disabled
	.r.c.gain.but.l2 configure -state disabled
	.r.c.gain.gra.s2 configure -state disabled
}

proc mbus_recv_tool.rat.enable.audio.ctls {} {
#	.r.c.vol.but.t1 configure -state normal
	.r.c.vol.but.l1 configure -state normal
	.r.c.vol.gra.s1 configure -state normal
#	.r.c.gain.but.t2 configure -state normal
	.r.c.gain.but.l2 configure -state normal
	.r.c.gain.gra.s2 configure -state normal
}
bind all <ButtonPress-3>   {toggle in_mute_var; input_mute $in_mute_var}
bind all <ButtonRelease-3> {toggle in_mute_var; input_mute $in_mute_var}
bind all <q>               {+if {[winfo class %W] != "Entry"} {do_quit}}

# Override default tk behaviour
wm protocol . WM_DELETE_WINDOW do_quit

if {$win32 == 0} {
	wm iconbitmap . rat_small
}
wm resizable . 0 1
if ([info exists geometry]) {
        wm geometry . $geometry
}

proc averageCharacterWidth {font} {
    set sample "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    set slen [string length $sample]
    set wpc  [expr [font measure $font $sample] / $slen + 1]
    return $wpc
}

# constrain_window - window, font, characters wide, characters high
proc constrain_window {w font cW cH} {
    
    catch {
            set wpc [averageCharacterWidth $font]
            set hpc [font metrics $font -ascent]
    
        # Calculate dimensions
            set width [expr $cW * $wpc]
            set height [expr $cH * $hpc]
            wm geometry $w [format "%sx%s" $width $height]
            set dummy ""
    } err
    if {$err != ""} {
        puts "Error: $err"
    }
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
.prefs.m.f.m.menu add command -label "Transmission" -command {set_pane prefs_pane .prefs.pane "Transmission"; update_codecs_displayed}
.prefs.m.f.m.menu add command -label "Reception"    -command {set_pane prefs_pane .prefs.pane "Reception"}
.prefs.m.f.m.menu add command -label "Audio"        -command {set_pane prefs_pane .prefs.pane "Audio"}
.prefs.m.f.m.menu add command -label "Codecs"        -command {set_pane prefs_pane .prefs.pane "Codecs"; codecs_panel_fill}
.prefs.m.f.m.menu add command -label "Security"     -command {set_pane prefs_pane .prefs.pane "Security"}
.prefs.m.f.m.menu add command -label "Interface"    -command {set_pane prefs_pane .prefs.pane "Interface"}

frame  .prefs.buttons
pack   .prefs.buttons       -side bottom -fill x 
button .prefs.buttons.bye   -text "Cancel" -command {sync_ui_to_engine; wm withdraw .prefs}
button .prefs.buttons.apply -text "Apply" -command {wm withdraw .prefs; sync_engine_to_ui}
#button .prefs.buttons.save  -text "Save & Apply" -command {save_settings; wm withdraw .prefs; sync_engine_to_ui}
#pack   .prefs.buttons.bye .prefs.buttons.apply .prefs.buttons.save -side right -padx 2 -pady 2
pack   .prefs.buttons.bye .prefs.buttons.apply -side right -padx 2 -pady 2

wm protocol .prefs WM_DELETE_WINDOW {sync_ui_to_engine; wm withdraw .prefs}

frame .prefs.pane -relief sunken
pack  .prefs.pane -side left -fill both -expand 1 -padx 4 -pady 2

# setup width of prefs panel
constrain_window .prefs $infofont 56 30

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
frame $i.dd 
frame $i.cc 
frame $i.cc.van 
frame $i.cc.red 
frame $i.cc.layer
frame $i.cc.int 
label $i.intro -text "This panel allows you to select codecs for transmission.  The choice\nof codecs available depends on the sampling rate and channels\nin the audio panel."
label $i.title1 -relief raised -text "Audio Encoding"
pack $i.intro $i.title1 $i.dd -side top -fill x

#pack $i.dd -fill x -side top -anchor n

label $i.title2 -relief raised -text "Channel Coding Options"
pack $i.title2 -fill x -side top
pack $i.cc -fill x -anchor w -pady 1

pack $i.cc.van $i.cc.red $i.cc.layer -fill x -anchor w -pady 0
# interleaving panel $i.cc.int not packed since interleaving isn't support in this release
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
radiobutton $i.cc.red.rb -text "Redundancy"         -justify right -value redundancy  -variable channel_var 
radiobutton $i.cc.layer.rb -text "Layering"			-justify right -value layering    -variable channel_var
radiobutton $i.cc.int.rb -text "Interleaving"       -justify right -value interleaved -variable channel_var -state disabled
pack $i.cc.van.rb $i.cc.red.rb $i.cc.layer.rb $i.cc.int.rb -side left -anchor nw -padx 2

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

frame $i.cc.layer.fc
label $i.cc.layer.fc.l -text "Layers:"
menubutton $i.cc.layer.fc.m -textvariable layerenc -indicatoron 1 -menu $i.cc.layer.fc.m.menu -relief raised -width 13
menu $i.cc.layer.fc.m.menu -tearoff 0
pack $i.cc.layer.fc -side right
pack $i.cc.layer.fc.l $i.cc.layer.fc.m

frame $i.cc.int.zz
label $i.cc.int.zz.l -text "Units:"
tk_optionCmdMenu $i.cc.int.zz.m int_units 2 4 6 8 
$i.cc.int.zz.m configure -width 13 -highlightthickness 0 -bd 1 -state disabled

frame $i.cc.int.fc
label $i.cc.int.fc.l -text "Separation:" 
tk_optionCmdMenu $i.cc.int.fc.m int_gap 2 4 6 8 
$i.cc.int.fc.m configure -width 13 -highlightthickness 0 -bd 1 -state disabled

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
tk_optionCmdMenu $i.r.m repair_var {}

label $i.r.ls -text "Sample Rate Conversion"
tk_optionCmdMenu $i.r.ms convert_var {}

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

label $i.dd.title -height 2 -width 40 -text "This panel allows for the selection of alternate audio devices\nand the configuring of device related options." -justify left
pack $i.dd.title -fill x

frame $i.dd.device
pack $i.dd.device -side top

label $i.dd.device.l -text "Audio Device:"
pack  $i.dd.device.l -side top -fill x
menubutton $i.dd.device.mdev -menu $i.dd.device.mdev.menu -indicatoron 1 \
                                -textvariable audio_device -relief raised -width 5
pack $i.dd.device.mdev -fill x -expand 1
menu $i.dd.device.mdev.menu -tearoff 0

frame $i.dd.sampling  
pack  $i.dd.sampling 

frame $i.dd.sampling.freq
frame $i.dd.sampling.ch_in
pack $i.dd.sampling.freq $i.dd.sampling.ch_in -side left -fill x

label $i.dd.sampling.freq.l   -text "Sample Rate:   "
label $i.dd.sampling.ch_in.l  -text "Channels:"
pack $i.dd.sampling.freq.l $i.dd.sampling.ch_in.l -fill x

menubutton $i.dd.sampling.freq.mb -menu $i.dd.sampling.freq.mb.m -indicatoron 1 \
                                  -textvariable freq -relief raised 
pack $i.dd.sampling.freq.mb -side left -fill x -expand 1
menu $i.dd.sampling.freq.mb.m 

menubutton $i.dd.sampling.ch_in.mb -menu $i.dd.sampling.ch_in.mb.m -indicatoron 1 \
                                  -textvariable ichannels -relief raised 
pack $i.dd.sampling.ch_in.mb -side left -fill x -expand 1
menu $i.dd.sampling.ch_in.mb.m 

frame $i.dd.cks
pack $i.dd.cks -fill both -expand 1 
frame $i.dd.cks.f 
frame $i.dd.cks.f.f
checkbutton $i.dd.cks.f.f.silence  -text "Silence Suppression"    -variable silence_var 
checkbutton $i.dd.cks.f.f.agc      -text "Automatic Gain Control" -variable agc_var 
checkbutton $i.dd.cks.f.f.loop     -text "Audio Loopback"         -variable audio_loop_var
checkbutton $i.dd.cks.f.f.suppress -text "Echo Suppression"       -variable echo_var
pack $i.dd.cks.f -fill x -side top -expand 1
pack $i.dd.cks.f.f
pack $i.dd.cks.f.f.silence $i.dd.cks.f.f.agc $i.dd.cks.f.f.loop $i.dd.cks.f.f.suppress -side top -anchor w

# Codecs pane #################################################################
set i .prefs.pane.codecs
frame $i 
frame $i.of -relief sunken
pack  $i.of -fill both -expand 1 -anchor w -pady 1

label $i.of.l -height 2 -width 40 -justify left -text "This panel shows the available codecs, their properties and allows\n their RTP payload types to be re-mapped." 
pack $i.of.l -side top -fill x

frame   $i.of.codecs

pack    $i.of.codecs -side left -padx 2 -fill y
label   $i.of.codecs.l    -text "Codec" -relief raised
listbox $i.of.codecs.lb -width 20 -yscrollcommand "$i.of.codecs.scroll set"
scrollbar $i.of.codecs.scroll -command "$i.of.codecs.lb yview"
pack    $i.of.codecs.l -side top -fill x
pack    $i.of.codecs.scroll $i.of.codecs.lb -side left -fill both 

frame   $i.of.details 
pack    $i.of.details -side left -fill both -expand 1

frame $i.of.details.upper
pack $i.of.details.upper -fill x

frame $i.of.details.desc
pack $i.of.details.desc -side top -fill x

frame $i.of.details.pt 
pack $i.of.details.pt -side bottom -fill x -anchor s
label $i.of.details.pt.l -anchor w -text "RTP payload:"
pack  $i.of.details.pt.l -side left -anchor w

entry $i.of.details.pt.e -width 4 
pack  $i.of.details.pt.e -side left -padx 4

button $i.of.details.pt.b -text "Map Codec" -command map_codec
pack  $i.of.details.pt.b -side left -padx 4

label $i.of.details.upper.l0 -text "Details" -relief raised
pack $i.of.details.upper.l0 -side top -fill x -expand 1

frame $i.of.details.upper.l 
pack $i.of.details.upper.l -side left
label $i.of.details.upper.l.0 -text "Short name:"  -anchor w
label $i.of.details.upper.l.1 -text "Sample Rate (Hz):" -anchor w
label $i.of.details.upper.l.2 -text "Channels:"    -anchor w
label $i.of.details.upper.l.3 -text "Bitrate (kbps):"     -anchor w
label $i.of.details.upper.l.4 -text "RTP Payload:" -anchor w
label $i.of.details.upper.l.5 -text "Capability:" -anchor w
label $i.of.details.upper.l.6 -text "Layers:" -anchor w

for {set idx 0} {$idx < 7} {incr idx} {
    pack $i.of.details.upper.l.$idx -side top -fill x
}

frame $i.of.details.upper.r
pack $i.of.details.upper.r -side left -fill x -expand 1
label $i.of.details.upper.r.0 -anchor w
label $i.of.details.upper.r.1 -anchor w
label $i.of.details.upper.r.2 -anchor w
label $i.of.details.upper.r.3 -anchor w
label $i.of.details.upper.r.4 -anchor w
label $i.of.details.upper.r.5 -anchor w
label $i.of.details.upper.r.6 -anchor w

for {set idx 0} {$idx < 7} {incr idx} {
    pack $i.of.details.upper.r.$idx -side top -fill x
}

set descw [expr [averageCharacterWidth $infofont] * 30]
label $i.of.details.desc.l -text "Description:" -anchor w -wraplength $descw -justify left
pack $i.of.details.desc.l -side left -fill x
unset descw

bind $i.of.codecs.lb <1> {
    codecs_panel_select [%W index @%x,%y]
}

bind $i.of.codecs.lb <ButtonRelease-1> {
    codecs_panel_select [%W index @%x,%y]
}
proc codecs_panel_fill {} {
    global codecs

    .prefs.pane.codecs.of.codecs.lb delete 0 end

    foreach {c} $codecs {
	.prefs.pane.codecs.of.codecs.lb insert end $c
    }
}

set last_selected_codec -1

proc codecs_panel_select { idx } {
    global codecs codec_nick_name codec_rate codec_channels codec_pt codec_block_size codec_data_size codec_desc codec_caps codec_layers
    global last_selected_codec 

    set last_selected_codec $idx

    set codec [lindex $codecs $idx]
    set root  .prefs.pane.codecs.of.details.upper.r
    $root.0 configure -text $codec_nick_name($codec)
    $root.1 configure -text $codec_rate($codec)
    $root.2 configure -text $codec_channels($codec)

    set fps [expr $codec_rate($codec) * 2 * $codec_channels($codec) / $codec_block_size($codec) ]
    set kbps [expr 8 * $fps * $codec_data_size($codec) / 1000.0]
    $root.3 configure -text [format "%.1f" $kbps]

    $root.4 configure -text $codec_pt($codec)
    $root.5 configure -text $codec_caps($codec)
    $root.6 configure -text $codec_layers($codec)
    
    .prefs.pane.codecs.of.details.desc.l configure -text "Description: $codec_desc($codec)"

}

proc map_codec {} {
    global codecs last_selected_codec

    set idx $last_selected_codec

    if {$last_selected_codec == -1} {
	return
    }

    set pt [.prefs.pane.codecs.of.details.pt.e get]
    .prefs.pane.codecs.of.details.pt.e delete 0 end

    set ptnot [string trim $pt 1234567890]
    if {$ptnot != ""} {
	return
    }

    set codec [lindex $codecs $idx]

    mbus_send "R" "tool.rat.payload.set" "[mbus_encode_str $codec] $pt"
    after 1000 codecs_panel_select $idx
}

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
text      .about.rim.d.copyright.f.f.blurb -height 14 -yscrollcommand ".about.rim.d.copyright.f.f.scroll set"
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
label     .about.rim.d.credits.f.f.3                  -text "\nDevelopment Team:"
label     .about.rim.d.credits.f.f.4 -foreground blue -text Bad
label     .about.rim.d.credits.f.f.5                  -text "Additional Contributions:"
label     .about.rim.d.credits.f.f.6 -foreground blue -text Ugly
for {set i 1} {$i<=6} {incr i} {
    pack  .about.rim.d.credits.f.f.$i -side top -fill x
}

button    .about.dismiss -text Dismiss -command "wm withdraw .about"
pack      .about.dismiss -side bottom -anchor e -padx 2 -pady 2

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
constrain_window .about $infofont 64 25 

.about.rim.d.copyright.f.f.blurb insert end {
Copyright (C) 1995-1999 University College London
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, is permitted, provided that the following conditions 
are met:

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
    .about.rim.d.credits.f.f.4 configure -text [shuffle_rats "Colin Perkins" "Orion Hodson"]
    .about.rim.d.credits.f.f.6 configure -text [shuffle_rats "Isidor Kouvelas" "Darren Harris" "Anna Watson" "Mark Handley" "Jon Crowcroft" "Marcus Iken" "Kris Hasler" "Tristan Henderson"]
}

proc sync_ui_to_engine {} {
    # the next time the display is shown, it needs to reflect the
    # state of the audio engine.
    mbus_send "R" "tool.rat.settings" ""
}

proc sync_engine_to_ui {} {
    # make audio engine concur with ui
    global my_ssrc rtcp_name rtcp_email rtcp_phone rtcp_loc 
    global prenc upp channel_var secenc layerenc red_off int_gap int_units
    global silence_var agc_var audio_loop_var echo_var
    global repair_var limit_var min_var max_var lecture_var 3d_audio_var convert_var  
    global meter_var sync_var gain volume input_port output_port 
    global in_mute_var out_mute_var ichannels freq key key_var
    global audio_device

    #rtcp details
    mbus_send "R" "rtp.source.name"  "[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_name]"
    mbus_send "R" "rtp.source.email" "[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_email]"
    mbus_send "R" "rtp.source.phone" "[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_phone]"
    mbus_send "R" "rtp.source.loc"   "[mbus_encode_str $my_ssrc] [mbus_encode_str $rtcp_loc]"
    
    #transmission details
    mbus_send "R" "tool.rat.codec"      "[mbus_encode_str $prenc] [mbus_encode_str $ichannels] [mbus_encode_str $freq]"
    mbus_send "R" "tool.rat.rate"         $upp

    switch $channel_var {
    	none         {mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var]"}
	redundancy   {mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var] [mbus_encode_str $secenc] $red_off"}
	interleaved {mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var] $int_gap $int_units"}
	layering	{mbus_send "R" "audio.channel.coding" "[mbus_encode_str $channel_var] [mbus_encode_str $prenc] [mbus_encode_str $ichannels] [mbus_encode_str $freq] $layerenc"}
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
    mbus_send "R" "tool.rat.3d.enabled"    $3d_audio_var
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

#
# Routines to display the "chart" of RTCP RR statistics...
#

set chart_font    6x10
set chart_size    1
set chart_boxsize 15
set chart_xoffset 180
set chart_yoffset 17

toplevel  .chart
wm protocol .chart WM_DELETE_WINDOW    {set matrix_on 0; chart_show}
canvas    .chart.c  -background white  -xscrollcommand {.chart.sb set} -yscrollcommand {.chart.sr set} 
scrollbar .chart.sr -orient vertical   -command {.chart.c yview}
scrollbar .chart.sb -orient horizontal -command {.chart.c xview}
button    .chart.d  -text "Dismiss"    -command {set matrix_on 0; chart_show}

pack .chart.d  -side bottom -expand 0 -anchor e -padx 2 -pady 2
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
  global INDEX chart_size chart_boxsize chart_xoffset chart_yoffset my_ssrc

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
  .chart.c delete ssrc_$ssrc
  .chart.c create text 2 $ypos -text [string range $val 0 25] -anchor w -font $chart_font -tag ssrc_$ssrc
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

set play_file(state) end
set rec_file(state) end

catch {
    toplevel .file
    wm protocol .file WM_DELETE_WINDOW {set files_on 0; file_show}
    frame .file.play -relief ridge
    frame .file.rec  -relief ridge
    pack  .file.play -side top -pady 2 -padx 2 -fill x -expand 1
    pack  .file.rec  -side top -pady 2 -padx 2 -fill x -expand 1
    
    label .file.play.l -text "Playback"
    pack  .file.play.l -side top -fill x
    label .file.rec.l -text "Record"
    pack  .file.rec.l -side top -fill x

    button .file.dismiss -text Dismiss -command "set files_on 0; file_show"
    pack   .file.dismiss -side bottom -anchor e -padx 2 -pady 2
    
    wm withdraw .file
    wm title	.file "RAT File Control"
    
    foreach action { play rec } {
	frame  .file.$action.buttons
	pack   .file.$action.buttons
	button .file.$action.buttons.disk -bitmap disk -command "fileDialog $action"
	pack   .file.$action.buttons.disk -side left -padx 2 -pady 2 -anchor n
	
	foreach cmd "$action pause stop" {
	    button .file.$action.buttons.$cmd -bitmap $cmd -state disabled -command file_$action\_$cmd
	    pack   .file.$action.buttons.$cmd -side left -padx 2 -pady 2 -anchor n -fill x
	}

	label  .file.$action.buttons.status -text "No file selected." -relief sunk -width 16 -anchor w
	pack   .file.$action.buttons.status -side bottom -fill both -expand 1 -padx 2 -pady 2
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
    .file.play.buttons.status configure -text "Ready to play."
}	

proc file_enable_record { } {
    .file.rec.buttons.rec configure -state normal
    .file.rec.buttons.pause  configure -state disabled
    .file.rec.buttons.stop   configure -state disabled
    .file.rec.buttons.status configure -text "Ready to record."
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
	.file.play.buttons.status configure -text "Playing."
	after 200 file_play_live
}

proc file_play_pause {} {
    global play_file
    
    .file.play.buttons.play   configure -state normal
    .file.play.buttons.pause  configure -state disabled
    .file.play.buttons.stop   configure -state normal
    .file.play.buttons.status configure -text "Paused."
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
	.file.rec.buttons.status configure -text "Recording."
	after 200 file_rec_live
}

proc file_rec_pause {} {
    global rec_file
    
    .file.rec.buttons.rec    configure -state normal
    .file.rec.buttons.pause  configure -state disabled
    .file.rec.buttons.stop   configure -state normal
    .file.rec.buttons.status configure -text "Paused."
    set rec_file(state) paused
    mbus_send "R" "audio.file.record.pause" 1
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

add_help .r.c.gain.gra.s2 	"This slider controls the volume\nof the sound you send."
add_help .r.c.gain.but.l2 	"Click to change input device."
add_help .r.c.gain.but.t2 	"If this button is pushed in, you are are transmitting, and\nmay be\
                         heard by other participants. Holding down the\nright mouse button in\
			 any RAT window will temporarily\ntoggle the state of this button,\
			 allowing for easy\npush-to-talk operation."
add_help .r.c.gain.gra.b2 	"Indicates the loudness of the\nsound you are sending. If this\nis\
                         moving, you may be heard by\nthe other participants."

add_help .r.c.vol.gra.s1  	"This slider controls the volume\nof the sound you hear."
add_help .r.c.vol.but.l1  	"Click to change output device."
add_help .r.c.vol.but.t1  	"If pushed in, reception is muted."
add_help .r.c.vol.gra.b1  	"Indicates the loudness of the\nsound you are hearing."

add_help .l.f		"Name of the session, and the IP address, port\n&\
		 	 TTL used to transmit the audio data."
add_help .l.t		"The participants in this session with you at the top.\nClick on a name\
                         with the left mouse button to display\ninformation on that participant,\
			 and with the middle\nbutton to mute that participant (the right button\nwill\
			 toggle the transmission mute button, as usual)."

add_help .st.opts   	"Brings up another window allowing\nthe control of various options."
add_help .st.about  	"Brings up another window displaying\ncopyright & author information."
add_help .st.quit   	"Press to leave the session."

# preferences help
add_help .prefs.m.f.m  "Click here to change the preference\ncategory."
set i .prefs.buttons
add_help $i.bye         "Cancel changes."
add_help $i.apply       "Apply changes."
#add_help $i.save        "Save and apply changes."

# user help
set i .prefs.pane.personal.a.f.f.ents
add_help $i.name      	"Enter your name for transmission\nto other participants."
add_help $i.email      	"Enter your email address for transmission\nto other participants."
add_help $i.phone     	"Enter your phone number for transmission\nto other participants."
add_help $i.loc      	"Enter your location for transmission\nto other participants."

#audio help
set i .prefs.pane.audio
add_help $i.dd.device.mdev "Selects preferred audio device."
add_help $i.dd.sampling.freq.mb \
                        "Sets the sampling rate of the audio device.\nThis changes the available codecs."
add_help $i.dd.sampling.ch_in.mb \
                        "Changes between mono and stereo audio input."
add_help $i.dd.cks.f.f.silence\
			 "Prevents silence from being transmitted when the speaker is silent\n\
                          and the input is unmuted."
add_help $i.dd.cks.f.f.agc	 "Enables automatic control of the volume\nof the sound you send."
add_help $i.dd.cks.f.f.loop "Enables hardware for loopback of audio input."
add_help $i.dd.cks.f.f.suppress \
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
add_help $i.cc.layer.fc.m  "Sets the number of discrete layers which will\nbe sent. You need\
						    to start RAT with the options\n-l n <address>/<port> <address>/<port>,\nwhere\
						    n is the number of layers and there is an\naddress and port for each layer.\
							NB: this is only\nsupported by the WBS codec at present."
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

} script_error

if { $script_error != "" } {
    puts "Error: \"$script_error\""
    destroy .
    exit -1
}
