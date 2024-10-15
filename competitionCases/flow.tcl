source load_db.tcl

##read constraint

##create macro

##placemacro
rtl_macro_placement -halo_width 10 -halo_height 10 #(recommended)
macro_palcement -halo {10 10} -channel {20 20}

global_placement -timing_driven -density 0.6 -overflow   -pad_left    -pad_right

##chipletpartition


#estimate_parasitics -placement

#puts "tns_late:  [format "%.3f" [sta::total_negative_slack -max]] ns"
#puts "tns_early: [format "%.3f" [sta::total_negative_slack -min]] ns"
#puts "wns_late:  [format "%.3f" [sta::worst_negative_slack -max]] ns"
#puts "wns_early: [format "%.3f" [sta::worst_negative_slack -min]] ns"
