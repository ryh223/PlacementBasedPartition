#pragma once

#include <string>
#include <vector>
#include "odb/db.h"
#include "utl/Logger.h"

namespace par {

typedef std::pair<float, float> utilization;
typedef std::pair<float, float> aspect_ratio;
typedef std::pair<std::pair<float, float>, std::pair<float, float>> core_box;

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
    void initPhisicalConstraints(core_box core_box, long int chiplet_area, int num_chiplets,
                                 std::vector<utilization> chiplet_utilization, std::vector<aspect_ratio> chiplet_aspect_ratio);

    // to do(kxy)
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

    // std::vector<type>  <module, vitual_macro>
    // to do(kxy): properties for partitionCONSTRAINTS
};

}  // namespace par
