#pragma once

// #include <pair>
#include <vector>

namespace par {

typedef std::pair<float,float> utilization;
typedef std::pair<float,float> aspect_ratio;
typedef std::pair<std::pair<float,float>,std::pair<float,float>> core_box;

class ChipletPartitioner {
public:
    static ChipletPartitioner& getInstance() {
        static ChipletPartitioner instance; 
        return instance;
    }

    static void deleteInstance() {
        ChipletPartitioner& instance = getInstance();
        delete &instance;
    }

private:
    ChipletPartitioner(const ChipletPartitioner&) = delete;
    ChipletPartitioner& operator=(const ChipletPartitioner&) = delete;

    ChipletPartitioner() {}

    ~ChipletPartitioner() {}

public:
    void initPhisicalConstraints(core_box core_box, int chiplet_area, int num_chiplets, 
        std::vector<utilization> chiplet_utilization, std::vector<aspect_ratio> chiplet_aspect_ratio);

    //to do(kxy)
    void initPartitionConstaints();

    void run_partition();
private:
    core_box _core_box;
    int _chiplet_area;
    int _num_chiplets;
    std::vector<utilization> _chiplet_utilization;
    std::vector<aspect_ratio> _chiplet_aspect_ratio;
    
    // std::vector<type>  <module, vitual_macro>
    //to do(kxy): properties for partitionCONSTRAINTS
};

}



