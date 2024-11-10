source load_db.tcl

read_constraint_file -physical_constraint public_case1/physical_constraints.txt -partition_constraint public_case1/module_constraints.txt

#region_partition_test
global_placement
global_placement -skip_initial_place
global_placement -skip_initial_place
run_partition -t0 1000 -tf 10 -step 10 -alpha 0.9
global_placement -skip_initial_place
macro_placement
global_placement -skip_initial_place
detail_placement