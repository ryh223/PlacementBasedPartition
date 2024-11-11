source load_db.tcl
read_constraint_file -physical_constraint public_case1/physical_constraints.txt -partition_constraint public_case1/module_constraints.txt
global_placement
global_placement -skip_initial_place
global_placement -incremental
macro_placement -halo {20 20}
global_placement -incremental
run_partition -t0 1000 -tf 1 -step 1 -alpha 0.9
