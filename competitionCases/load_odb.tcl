# Setting lef files
read_db case1.odb

par::printDesignInfo

read_constaint_file -physical_constraint public_case1/physical_constraints.txt -partition_constraint public_case1/module_constraints.txt