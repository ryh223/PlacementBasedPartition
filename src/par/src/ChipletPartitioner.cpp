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
  iss.clear();

  std::getline(file, line);
  iss.str(line);
  long int chiplet_area;
  iss >> chiplet_area;
  iss.clear();

  int chiplet_num = 0;
  std::vector<std::pair<double, double>> chiplet_utilizations;
  std::vector<std::pair<double, double>> chiplet_aspect_ratios;
  while (std::getline(file, line)) {
    iss.str(line);
    double utilization_min, utilization_max, aspect_ratio_min, aspect_ratio_max;
    iss >> utilization_min >> utilization_max >> aspect_ratio_min
        >> aspect_ratio_max;
    chiplet_utilizations.emplace_back(utilization_min, utilization_max);
    chiplet_aspect_ratios.emplace_back(aspect_ratio_min, aspect_ratio_max);
    chiplet_num++;
    iss.clear();
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
  std::shared_ptr<ModuleManager> module_manager = std::make_shared<ModuleManager>();
  module_manager->processFile(partition_constraint_filename);
  std::vector<std::vector<std::string>>& combination
      = module_manager->getCombine();
  std::vector<std::vector<std::string>>& abort = module_manager->getAbort();
  module_manager->printResults();
  ChipletModuleWrapper& chiplet_module_wrapper = ChipletModuleWrapper::getInstance();
  chiplet_module_wrapper.setOpenROAD(_db, _block, _logger);
  chiplet_module_wrapper.runWrap(combination, abort);
}

void ChipletPartitioner::run_partition(double temp, double freeze_temp, int step, double alpha)
{
  // odb::dbSet<odb::dbInst> insts = _block->getInsts();
  // for (odb::dbInst* inst : insts) {
  //   std::string inst_name = inst->getName();
  //   odb::Point inst_pt = inst->getLocation();
  //   int64_t inst_width = inst->getMaster()->getWidth();
  //   int64_t inst_height = inst->getMaster()->getHeight();
  //   std::cout << "Instance name: " << inst_name << ";lb_pt: " << inst_pt << ";width: " << inst_width << ";height: " << inst_height << std::endl;
  // }
  std::vector<ChipletBlock> blocks = initChipletBlocks();
  SlicingTree* slicing_tree = new SlicingTree(_core_box.second.first - _core_box.first.first, _core_box.second.second - _core_box.first.second, blocks);
  run_simulated_annealing(temp, freeze_temp, step, alpha, slicing_tree);
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

void ChipletPartitioner::run_simulated_annealing(int temp, int freeze_temp, int step, double alpha, SlicingTree* slicing_tree)
{
  auto& chiplet_module_wrapper = ChipletModuleWrapper::getInstance();
  // print some module info
  for (auto& module_group : chiplet_module_wrapper.getModuleGroups()) {
    // cout name, area of the module group
    std::cout << "Module group " << module_group->getName() << " size: " << module_group->getInsts().size() << " Area: " << module_group->getArea() << std::endl;
  }
  //minimize score
  // SlicingTree current_tree = SlicingTree(*slicing_tree);
  SlicingTree current_tree = *slicing_tree;
  std::cout << "current_tree: " << current_tree.blocks[0]->name << std::endl;
  std::vector<Chiplet> current_solution;
  double current_score = evaluate(&current_tree, current_solution);
  // std::cout << "current_tree: " << current_tree.blocks[0]->name << std::endl;
  double best_score = current_score;
  std::vector<Chiplet> best_solition = current_solution;

  SlicingTree new_tree;

  while (temp > freeze_temp) {
    for (int i = 0; i < step; i++) {
      new_tree = current_tree;
      new_tree.makeMove();
      double new_score = evaluate(&new_tree, current_solution);
      double delta = new_score - current_score;

      if (delta < 0) {
        current_score = new_score;
        current_tree = new_tree;
        if (new_score < best_score) {
          best_score = new_score;
          best_solition = current_solution;
        }
      } else {
        double prob = exp(-delta / temp);
        if (rand() / RAND_MAX < prob) {
          current_score = new_score;
          current_tree = new_tree;
        }
      } 

      // if(!accept){
      //   current_tree.refresh();
      // }
      temp *= alpha;
    }
    _logger->report("current best_score: {}", best_score);
  }
  for(Chiplet& chiplet : best_solition){
    _logger->report("chiplet: {} {} {} {} {}", chiplet.name, chiplet.location.first, chiplet.location.second, chiplet.width, chiplet.height);
  }
  updateInsts(best_solition);
  // addBlockage(best_solition);
  resetMacro();
  chipletAlign(best_solition);
  odb::dbGroup* null_group = _block->findGroup("null_group");
  std::vector<double> area_target(best_solition.size(), 0);
  std::vector<odb::dbGroup*> assignment(best_solition.size(), nullptr);
  moduleGroupReAssignment(best_solition, area_target);
  nullGroupReAssignment(null_group, area_target, assignment);
}

void ChipletPartitioner::groupRefinement(std::vector<Chiplet>& chiplet_boxes){
  // refine the chiplet boxes
}

void ChipletPartitioner::moduleGroupReAssignment(std::vector<Chiplet>& chiplet_boxes, std::vector<double>& area_target){
  // get the module group
} 

void ChipletPartitioner::nullGroupReAssignment(odb::dbGroup* group, const std::vector<double>& area, std::vector<odb::dbGroup*>& assignment){
  // assign region for the insts belongs to null group
}

void ChipletPartitioner::chipletAlign(std::vector<Chiplet>& chiplet_boxes)
{
  // align the chiplet boxes
}

void ChipletPartitioner::resetMacro(){
  for (odb::dbInst* inst : _block->getInsts()) {
    if (inst->getMaster()->isBlock()) {
      _logger->report("reset macro: {}", inst->getName());
      inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);
    }
  }
}

