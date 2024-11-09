source check_region_info.tcl

#region_partition_test
global_placement
macro_placement
global_placement -skip_initial_place
run_partition -t0 1000 -tf 10 -step 10 -alpha 0.9
global_placement -skip_initial_place