# Setting lef files
set tech_lef "pdk/lef/NangateOpenCellLibrary.tech.lef"
set std_lef "pdk/lef/NangateOpenCellLibrary.macro.mod.lef"
set lefs "
    pdk/lef/fakeram45_32x64.lef \
    pdk/lef/fakeram45_64x7.lef \
    pdk/lef/fakeram45_64x15.lef \
    pdk/lef/fakeram45_64x21.lef \
    pdk/lef/fakeram45_64x32.lef \
    pdk/lef/fakeram45_64x96.lef \
    pdk/lef/fakeram45_256x16.lef \
    pdk/lef/fakeram45_256x34.lef \
    pdk/lef/fakeram45_256x95.lef \
    pdk/lef/fakeram45_256x96.lef \
    pdk/lef/fakeram45_512x64.lef \
    pdk/lef/fakeram45_1024x32.lef \
    pdk/lef/fakeram45_2048x39.lef \
    pdk/lef/NangateOpenCellLibrary.macro.lef \
    pdk/lef/NangateOpenCellLibrary.macro.rect.lef \
"

# Setting lib files
set libs "
    pdk/lib/fakeram45_32x64.lib \
    pdk/lib/fakeram45_64x7.lib \
    pdk/lib/fakeram45_64x15.lib \
    pdk/lib/fakeram45_64x21.lib \
    pdk/lib/fakeram45_64x96.lib \
    pdk/lib/fakeram45_256x16.lib \
    pdk/lib/fakeram45_256x34.lib \
    pdk/lib/fakeram45_256x95.lib \
    pdk/lib/fakeram45_256x96.lib \
    pdk/lib/fakeram45_512x64.lib \
    pdk/lib/fakeram45_1024x32.lib \
    pdk/lib/fakeram45_2048x39.lib \
    pdk/lib/NangateOpenCellLibrary_typical.lib \
"

read_lef $tech_lef
read_lef $std_lef

foreach lef_file ${lefs} {
  read_lef $lef_file
}

foreach lib_file ${libs} {
  read_liberty $lib_file
}

read_verilog public_case1/input.v

link_design top

read_sdc public_case1/input.sdc

# Liberty units are fF,kOhm
set_layer_rc -layer metal1 -resistance 5.4286e-03 -capacitance 7.41819E-02
set_layer_rc -layer metal2 -resistance 3.5714e-03 -capacitance 6.74606E-02
set_layer_rc -layer metal3 -resistance 3.5714e-03 -capacitance 8.88758E-02
set_layer_rc -layer metal4 -resistance 1.5000e-03 -capacitance 1.07121E-01
set_layer_rc -layer metal5 -resistance 1.5000e-03 -capacitance 1.08964E-01
set_layer_rc -layer metal6 -resistance 1.5000e-03 -capacitance 1.02044E-01
set_layer_rc -layer metal7 -resistance 1.8750e-04 -capacitance 1.10436E-01
set_layer_rc -layer metal8 -resistance 1.8750e-04 -capacitance 9.69714E-02

set_wire_rc -signal -layer metal3
set_wire_rc -clock  -layer metal5

read_def -floorplan_initialize public_case1/input.def

par::printDesignInfo