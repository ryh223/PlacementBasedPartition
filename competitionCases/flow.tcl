# check if db already loaded

source load_db.tcl

##read constraint and create regions

read_constraint_file -physical_constraint public_case1/physical_constraints.txt -partition_constraint public_case1/module_constraints.txt

##placemacro
# rtl_macro_placer -halo_width 10 -halo_height 10

global_placement -timing_driven -density 0.6

##chipletpartition


#estimate_parasitics -placement

puts "tns_late:  [format "%.3f" [sta::total_negative_slack -max]] ns"
puts "tns_early: [format "%.3f" [sta::total_negative_slack -min]] ns"
puts "wns_late:  [format "%.3f" [sta::worst_negative_slack -max]] ns"
puts "wns_early: [format "%.3f" [sta::worst_negative_slack -min]] ns"
