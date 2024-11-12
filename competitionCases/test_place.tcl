source load_db.tcl
read_constraint_file -physical_constraint public_case1/physical_constraints.txt -partition_constraint public_case1/module_constraints.txt
global_placement
macro_placement
global_placement
run_partition -t0 1000 -tf 10 -step 10 -alpha 0.9
# global_placement -skip_initial_place
# global_placement -skip_initial_place -overflow 0.04 -density
# check_region_info
# detailed_placement
