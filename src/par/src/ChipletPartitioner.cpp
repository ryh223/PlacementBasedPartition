#include "ChipletPartitioner.h"
#include "ChipletModuleWrapper.h"
#include "moduleMananger.h"
#include "odb/db.h"
#include "utl/Logger.h"
#include <queue>

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
  chipletAlign(best_solition);
  updateInsts(best_solition);
  // addBlockage(best_solition);
  resetMacro();
  odb::dbGroup* null_group = _block->findGroup("null_group");
  std::vector<double> area_target(best_solition.size(), 0);
  std::vector<odb::dbGroup*> assignment(best_solition.size(), nullptr);
  moduleGroupReAssignment(best_solition, area_target);
  nullGroupReAssignment(null_group, area_target, assignment);
  updateGroups(best_solition, assignment);
}

void ChipletPartitioner::updateGroups(std::vector<Chiplet>& chiplet_boxes, std::vector<odb::dbGroup*>& assignment)
{
  for (int i = 0; i < assignment.size(); i++){
    odb::dbGroup* group = assignment[i];
    if (group){
      chiplet_boxes[i].top_group->addGroup(group);
    }
    else {
      _logger->report("group is nullptr");
    }
  }
}

void ChipletPartitioner::groupRefinement(std::vector<Chiplet>& chiplet_boxes)
{
  // move the illegal insts
  // for insts in blocks check if the insts is in right region
  for (auto inst : _block->getInsts()){
    // move the inst to the right region
    // get top group
    odb::dbGroup* top_group = inst->getGroup();
    while (top_group->getParentGroup() != nullptr){
      top_group = top_group->getParentGroup();
    }
    int inst_x, inst_y;
    inst->getLocation(inst_x, inst_y);
    odb::dbRegion* region = inst->getRegion();
    int chiplet_idx = std::stoi(top_group->getName());
    if(region){
      odb::Rect rect = (*region->getBoundaries().begin())->getBox();
      if(inst_x < rect.xMin() || inst_x > rect.xMax() || inst_y < rect.yMin() || inst_y > rect.yMax()){
        // move the inst to the right region
        // get the chiplet box
        Chiplet& chiplet = chiplet_boxes[chiplet_idx];
        // get the chiplet box boundary
        int llx = chiplet.location.first;
        int lly = chiplet.location.second;
        int urx = llx + chiplet.width;
        int ury = lly + chiplet.height;
        // move the inst to the center of the chiplet box
        int new_x = (llx + urx) / 2;
        int new_y = (lly + ury) / 2;
        inst->setLocation(new_x, new_y);
        _logger->report("inst {} is moved to the center of the chiplet box {}", inst->getName(), chiplet.name);
      }
    }
    else{
      _logger->report("inst {} is not in any region", inst->getName());
    }
  }
}

void ChipletPartitioner::moduleGroupReAssignment(std::vector<Chiplet>& chiplet_boxes, std::vector<double>& area_target){
  // reassign the groups to different chiplet boxes
  // check the region utilization now
  // get the min area module_group and move it to the chiplet box with minimum utilization to balance

  auto& chiplet_module_wrapper = ChipletModuleWrapper::getInstance();
  auto module_groups = chiplet_module_wrapper.getModuleGroups();

  // Create a priority queue to store module groups based on their area and utilization
  auto cmp = [](std::shared_ptr<ModuleConstraintGroup> a, std::shared_ptr<ModuleConstraintGroup> b) {
    double ultilization_a = a->getChiplet()->getUtilization();
    double ultilization_b = b->getChiplet()->getUtilization();
    double area_a = a->getArea();
    double area_b = b->getArea();
    if (ultilization_a == ultilization_b) {
      return area_a < area_b;
    }
    return ultilization_a > ultilization_b;
  };
  std::priority_queue<std::shared_ptr<ModuleConstraintGroup>, std::vector<std::shared_ptr<ModuleConstraintGroup>>, decltype(cmp)> q(cmp);

  // Add all module groups to the priority queue
  for (auto& module_group : module_groups) {
    q.push(module_group);
  }

  int max_it = 500;
  int it = 0;
  while (!q.empty() && it < max_it) {
    it++;
    auto cur = q.top();
    q.pop();

    double max_gain = std::numeric_limits<double>::lowest();
    size_t best_chiplet_idx = 0;

    // Calculate move gain for each chiplet and choose the one with the maximum gain
    for (size_t i = 0; i < chiplet_boxes.size(); i++) {
      double gain = calculateMoveGain(cur, &chiplet_boxes[i]);
      std::cout << cur->getName()<< " move to chiplet " << i << " gain: " << gain << std::endl;
      if (gain > max_gain) {
        max_gain = gain;
        best_chiplet_idx = i;
      }
    }

    if (max_gain > 0) {
      // Move the current module group to the best chiplet and update
      moveModuleGroupToChiplet(cur, &chiplet_boxes[best_chiplet_idx]);
      q.push(cur);
    }
  }
}

