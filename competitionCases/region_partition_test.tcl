source check_region_info.tcl

#region_partition_test
global_placement
macro_placement
run_partition -t0 1000 -tf 10 -step 10 -alpha 0.9