double ChipletPartitioner::evaluate(SlicingTree* slicing_tree, std::vector<Chiplet>& chiplet_boxes)
{
  double best_score = std::numeric_limits<double>::max();
  for(int i = 0; i < slicing_tree->blocks.back()->shapes.size(); i++){
    std::vector<Chiplet> solution = slicing_tree->genetateSolution(i, _core_box.first.first, _core_box.first.second);
    if(solution.size() > 0){
      double score = calculateScore(solution);
      if(score < best_score){
        best_score = score;
        // fineShape(slicing_tree, solution);
        chiplet_boxes = solution;
      }
    }
  }
  return best_score;
}

void ChipletPartitioner::fineShape(SlicingTree* slicing_tree, std::vector<Chiplet>& chiplet_boxes){
  // here we need to adjust the shape of the chiplet to legalize the solution and avoid the macro 
}

void ChipletPartitioner::addBlockage(std::vector<Chiplet>& chiplet_boxes){
  int HalfBlockageWidth = 380;
  // add blockage to the chiplet boxes
  for(auto& chiplet : chiplet_boxes){
    // get the boundary of the chiplet
    int llx = chiplet.location.first;
    int lly = chiplet.location.second;
    int urx = llx + chiplet.width;
    int ury = lly + chiplet.height;
    odb::dbBlockage::create(_block, llx - HalfBlockageWidth, lly - HalfBlockageWidth, llx + HalfBlockageWidth, ury + HalfBlockageWidth);
    odb::dbBlockage::create(_block, llx - HalfBlockageWidth, lly - HalfBlockageWidth, urx + HalfBlockageWidth, lly + HalfBlockageWidth);
    odb::dbBlockage::create(_block, urx - HalfBlockageWidth, lly - HalfBlockageWidth, urx + HalfBlockageWidth, ury + HalfBlockageWidth);
    odb::dbBlockage::create(_block, llx - HalfBlockageWidth, ury - HalfBlockageWidth, urx + HalfBlockageWidth, ury + HalfBlockageWidth);
  }
}

