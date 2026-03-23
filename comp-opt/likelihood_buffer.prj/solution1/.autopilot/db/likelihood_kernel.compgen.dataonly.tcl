# This script segment is generated automatically by AutoPilot

set axilite_register_dict [dict create]
set port_control {
Nparticles { 
	dir I
	width 32
	depth 1
	mode ap_none
	offset 16
	offset_end 23
}
countOnes { 
	dir I
	width 32
	depth 1
	mode ap_none
	offset 24
	offset_end 31
}
IszY { 
	dir I
	width 32
	depth 1
	mode ap_none
	offset 32
	offset_end 39
}
Nfr { 
	dir I
	width 32
	depth 1
	mode ap_none
	offset 40
	offset_end 47
}
k { 
	dir I
	width 32
	depth 1
	mode ap_none
	offset 48
	offset_end 55
}
max_size { 
	dir I
	width 64
	depth 1
	mode ap_none
	offset 56
	offset_end 67
}
arrayX { 
	dir I
	width 64
	depth 1
	mode ap_none
	offset 68
	offset_end 79
}
arrayY { 
	dir I
	width 64
	depth 1
	mode ap_none
	offset 80
	offset_end 91
}
objxy { 
	dir I
	width 64
	depth 1
	mode ap_none
	offset 92
	offset_end 103
}
I { 
	dir I
	width 64
	depth 1
	mode ap_none
	offset 104
	offset_end 115
}
likelihood { 
	dir I
	width 64
	depth 1
	mode ap_none
	offset 116
	offset_end 127
}
ap_start { }
ap_done { }
ap_ready { }
ap_idle { }
}
dict set axilite_register_dict control $port_control


