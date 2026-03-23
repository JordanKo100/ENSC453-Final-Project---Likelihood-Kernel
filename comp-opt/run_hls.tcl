#===================================
# run_hls.tcl for likelihood kernel
#===================================

# open the HLS project likelihood_buffer.prj
open_project likelihood_compopt.prj -reset

# set the top-level function of the design to be likelihood_kernel
set_top likelihood_kernel

# add design files
add_files likelihood_kernel.cpp
add_files likelihood_kernel.h

# add test files
add_files -tb likelihood_tb.cpp

# open HLS solution solution1
open_solution "solution1"

# set target FPGA device: Alveo U50 in this example
set_part {xcu50-fsvh2104-2-e}

# target clock period is 300MHz
create_clock -period 3.33

# do a c simulation
csim_design

# synthesize the design
csynth_design

# do a co-simulation
# cosim_design

# close project and quit
close_project

# exit Vivado HLS
quit