void ChipletPartitioner::updateInsts(std::vector<Chiplet>& chiplet_boxes){
  size_t num_chiplets = chiplet_boxes.size();
  std::map<std::string, int> wrapper_inst_partition;
  // get the chiplet module wrapper
  ChipletModuleWrapper& chiplet_module_wrapper = ChipletModuleWrapper::getInstance();
  for(auto& chiplet : chiplet_boxes){
    chiplet.instances.clear();
  }
  // divide instances into different chiplet boxes
  for (auto inst : _block->getInsts()) {
    size_t max_overlap_idx = 0;
    if (inst->getMaster()->isBlock()) {
      // divide instances into different chiplet boxes and then calculate the
      // score
      double max_overlap = 0;
      for (size_t i = 0; i < num_chiplets; i++) {
        auto& chiplet = chiplet_boxes[i];
        double overlap = chiplet.getOverlapRatio(inst);
        if (overlap > max_overlap) {
          max_overlap = overlap;
          max_overlap_idx = i;
        }
      }
      if (inst->getName().substr(0, 7) == "wrapped") {
        _logger->report("block inst: {} is in chiplet: {}",
                        inst->getName(),
                        chiplet_boxes[max_overlap_idx].name);
        wrapper_inst_partition[inst->getName()] = max_overlap_idx;
      }
    }
    else {
      // for standard cells, do not consider its area
      for (size_t i = 0; i < num_chiplets; i++) {
        auto& chiplet = chiplet_boxes[i];
        if (chiplet.isInChiplet(inst)) {
          max_overlap_idx = i;
          break;
        }
      }
    }
    // chiplet_boxes[max_overlap_idx].instances.insert(inst);
  }
  // update the chiplet_boxes
  chiplet_module_wrapper.runUnwrap();
  // update regions
  // update the chiplet boxes
  std::shared_ptr<ChipletRegionCreater> chiplet_region_creater = std::make_shared<ChipletRegionCreater>(_db, _block, _logger);
  chipletCreateRegions(chiplet_boxes, chiplet_region_creater);
}

void ChipletPartitioner::chipletCreateRegions(std::vector<Chiplet>& chiplet_boxes, std::shared_ptr<ChipletRegionCreater> chiplet_region_creater)
{
  for(auto& chiplet : chiplet_boxes){
    std::set<odb::dbGroup*> groups;
    for (auto module_inst : chiplet.groups){
      groups.insert(module_inst->getGroup());
    }
    auto region = chiplet_region_creater->createRegion(chiplet.name, groups, chiplet.location.first, chiplet.location.second, chiplet.location.first + chiplet.width, chiplet.location.second + chiplet.height);
  }
  odb::dbGroup* null_group = odb::dbGroup::create(_block, "null_group");
  for (auto inst : _block->getInsts()) {
    if (inst->getGroup() == nullptr) {
      null_group->addInst(inst);
    }
  }
}

// void ChipletPartitioner::reassignGroups()
// {
//   // move instances according to the insts area in the chiplet, make it balance
//   std::map<odb::dbModInst*, int64_t> mod_inst_area;
// }

double ChipletPartitioner::calculateScore(std::vector<Chiplet>& chiplet_boxes)
{
  // regularization parameters
  double alpha = 50.0;
  double beta = 10.0;
  double gamma = 100.0;
  double score = 0;
  double overlap_score = 0;
  double utilization_score = 0;
  double utilization_diff_score = 0;
  auto& chiplet_module_wrapper = ChipletModuleWrapper::getInstance();
  // what gonna to do here is to calculate metrics we define to determine the
  // quality of partition
  // 1. for each insts in module group calculate the overlap ratio with chiplet
  for (auto& chiplet : chiplet_boxes) {
    chiplet.insts_area = 0;
    chiplet.groups.clear();
  }
  size_t num_chiplets = chiplet_boxes.size();
  for (auto& module_inst : chiplet_module_wrapper.getModuleGroups()) {
    double max_overlap = 0;
    size_t max_overlap_idx = 0;
    for (size_t i = 0; i < num_chiplets; i++) {
      auto& chiplet = chiplet_boxes[i];
      double overlap = chiplet.getOverlapRatio(module_inst);
      if (overlap > max_overlap) {
        max_overlap = overlap;
        max_overlap_idx = i;
      }
    }
    overlap_score += alpha * std::abs(1 - max_overlap);
    chiplet_boxes[max_overlap_idx].groups.insert(module_inst);
    chiplet_boxes[max_overlap_idx].insts_area += module_inst->getArea();
  }
  // 2. for each macro, calculate the max overlap ratio with chiplet partition
  std::vector<int64_t> chiplet_insts_areas(num_chiplets, 0);
  for (auto& chiplet : chiplet_boxes) {
    if (chiplet.getArea() < _chiplet_area) {
      return std::numeric_limits<double>::max();
    }
  }
  for (auto inst : _block->getInsts()) {
    if(inst->getGroup() != nullptr && inst->getGroup()->getType() == odb::dbGroupType::PHYSICAL_CLUSTER){
      continue;
    }
    if (inst->getMaster()->isBlock()) {
      // divide instances into different chiplet boxes and then calculate the
      // score
      double max_overlap = 0;
      size_t max_overlap_idx = 0;
      for (size_t i = 0; i < num_chiplets; i++) {
        auto& chiplet = chiplet_boxes[i];
        double overlap = chiplet.getOverlapRatio(inst);
        if (overlap > max_overlap) {
          max_overlap = overlap;
          max_overlap_idx = i;
        }
      }
      overlap_score += beta * std::abs(1 - max_overlap);
      chiplet_insts_areas[max_overlap_idx] += inst->getMaster()->getArea();
    }
    else {
      // for standard cells, do not consider its overlap score
      for (size_t i = 0; i < num_chiplets; i++) {
        auto& chiplet = chiplet_boxes[i];
        if (chiplet.isInChiplet(inst)) {
          chiplet_insts_areas[i] += inst->getMaster()->getArea();
          break;
        }
      }
    }
  }
  _logger->report(" Overlap exceed score: {}", overlap_score);
  score += overlap_score;

  // 2. for each chiplet partition calculate the utilization ratio
  for (size_t i = 0; i < num_chiplets; i++) {
    auto& chiplet = chiplet_boxes[i];
    chiplet.insts_area += chiplet_insts_areas[i];
  }
  _logger->report(" Utilization exceed score: {}", utilization_score);
  score += utilization_score;

  // Add a score to penalize the utilization difference between chiplets
  for (size_t i = 0; i < num_chiplets; i++) {
    for (size_t j = i + 1; j < num_chiplets; j++) {
      double utilization_diff = std::abs(chiplet_boxes[i].getUtilization() - chiplet_boxes[j].getUtilization());
      utilization_diff_score += gamma * utilization_diff;
    }
  }
  _logger->report(" Utilization difference score: {}", utilization_diff_score);
  score += utilization_diff_score;

  return score;
}

