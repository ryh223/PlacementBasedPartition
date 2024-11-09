#pragma once

#include <string>
#include <vector>
#include "odb/db.h"
#include "utl/Logger.h"
#include "SlicingTree.h"

#define TEMPRAURE0 100
#define FREEZE_TEMPERATURE 10
#define STEP 10
#define ALPHA 0.9

namespace par {
    
typedef std::pair<double, double> aspect_ratio;
typedef std::pair<std::pair<double, double>, std::pair<double, double>> core_box;

class Chiplet{
    public:
        std::string name;
        std::vector<odb::dbInst*> instances;
        double width;
        double height;
        std::pair<double, double> location;
        utilization utilization_constaint;

    public:
        double getAspect_ratio() const { return height / width; }
        double getArea() const { return width * height; }
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

    void run_partition(double temp = TEMPRAURE0, double freeze_temp = FREEZE_TEMPERATURE, int step = STEP, double alpha = ALPHA);

    

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
    void run_simulated_annealing(int temp, int freeze_temp, int step, double alpha, SlicingTree slicing_tree);
    double evaluate(SlicingTree* slicing_tree, std::vector<Chiplet>& chiplet_boxes);
    double calculateScore(std::vector<Chiplet>& chiplet_boxes);

    // std::vector<type>  <module, vitual_macro>
};

}  // namespace par
