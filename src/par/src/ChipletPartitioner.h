#pragma once
#include <string>
#include <vector>

#include "SlicingTree.h"
#include "odb/db.h"
#include "utl/Logger.h"

#define TEMPRAURE0 100
#define FREEZE_TEMPERATURE 10
#define STEP 10
#define ALPHA 0.9

namespace par {

typedef std::pair<double, double> aspect_ratio;
typedef std::pair<std::pair<double, double>, std::pair<double, double>> core_box;

class Chiplet
{
 public:
  std::string name;
  std::set<odb::dbInst*> instances;
  odb::uint width;
  odb::uint height;
  std::pair<odb::uint, odb::uint> location;
  utilization utilization_constaint;
  odb::uint inst_area;
 public:
  double getAspect_ratio() const { return double(height) / width; }
  double getArea() const { return double(width * height); }
  // this method will calculate the overlap area of the instance with the
  // chiplet over the total area of the instance  to see how likely the instance
  // will be placed in the chiplet
  double getOverlapRatio(odb::dbInst* inst);
  // this method  will calculate the utilization for the current partition
  double getUtilization();
};

class ChipletPartitioner
{
 public:
  static ChipletPartitioner& getInstance(odb::dbDatabase* db,
                                         odb::dbBlock* block,
                                         utl::Logger* logger)
  {
    static ChipletPartitioner instance(db, block, logger);
    return instance;
  }

  static void deleteInstance()
  {
    ChipletPartitioner& instance = getInstance(nullptr, nullptr, nullptr);
    delete &instance;
  }

 private:
  ChipletPartitioner(const ChipletPartitioner&) = delete;
  ChipletPartitioner& operator=(const ChipletPartitioner&) = delete;

  ChipletPartitioner(odb::dbDatabase* db,
                     odb::dbBlock* block,
                     utl::Logger* logger)
      : _db(db), _block(block), _logger(logger)
  {
  }

  ~ChipletPartitioner() {}

  void updateInsts(std::vector<Chiplet>& chiplet_boxes);

 public:
  void initPhisicalConstraints(const std::string& physical_constraint_filename);

  void initModuleConstraints(const std::string& partition_constraint_filename);

  void run_partition(double temp = TEMPRAURE0,
                     double freeze_temp = FREEZE_TEMPERATURE,
                     int step = STEP,
                     double alpha = ALPHA);

  core_box _core_box;
  long int _chiplet_area;
  int _num_chiplets;
  std::vector<utilization> _chiplet_utilization;
  std::vector<aspect_ratio> _chiplet_aspect_ratio;

  odb::dbDatabase* _db;
  odb::dbBlock* _block;
  utl::Logger* _logger;

  std::vector<ChipletBlock> initChipletBlocks();
  void run_simulated_annealing(int temp,
                               int freeze_temp,
                               int step,
                               double alpha,
                               SlicingTree slicing_tree);
  double evaluate(SlicingTree* slicing_tree,
                  std::vector<Chiplet>& chiplet_boxes);
  double calculateScore(std::vector<Chiplet>& chiplet_boxes);
};

}  // namespace par