// this method will calculate the overlap area of the instance with the chiplet
// over the total area of the instance  to see how likely the instance will be
// placed in the chiplet
double Chiplet::getOverlapRatio(odb::dbInst* inst)
{
  int inst_x, inst_y;
  inst->getLocation(inst_x, inst_y);
  double inst_width = inst->getMaster()->getWidth();
  double inst_height = inst->getMaster()->getHeight();
  double inst_area = inst->getMaster()->getArea(); 
  int chiplet_x, chiplet_y;
  chiplet_x = location.first;
  chiplet_y = location.second;
  double chiplet_width = width;
  double chiplet_height = height;
  double overlap = 0;

  // inst and chiplet
  if (inst_x + inst_width > chiplet_x && inst_x < chiplet_x + chiplet_width &&
      inst_y + inst_height > chiplet_y && inst_y < chiplet_y + chiplet_height) {
    double overlap_x = std::min(inst_x + inst_width, chiplet_x + chiplet_width) -
                    std::max(inst_x, chiplet_x);
    double overlap_y = std::min(inst_y + inst_height, chiplet_y + chiplet_height) -
                    std::max(inst_y, chiplet_y);
    overlap = overlap_x * overlap_y;
  }
  return overlap / inst_area;
}

double Chiplet::getOverlapRatio(std::shared_ptr<ModuleConstraintGroup> module_group)
{
  int64_t overlap = 0;
  for (auto inst : module_group->getGroup()->getInsts()) {
    if (isInChiplet(inst)) {
      overlap += inst->getMaster()->getArea();
    }
  }
  return double(overlap) / module_group->getArea();
}

// this method  will calculate the utilization for the current partition
double Chiplet::getUtilization()
{
  return insts_area / getArea();
}

bool Chiplet::isInChiplet(odb::dbInst* inst)
{
  int inst_x, inst_y;
  inst->getLocation(inst_x, inst_y);
  if (inst->getMaster()->isBlock()){
    inst_x = inst_x + inst->getMaster()->getWidth() / 2;
    inst_y = inst_y + inst->getMaster()->getHeight() / 2;
    return inst_x >= location.first && inst_x <= location.first + width &&
         inst_y >= location.second && inst_y <= location.second + height;
  }
  return inst_x >= location.first && inst_x <= location.first + width &&
         inst_y >= location.second && inst_y <= location.second + height;
}

}  // namespace par
