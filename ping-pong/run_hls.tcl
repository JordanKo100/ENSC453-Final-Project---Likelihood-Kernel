#===================================
# run_hls.tcl for likelihood kernel
#===================================

open_project likelihood_pingpong.prj -reset
set_top likelihood_kernel

add_files likelihood_kernel.cpp
add_files likelihood_kernel.h
add_files -tb likelihood_tb.cpp

open_solution "solution1"
set_part {xcu50-fsvh2104-2-e}
create_clock -period 3.33

csim_design
csynth_design

close_project
quit
