itcl_class VectorSeg {
    inherit Module
    constructor {config} {
	set name VectorSeg
	set_defaults
    }
    method set_defaults {} {
	set material(1,name) skin
	set material(1,min) 0
	set material(1,max) 0
	set material(2,name) bone
	set material(2,min) 0
	set material(2,max) 0
	set material(3,name) fluid
	set material(3,min) 0
	set material(3,max) 0
	set material(4,name) greyMatter
	set material(4,min) 0
	set material(4,max) 0
	set material(5,name) whiteMatter
	set material(5,min) 0
	set material(5,max) 0
	global $this-numFields
	set $this-numFields 0
    }
    method ui {} {
	set w .ui$this
	if {[winfo exists $w]} {
	    raise $w
	    return;
	}
	toplevel $w
	wm minsize $w 300 100
	frame $w.f -width 400 -height 500
	pack $w.f -padx 2 -pady 2 -side top -fill x -expand yes
	set n "$this-c needexecute "
	global $this-numFields
puts "In VectorSeg"
puts [set $this-numFields]
	set i 0
	while {$i<[set $this-numFields]} {
	    incr i
	    frame $w.f.f$i -relief groove -borderwidth 2
	    pack $w.f.f$i -side left -fill x -expand 1
	    checkbutton $w.f.f$i.label -text "FIELD $i" -variable $this-f$i \
		    -command "$this do_field $i"
	    $w.f.f$i.label select
	    frame $w.f.f$i.ranges
	    pack $w.f.f$i.label $w.f.f$i.ranges -side top -fill x -expand 1
	    set mat 0
	    while {$mat < 5} {
		incr mat
		set name $material($mat,name)
		set min $material($mat,min)
		set max $material($mat,max)
		global $this-f${i}m${mat}min
		global $this-f${i}m${mat}max
		set lbl "$name range:"
		range $w.f.f$i.ranges.$name -from 0 -to 255 -label $lbl \
			-showvalue true -tickinterval 51 -var_min \
			$this-f${i}m${mat}min -var_max $this-f${i}m${mat}max \
			-orient horizontal -length 250
		$w.f.f$i.ranges.$name setMinMax $min $max
		pack $w.f.f$i.ranges.$name -side top -fill x -expand 1
	    }
	}
    }
    method do_field {i} {
	global $this-f$i
	set w .ui$this
	if {[set $this-f$i]} {
	    set mat 0
	    while {$mat < 5} {
		incr mat
		set name $material($mat,name)
		$w.f.f$i.ranges.$name configure -state normal
	    }
	} else {
	    set mat 0
	    while {$mat < 5} {
		incr mat
		set name $material($mat,name)
		$w.f.f$i.ranges.$name configure -state disabled
	    }
	}
    }
    protected material
    protected num_fields
}
