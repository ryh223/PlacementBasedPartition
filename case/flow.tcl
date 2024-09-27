read_libraries
read_verilog $synth_verilog
link_design $top_module
read_sdc $sdc_file

set_thread_count [exec getconf _NPROCESSORS_ONLN]
# Temporarily disable sta's threading due to random failures
sta::set_thread_count 1

utl::metric "IFP::ord_version" [ord::openroad_git_describe]
# Note that sta::network_instance_count is not valid after tapcells are added.
utl::metric "IFP::instance_count" [sta::network_instance_count]

initialize_floorplan -site $site \
  -die_area $die_area \
  -core_area $core_area

################################################################
# IO Placement (random)

place_pins -random -hor_layers $io_placer_hor_layer -ver_layers $io_placer_ver_layer

################################################################
# create virtual macro via partition constraint

# create_virtual_macro (to do)

################################################################
# Macro Placement

if { [have_macros] } {
  global_placement -density $global_place_density
  macro_placement -halo $macro_place_halo -channel $macro_place_channel
}

################################################################
# Global placement

foreach layer_adjustment $global_routing_layer_adjustments {
  lassign $layer_adjustment layer adjustment
  set_global_routing_layer_adjustment $layer $adjustment
}
set_routing_layers -signal $global_routing_layers \
  -clock $global_routing_clock_layers
set_macro_extension 2

global_placement -timing_driven -density $global_place_density \
  -pad_left $global_place_pad -pad_right $global_place_pad

################################################################
# chiplet partition

# chiplet_partition (to do)

################################################################
# decluster virtual macro

# decluster_virtual_macro (to do)

################################################################
# replacement in chiplets

# replacement_in_chiplets (to do)



