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

void ChipletPartitioner::run_partition()
{
  // odb::dbSet<odb::dbInst> insts = _block->getInsts();
  // for (odb::dbInst* inst : insts) {
  //   std::string inst_name = inst->getName();
  //   odb::Point inst_pt = inst->getLocation();
  //   odb::uint inst_width = inst->getMaster()->getWidth();
  //   odb::uint inst_height = inst->getMaster()->getHeight();
  //   std::cout << "Instance name: " << inst_name << ";lb_pt: " << inst_pt << ";width: " << inst_width << ";height: " << inst_height << std::endl;
  // }
  std::vector<ChipletBlock> blocks = initChipletBlocks();
  SlicingTree* slicing_tree = new SlicingTree(_core_box.second.first - _core_box.first.first, _core_box.second.second - _core_box.first.second, blocks);
  int temp = 100;
  int freeze_temp = 10;
  int step = 10;
  run_simulated_annealing(temp, freeze_temp, step, *slicing_tree);
}

std::vector<ChipletBlock> ChipletPartitioner::initChipletBlocks()
{
  std::vector<ChipletBlock> blocks;
  for (int i = 0; i < _num_chiplets; i++) {
    ChipletBlock block(std::to_string(i), _chiplet_area, _chiplet_aspect_ratio[i].first,
                       _chiplet_aspect_ratio[i].second, 3, _chiplet_utilization[i]);
    blocks.push_back(block);
  }
  return blocks;
}

void ChipletPartitioner::run_simulated_annealing(int temp, int freeze_temp, int step, SlicingTree slicing_tree)
{
  //minimize score
  SlicingTree current_tree = slicing_tree;
  std::vector<Chiplet> current_solution;
  float current_score = evaluate(current_tree, current_solution);
  float best_score = current_score;
  std::vector<Chiplet> best_solition = current_solution;
  while (temp > freeze_temp) {
    for (int i = 0; i < step; i++) {
      SlicingTree new_tree = current_tree;
      new_tree.makeMove();
      float new_score = evaluate(new_tree, current_solution);
      float delta = new_score - current_score;
      if (delta < 0) {
        current_score = new_score;
        current_tree = new_tree;
        if (new_score < best_score) {
          best_score = new_score;
          best_solition = current_solution;
        }
      } else {
        float prob = exp(-delta / temp);
        if (rand() / RAND_MAX < prob) {
          current_score = new_score;
          current_tree = new_tree;
        }
      }
      temp--;
    }
  }
}

float ChipletPartitioner::evaluate(SlicingTree slicing_tree, std::vector<Chiplet>& chiplet_boxes)
{
  float best_score = std::numeric_limits<float>::max();
  for(int i = 0; i < slicing_tree.blocks.back()->shapes.size(); i++){
    std::vector<Chiplet> solution = slicing_tree.genetateSolution(i);
    if(solution.size() > 0){
      float score = calculateScore(solution);
      if(score < best_score){
        best_score = score;
        chiplet_boxes = solution;
      }
    }
  }
  return best_score;
}

float ChipletPartitioner::calculateScore(std::vector<Chiplet>& chiplet_boxes)
{
  float score = 0;
  return score;
}



}  // namespace par
