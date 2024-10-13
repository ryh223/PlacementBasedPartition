#include "ChipletPartitioner.h"

namespace par {
    void ChipletPartitioner::initPhisicalConstraints(core_box core_box, long int chiplet_area, int num_chiplets, 
        std::vector<utilization> chiplet_utilization, std::vector<aspect_ratio> chiplet_aspect_ratio) {
        _core_box = core_box;
        _chiplet_area = chiplet_area;
        _num_chiplets = num_chiplets;
        _chiplet_utilization = chiplet_utilization;
        _chiplet_aspect_ratio = chiplet_aspect_ratio;
    }

    
}