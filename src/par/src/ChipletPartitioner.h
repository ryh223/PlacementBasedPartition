#pragma once

#include <string>
#include <vector>
#include "odb/db.h"
#include "utl/Logger.h"
#include "SlicingTree.h"

namespace par {
    
typedef std::pair<float, float> aspect_ratio;
typedef std::pair<std::pair<float, float>, std::pair<float, float>> core_box;

class Chiplet{
    public:
        std::string name;
        std::vector<odb::dbInst*> instances;
        float width;
        float height;
        std::pair<float, float> location;
        utilization utilization_constaint;
        float aspect_ratio;
};

class ChipletPartitioner {
public:
    static ChipletPartitioner& getInstance(odb::dbDatabase* db, odb::dbBlock* block, utl::Logger* logger) {
        static ChipletPartitioner instance(db, block, logger);
        return instance;
    }

    static void deleteInstance() {
        ChipletPartitioner& instance = getInstance(nullptr, nullptr, nullptr);
        delete &instance;
    }

private:
    ChipletPartitioner(const ChipletPartitioner&) = delete;
    ChipletPartitioner& operator=(const ChipletPartitioner&) = delete;

    ChipletPartitioner(odb::dbDatabase* db, odb::dbBlock* block, utl::Logger* logger)
        : _db(db), _block(block), _logger(logger) {}

    ~ChipletPartitioner() {}

    

public:
    void initPhisicalConstraints(const std::string& physical_constraint_filename);

    void initModuleConstraints(const std::string& partition_constraint_filename);

    void run_partition();

    

private:
    core_box _core_box;
    long int _chiplet_area;
    int _num_chiplets;
    std::vector<utilization> _chiplet_utilization;
    std::vector<aspect_ratio> _chiplet_aspect_ratio;

    odb::dbDatabase* _db;
    odb::dbBlock* _block;
    utl::Logger* _logger;

    std::vector<ChipletBlock> initChipletBlocks();
    void run_simulated_annealing(int temp, int freeze_temp, int step, SlicingTree slicing_tree);
    float evaluate(SlicingTree slicing_tree, std::vector<Chiplet>& chiplet_boxes);
    float calculateScore(std::vector<Chiplet>& chiplet_boxes);

    // std::vector<type>  <module, vitual_macro>
};

}  // namespace par