double ChipletPartitioner::calculateMoveGain(std::shared_ptr<ModuleConstraintGroup> module_group, Chiplet* dest_chiplet) {
  Chiplet* source_chiplet = module_group->getChiplet();
  double min_util_dest = dest_chiplet->utilization_constaint.first;
  double max_util_dest = dest_chiplet->utilization_constaint.second;
  double min_util_source = source_chiplet->utilization_constaint.first;
  double max_util_source = source_chiplet->utilization_constaint.second;
  double move_gain = std::numeric_limits<double>::lowest();
  double dest_insts_area = dest_chiplet->insts_area;
  double dest_chiplet_area = dest_chiplet->getArea();
  double module_group_area = module_group->getArea();
  double source_chiplet_area = source_chiplet->getArea();
  double source_insts_area = source_chiplet->insts_area;
  double distance = std::abs(dest_chiplet->location.first - source_chiplet->location.first)
                    + std::abs(dest_chiplet->location.second - source_chiplet->location.second);
  double util_source = source_insts_area / source_chiplet_area;
  double new_util_source = (source_insts_area - module_group_area) / source_chiplet_area;
  double util_dest = dest_insts_area / dest_chiplet_area;
  double new_util_dest = (dest_insts_area + module_group_area) / dest_chiplet_area;
  // should matain the utilization of the chiplet in range of chiplet->utilization_constaint
  // calculate the squre
  double util_diff = util_dest - util_source;
  double new_util_diff = new_util_dest - new_util_source;
  double utilization_regulization = util_diff * util_diff - new_util_diff * new_util_diff;
  double distance_diff = 0.05 * distance / std::sqrt(dest_chiplet_area + source_chiplet_area);
  move_gain = utilization_regulization - distance_diff;
  std::cout << "utilization_regulization: " << utilization_regulization << " distance_diff: " << distance_diff << " move_gain: " << move_gain << std::endl;
  return move_gain;
}

void ChipletPartitioner::moveModuleGroupToChiplet(std::shared_ptr<ModuleConstraintGroup> module_group, Chiplet* chiplet) {
  // Implement the logic to move the module group to the chiplet
  // Update the chiplet's area, utilization, and other relevant properties
  chiplet->removeModuleGroup(module_group);
  chiplet->addModuleGroup(module_group);
  chiplet->top_group->addGroup(module_group->getGroup());
}

void ChipletPartitioner::nullGroupReAssignment(odb::dbGroup* group, const std::vector<double>& area, std::vector<odb::dbGroup*>& assignment){
  std::vector<std::pair<size_t, std::pair<int64_t, int64_t>>> cur_area_and_constraint;
  for(size_t i = 0; i < area.size(); i++){
    cur_area_and_constraint.push_back(std::make_pair(i, std::make_pair(0, area[i])));
  }
  std::sort(cur_area_and_constraint.begin(), cur_area_and_constraint.end(), [](const std::pair<size_t, std::pair<int64_t, int64_t>>& a, const std::pair<size_t, std::pair<int64_t, int64_t>>& b) {
    return a.second.second > b.second.second;
  });

  for (size_t i = 0; i < area.size(); i++) {
    odb::dbGroup* child_group = odb::dbGroup::create(group, fmt::format("sub_null_group_{}", i).c_str());
    assignment[i] = child_group;
  }

  std::set<odb::dbInst*> already_travel;
  double ratio = 0.9;
  // std::vector<odb::dbInst*> to_assign = group->getInsts();
  std::set<odb::dbInst*> to_assign;
  for(auto inst : group->getInsts()){
    to_assign.insert(inst);
  }

  auto can_place = [&](odb::dbInst* inst, size_t group_id) -> bool {
    if(cur_area_and_constraint[group_id].second.first + inst->getMaster()->getArea() > cur_area_and_constraint[group_id].second.second){
      return false;
    } else {
      return true;
    }
  };

  std::function<void(odb::dbModInst*, double, size_t)> add_module_insts_recursive;
  add_module_insts_recursive = [&](odb::dbModInst* mod_inst, double ratio, size_t group_id) {
      odb::dbModule* _module = mod_inst->getMaster();
      for(odb::dbModInst* child_mod_inst : _module->getChildren()){
        add_module_insts_recursive(child_mod_inst, ratio, group_id);
      }
      for(odb::dbInst* inst : _module->getLeafInsts()){
        assignment[cur_area_and_constraint[group_id].first]->addInst(inst);
        cur_area_and_constraint[group_id].second.first += inst->getMaster()->getArea();
      }
  };

  while (!to_assign.empty()) {
    size_t cur_group_id = 0;
    std::set<odb::dbInst*> no_assigned;
    for (odb::dbInst* inst : to_assign) {
      if (already_travel.find(inst) != already_travel.end()) {
        already_travel.erase(inst);
        continue;
      }
      already_travel.insert(inst);
      bool ifassigned = false;
      while (cur_group_id < area.size() && !ifassigned) {
        if (can_place(inst, cur_group_id)) {
          ifassigned = true;
          assignment[cur_area_and_constraint[cur_group_id].first]->addInst(inst);
          cur_area_and_constraint[cur_group_id].second.first += inst->getMaster()->getArea();
          odb::dbModule *module = inst->getModule();
          for(odb::dbModInst* mod_inst : module->getChildren()){
            add_module_insts_recursive(mod_inst, ratio, cur_group_id);
          }
          for(odb::dbInst* inst : module->getLeafInsts()){
            assignment[cur_area_and_constraint[cur_group_id].first]->addInst(inst);
            cur_area_and_constraint[cur_group_id].second.first += inst->getMaster()->getArea();
          }
          // add_module_insts_recursive(inst, ratio, cur_group_id);
        } else {
          cur_group_id++;
        }
      }
      if (!ifassigned) {
        no_assigned.insert(inst);
      }
    }
    ratio *= 1.1;
  }
}



