#
#  The contents of this file are subject to the University of Utah Public
#  License (the "License"); you may not use this file except in compliance
#  with the License.
#  
#  Software distributed under the License is distributed on an "AS IS"
#  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
#  License for the specific language governing rights and limitations under
#  the License.
#  
#  The Original Source Code is SCIRun, released March 12, 2001.
#  
#  The Original Source Code was developed by the University of Utah.
#  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
#  University of Utah. All Rights Reserved.
#

itcl_class Teem_NrrdData_ChooseNrrd {
    inherit Module
    constructor {config} {
        set name ChooseNrrd

	global $this-port-index

        set_defaults
    }

    method set_defaults {} {
	set $this-port-index 0
    }

    method ui {} {
        set w .ui[modname]
        if {[winfo exists $w]} {
            raise $w
            return
        }
        toplevel $w

	frame $w.c
	pack $w.c -side top -e y -f both -padx 5 -pady 5
	
	label $w.c.l -text "Selected port: "
	entry $w.c.e -textvariable $this-port-index
	bind $w.c.e <Return> "$this-c needexecute"
	pack $w.c.l $w.c.e -side left
    }
}


