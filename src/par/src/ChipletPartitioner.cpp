#include "ChipletPartitioner.h"

#include "ChipletModuleWrapper.h"
#include "moduleMananger.h"
#include "odb/db.h"
#include "utl/Logger.h"

namespace par {
void ChipletPartitioner::initPhisicalConstraints(
    const std::string& physical_constraint_filename)
{
  std::ifstream file(physical_constraint_filename);
  if (!file.is_open()) {
    throw std::runtime_error("Unable to open file");
  }

  std::string line;
  std::getline(file, line);
  std::istringstream iss(line);
  int x1, y1, x2, y2;
  iss >> x1 >> y1 >> x2 >> y2;
  core_box CoreBox(std::pair<int, int>(x1, y1), std::pair<int, int>(x2, y2));

  std::getline(file, line);
  iss.str(line);
  long int chiplet_area;
  iss >> chiplet_area;

  int chiplet_num = 0;
  std::vector<std::pair<float, float>> chiplet_utilizations;
  std::vector<std::pair<float, float>> chiplet_aspect_ratios;
  while (std::getline(file, line)) {
    iss.str(line);
    float utilization_min, utilization_max, aspect_ratio_min, aspect_ratio_max;
    iss >> utilization_min >> utilization_max >> aspect_ratio_min
        >> aspect_ratio_max;
    chiplet_utilizations.emplace_back(utilization_min, utilization_max);
    chiplet_aspect_ratios.emplace_back(aspect_ratio_min, aspect_ratio_max);
    chiplet_num++;
  }

  _core_box = CoreBox;
  _chiplet_area = chiplet_area;
  _num_chiplets = chiplet_num;
  _chiplet_utilization = chiplet_utilizations;
  _chiplet_aspect_ratio = chiplet_aspect_ratios;
}
void ChipletPartitioner::initModuleConstraints(
    const std::string& partition_constraint_filename)
{
  ModuleManager* module_manager = new ModuleManager();
  module_manager->processFile(partition_constraint_filename);
  std::vector<std::vector<std::string>>& combination
      = module_manager->getCombine();
  std::vector<std::vector<std::string>>& abort = module_manager->getAbort();
  module_manager->printResults();
  ChipletModuleWrapper* chiplet_module_wrapper
      = new ChipletModuleWrapper(_db, _block, _logger, combination, abort);
  chiplet_module_wrapper->run();
  delete module_manager;
}

}  // namespace par