void ChipletPartitioner::chipletAlign(std::vector<Chiplet>& chiplet_boxes)
{
  // align the chiplet boxes
  // in this method we want to do alignment for the chiplet to the nearest site
  // get site size from database
  odb::dbRow* row = *_block->getRows().begin();
  odb::dbSite* site = row->getSite();
  int site_size_x = site->getWidth();
  int site_size_y = site->getHeight();
  for (auto& chiplet : chiplet_boxes){
    // location, width, height are double at first
    int llx = chiplet.location.first;
    int lly = chiplet.location.second;
    // align the chiplet to the nearest site
    int new_llx = llx / site_size_x * site_size_x;
    int new_lly = lly / site_size_y * site_size_y;
    // consider the core box boundary
    if (new_llx < _core_box.first.first) {
      new_llx = _core_box.first.first;
    }
    if (new_lly < _core_box.first.second) {
      new_lly = _core_box.first.second;
    }
    int new_width = (int(chiplet.width) / site_size_x + 1) * site_size_x;
    int new_height = (int(chiplet.height) / site_size_y + 1) * site_size_y;
    // consider the core box boundary
    if (new_llx + new_width > _core_box.second.first) {
      new_width = int(_core_box.second.first) - new_llx;
    }
    if (new_lly + new_height > _core_box.second.second) {
      new_height = int(_core_box.second.second) - new_lly;
    }
    chiplet.location.first = new_llx;
    chiplet.location.second = new_lly;
    chiplet.width = new_width;
    chiplet.height = new_height;
  }
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
    chiplet_region_creater->createRegion(chiplet.name, groups, chiplet.location.first, chiplet.location.second, chiplet.location.first + chiplet.width, chiplet.location.second + chiplet.height);
    chiplet.top_group = _block->findGroup(chiplet.name.c_str());
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
  // check partition area
  for (auto& chiplet : chiplet_boxes) {
    if (chiplet.getArea() < _chiplet_area) {
      return std::numeric_limits<double>::max();
    }
  }
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
    chiplet_boxes[max_overlap_idx].addModuleGroup(module_inst);
  }
  // 2. for each macro, calculate the max overlap ratio with chiplet partition
  std::vector<int64_t> chiplet_insts_areas(num_chiplets, 0);
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
  // Calculate the average
  double total_insts_area = 0;
  double chiplets_area = 0;
  for (size_t i = 0; i < num_chiplets; i++) {
    auto& chiplet = chiplet_boxes[i];
    total_insts_area += chiplet.insts_area;
    chiplets_area += chiplet.getArea();
  }
  double average_utilization = total_insts_area / chiplets_area;
  for (size_t i = 0; i < num_chiplets; i++) {
    auto& chiplet = chiplet_boxes[i];
    utilization_diff_score += gamma * std::sqrt(std::abs(chiplet.getUtilization() - average_utilization));
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
