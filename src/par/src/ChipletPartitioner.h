#pragma once
#include <string>
#include <vector>

#include "SlicingTree.h"
#include "ChipletModuleWrapper.h"
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
  std::set<std::shared_ptr<ModuleConstraintGroup>> groups;
  std::set<odb::dbInst*> instances;
  double width;
  double height;
  std::pair<double, double> location;
  utilization utilization_constaint;
  double insts_area;
 public:
  double getAspect_ratio() const { return height / width; }
  double getArea() const { return width * height; }
  // this method will calculate the overlap area of the instance with the
  // chiplet over the total area of the instance  to see how likely the instance
  // will be placed in the chiplet
  double getOverlapRatio(odb::dbInst* inst);
  double getOverlapRatio(std::shared_ptr<ModuleConstraintGroup> module_group);
  // this method  will calculate the utilization for the current partition
  double getUtilization();
  bool isInChiplet(odb::dbInst* inst);
};

class ChipletPartitioner
{
 public:
  // Singleton instance retrieval
  static ChipletPartitioner& getInstance()
  {
    static ChipletPartitioner instance;
    return instance;
  }

  // Singleton instance deletion
  static void deleteInstance()
  {
    ChipletPartitioner& instance = getInstance();
    delete &instance;
  }

  // Initialization method
  void init(odb::dbDatabase* db, odb::dbBlock* block, utl::Logger* logger)
  {
    _db = db;
    _block = block;
    _logger = logger;
  }

 private:
  // Delete copy constructor and assignment operator
  ChipletPartitioner(const ChipletPartitioner&) = delete;
  ChipletPartitioner& operator=(const ChipletPartitioner&) = delete;

  // Private constructor
  ChipletPartitioner() = default;
  ChipletPartitioner(odb::dbDatabase* db,
                     odb::dbBlock* block,
                     utl::Logger* logger)
      : _db(db), _block(block), _logger(logger)
  {
  }

  // Destructor
  ~ChipletPartitioner() {}

  // Update instances with chiplet boxes
  void updateInsts(std::vector<Chiplet>& chiplet_boxes);

 public:
  // Initialize physical constraints from file
  void initPhisicalConstraints(const std::string& physical_constraint_filename);

  // Initialize module constraints from file
  void initModuleConstraints(const std::string& partition_constraint_filename);

  // Run partitioning algorithm
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

  // Initialize chiplet blocks
  std::vector<ChipletBlock> initChipletBlocks();

  // Run simulated annealing algorithm
  void run_simulated_annealing(int temp,
                               int freeze_temp,
                               int step,
                               double alpha,
                               SlicingTree* slicing_tree);

  // Evaluate the current slicing tree and chiplet boxes
  double evaluate(SlicingTree* slicing_tree,
                  std::vector<Chiplet>& chiplet_boxes);

  // Calculate score for chiplet boxes
  double calculateScore(std::vector<Chiplet>& chiplet_boxes);

  // Fine-tune the shape of chiplets
  void fineShape(SlicingTree* slicing_tree, std::vector<Chiplet>& chiplet_boxes);

  // Create regions for chiplets
  void chipletCreateRegions(std::vector<Chiplet>& chiplet_boxes, std::shared_ptr<ChipletRegionCreater> chiplet_region_creater);

  // Add blockages to chiplets
  void addBlockage(std::vector<Chiplet>& chiplet_boxes);

  // Reset macro placements
  void resetMacro();

  // Refine groups within chiplets
  void groupRefinement(std::vector<Chiplet>& chiplet_boxes);

  // Reassign module groups to chiplets
  void moduleGroupReAssignment(
      std::vector<Chiplet>& chiplet_boxes,
      std::vector<double>& area_target);

  // Reassign null groups to chiplets
  void nullGroupReAssignment(
      odb::dbGroup* group,
      const std::vector<double>& area,
      std::vector<odb::dbGroup*>& assignment);

  // Align chiplets
  void chipletAlign(std::vector<Chiplet>& chiplet_boxes);
};;

}  // namespace par
