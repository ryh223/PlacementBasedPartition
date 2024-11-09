#include "ChipletModuleWrapper.h"

#include <fstream>
#include <iostream>
#include <set>
#include <unordered_set>

namespace par {

bool ModuleConstraintGroup::collapseBlock(odb::dbInst* block_inst)
{
  // reccreate the insts in the top block and recreate the connection
  std::cout << "Collapsing " << wrapped_inst_->getName() << std::endl;
  wrapped_inst_ = block_inst;
  // old insts map to new insts
  std::map<odb::dbInst*, odb::dbInst*> old_new_insts_map;
  child_block_ = block_inst->getChild();
  block_name_ = block_inst->getName();
  odb::dbBlock* top_block = block_inst->getBlock();
  for (auto inst : child_block_->getInsts()) {
    old_new_insts_map[inst] = odb::dbInst::create(
        top_block, inst->getMaster(), inst->getName().c_str(), true);
  }
  insts_.clear();
  for (auto& [old_inst, new_inst] : old_new_insts_map) {
    insts_.insert(new_inst);
  }
  // get nets connect to wrapper inst
  std::set<odb::dbNet*> cross_nets;
  for (auto iterm : wrapped_inst_->getITerms()) {
    odb::dbNet* net = iterm->getNet();
    if (cross_nets.find(net) != cross_nets.end()) {
      continue;
    }
    cross_nets.insert(net);
  }
  // reconnect the cross nets to the insts recreate in the child block
  for (auto net : cross_nets) {
    odb::dbNet* inner_cross_net = child_block_->findNet(net->getName().c_str());
    for (auto iterm : inner_cross_net->getITerms()) {
      auto mterm = iterm->getMTerm();
      auto originst = iterm->getInst();
      old_new_insts_map[originst]->getITerm(mterm)->connect(net);
    }
    odb::dbNet::destroy(inner_cross_net);
  }
  // reconnect the inner nets in child block to recovered insts
  for (auto net : child_block_->getNets()) {
    odb::dbNet* new_net = odb::dbNet::create(top_block, net->getName().c_str());
    for (auto iterm : net->getITerms()) {
      auto mterm = iterm->getMTerm();
      auto originst = iterm->getInst();
      old_new_insts_map[originst]->getITerm(mterm)->connect(new_net);
    }
    odb::dbNet::destroy(net);
  }
  // for (auto inst : child_block_->getInsts()) {
  //   odb::dbInst::destroy(inst);
  // }
  odb::dbBlock::destroy(child_block_);
  odb::dbMaster* wrapped_inst_master = wrapped_inst_->getMaster();
  odb::dbInst::destroy(wrapped_inst_);
  odb::dbMaster::destroy(wrapped_inst_master);
  return true;
}

bool ModuleConstraintGroup::createBlock(odb::dbBlock* top_block)
{
  int mpin_halo = 10;
  // travel the insts to get the area of the block
  DEBUG_PRINT("Calculating area of the block...");
  for (auto& inst : insts_) {
    odb::dbMaster* master = inst->getMaster();
    DEBUG_PRINT("Instance: " << inst->getName()
                             << " Master: " << master->getName());
    area_ += master->getArea();
  }
  height_ = width_ = int64_t(sqrt(area_));
  DEBUG_PRINT("Total area: " << area_);
  DEBUG_PRINT("Block height: " << height_ << " width: " << width_);
  // get cross nets that connect the insts in the group and the insts outside
  // copy insts to child block
  DEBUG_PRINT("Copying instances to child block...");
  // old insts map to new insts
  std::map<odb::dbInst*, odb::dbInst*> old_new_insts_map;
  for (auto inst : insts_) {
    old_new_insts_map[inst] = odb::dbInst::create(
        child_block_, inst->getMaster(), inst->getName().c_str(), true);
  }
  std::set<odb::dbNet*> cross_nets;
  std::set<odb::dbNet*> inner_nets;
  DEBUG_PRINT("Identifying cross nets and inner nets...");
  for (auto inst : insts_) {
    DEBUG_PRINT("Processing instance: " << inst->getName());
    for (auto iterm : inst->getITerms()) {
      odb::dbNet* net = iterm->getNet();
      if (!net) {
        // DEBUG_PRINT("Net is null " << inst->getName() << " "
        //                           << iterm->getMTerm()->getName());
        continue;
      }
      if (cross_nets.find(net) != cross_nets.end()
          || inner_nets.find(net) != inner_nets.end()) {
        continue;
      }
      if (net->getBTerms().size() != 0) {
        DEBUG_PRINT("Net connected to block terminals: " << net->getName());
        cross_nets.insert(net);
        continue;
      }
      bool net_inside = true;
      for (auto iterm : net->getITerms()) {
        if (insts_.find(iterm->getInst()) == insts_.end()) {
          DEBUG_PRINT("Net connected to instance outside the group: "
                      << net->getName());
          cross_nets.insert(net);
          net_inside = false;
          break;
        }
      }
      if (net_inside) {
        DEBUG_PRINT(
            "Net connected to instance within the group: " << net->getName());
        inner_nets.insert(net);
      }
    }
  }
  DEBUG_PRINT("Number of cross nets identified: " << cross_nets.size());
  DEBUG_PRINT("Number of inner nets identified: " << inner_nets.size());
  // spilt the cross net into two nets, one is outside, one is inside
  DEBUG_PRINT("Splitting cross nets...");
  std::map<odb::dbNet*, odb::dbBTerm*> outside_net_bterm_map;
  for (auto net : cross_nets) {
    odb::dbNet* outside_net
        = net;  // the net in the parent block is the outside net
    odb::dbNet* inside_net
        = odb::dbNet::create(child_block_, outside_net->getName().c_str());
    if (!inside_net) {
      std::cerr << "Failed to create net in child block" << std::endl;
      return false;
    }
    for (odb::dbITerm* iterm : outside_net->getITerms()) {
      if (insts_.find(iterm->getInst()) != insts_.end()) {
        auto mterm = iterm->getMTerm();
        auto originst = iterm->getInst();
        old_new_insts_map[originst]->getITerm(mterm)->connect(inside_net);
      } else {
        if (outside_net_bterm_map.find(outside_net)
            == outside_net_bterm_map.end()) {
          odb::dbBTerm* bterm = odb::dbBTerm::create(
              inside_net, outside_net->getName().c_str());
          outside_net_bterm_map[outside_net] = bterm;
        }
      }
    }
  }
  DEBUG_PRINT("Cross nets split successfully");
  DEBUG_PRINT("Inner nets identified, creating nets in child block...");
  for (auto net : inner_nets) {
    odb::dbNet* inside_net
        = odb::dbNet::create(child_block_, net->getName().c_str());
    if (!inside_net) {
      std::cerr << "Failed to create net in child block" << std::endl;
      return false;
    }
    for (auto iterm : net->getITerms()) {
      if (insts_.find(iterm->getInst()) != insts_.end()) {
        auto mterm = iterm->getMTerm();
        auto originst = iterm->getInst();
        old_new_insts_map[originst]->getITerm(mterm)->connect(inside_net);
      }
    }
    odb::dbNet::destroy(net);
  }
  // destroy the insts in the top block
  for (auto inst : insts_) {
    odb::dbInst::destroy(inst);
  }
  DEBUG_PRINT("Creating wrapper instance with name: " << block_name_);
  wrapped_inst_
      = odb::dbInst::create(top_block, child_block_, block_name_.c_str());
  if (!wrapped_inst_) {
    std::cerr << "Failed to create wrapper instance" << std::endl;
    return false;
  }
  DEBUG_PRINT("Wrapped Inst has ITerms: " << wrapped_inst_->getITerms().size());
  // connect cross nets to the wrapper inst
  DEBUG_PRINT("Connecting cross nets to the wrapper instance...");
  for (auto it = outside_net_bterm_map.begin();
       it != outside_net_bterm_map.end();
       ++it) {
    odb::dbNet* outside_net = it->first;
    odb::dbBTerm* bterm = it->second;
    DEBUG_PRINT("Connecting net: " << outside_net->getName()
                                   << " to bterm: " << bterm->getName());
    odb::dbITerm* iterm = bterm->getITerm();
    DEBUG_PRINT("ITerm found: " << iterm->getMTerm()->getName());
    iterm->connect(outside_net);
  }
  // reassign the dbInst set
  insts_.clear();
  for (auto& [old_inst, new_inst] : old_new_insts_map) {
    insts_.insert(new_inst);
  }
  // set the pin location for master pins and block boundary
  odb::dbMaster* wrapped_inst_master = wrapped_inst_->getMaster();
  wrapped_inst_master->setWidth(width_);
  wrapped_inst_master->setHeight(height_);
  odb::dbTechLayer* pinlayer = top_block->getTech()->findLayer(
      top_block->getTech()->getRoutingLayerCount());
  for (auto mterm : wrapped_inst_master->getMTerms()) {
    // dbBox* dbBox::create(dbMPin* pin_, dbTechLayer* layer_, int x1, int y1,
    // int x2, int y2)
    odb::dbMPin* pin = odb::dbMPin::create(mterm);
    odb::dbBox::create(pin,
                         pinlayer,
                         width_ / 2 - mpin_halo,
                         height_ / 2 - mpin_halo,
                         width_ / 2 + mpin_halo,
                         height_ / 2 + mpin_halo);
  }
  DEBUG_PRINT("Wrapper instance bounding box created successfully\n");
  return true;
}

ChipletModuleWrapper::~ChipletModuleWrapper()
{
  // Destructor implementation (if needed)
}

void ChipletModuleWrapper::printModuleInfo(std::string file_name)
{
  // Print module information
  std::ofstream ofs(file_name);
  if (!ofs.is_open()) {
    _logger->report("Failed to open file: {}", file_name);
    return;
  }

  ofs << "Module information:\n";
  // Iterate over modules
  for (const auto& dbmodule : _module_groups) {
    ofs << "Module name: " << dbmodule->getName() << "\n";
    ofs << "Number of instances: " << dbmodule->getInsts().size() << "\n";
    // Iterate over instances
    for (const auto& inst : dbmodule->getInsts()) {
      ofs << "  Instance name: " << inst->getName() << "\n";
      ofs << "  Master name: " << inst->getMaster()->getName() << "\n";
      ofs << "  Instance location: " << inst->getLocation().getX() << " x "
          << inst->getLocation().getY() << "\n";
      if (inst->isBlock()) {
        ofs << "  IsBlock\n";
      }
    }
  }

  ofs.close();
}

void ChipletModuleWrapper::printDesignInfo(std::string file_name)
{
  std::ofstream ofs(file_name);
  if (!ofs.is_open()) {
    _logger->report("Failed to open file: {}", file_name);
    return;
  }

  // Print design information
  ofs << "Design information:\n";
  ofs << "Block name: " << _block->getName() << "\n";
  // Calculate block area from bounding box
  int llx, lly, urx, ury;
  odb::dbBox* bbox = _block->getBBox();
  if (bbox) {
    llx = bbox->xMin();
    lly = bbox->yMin();
    urx = bbox->xMax();
    ury = bbox->yMax();
  }
  int width = urx - llx;
  int height = ury - lly;
  ofs << "Block area: " << width << " x " << height << "\n";
  // Number of instances
  ofs << "Number of instances: " << _block->getInsts().size() << "\n";
  // Iterate over instances
  for (auto inst : _block->getInsts()) {
    ofs << "Instance name: " << inst->getName() << "\n";
    ofs << "Master name: " << inst->getMaster()->getName() << "\n";
    ofs << "Instance location: " << inst->getLocation().getX() << " x "
        << inst->getLocation().getY() << "\n";
    if (inst->isBlock()) {
      ofs << "IsBlock\n";
    }
  }
  // Print module information
  for (auto mod_inst : _block->getModules()) {
    ofs << "Module name: " << mod_inst->getName() << "\n";
    ofs << "    Number of instances in module: " << mod_inst->getInsts().size()
        << "\n";
    ofs << "    Number of children modules: " << mod_inst->getChildren().size()
        << "\n";
    // std::cout << "  Module name: " << mod_inst->getName() << "\n";
  }

  ofs.close();
}

bool ChipletModuleWrapper::initModuleGroups(
    std::vector<std::vector<std::string>>& combination,
    std::vector<std::vector<std::string>>& abort)
{
  // Initialize the module groups with combination and abort
  _logger->report("Initializing module groups, {} combinations and {} aborts",
                  combination.size(),
                  abort.size());
  // Initialize _module_groups based on some logic involving combination and
  // abort Method: create a module group for each combination, and add the
  // instances pointers
  size_t combination_size = combination.size();
  size_t abort_size = abort.size();
  if (combination_size == 0) {
    _logger->report("No combination found");
    return true;
  }
  if (combination_size != abort_size) {
    _logger->report("The number of combination and abort is not equal");
    return false;
  }
  // Add "top/" prefix to all elements in combination and abort vectors
  for (auto& comb : combination) {
    for (auto& module_name : comb) {
      module_name = "top/" + module_name;
      std::cout << "Combination module name updated to: " << module_name
                << std::endl;
    }
  }
  for (auto& ab : abort) {
    for (auto& module_name : ab) {
      module_name = "top/" + module_name;
      std::cout << "Abort module name updated to: " << module_name << std::endl;
    }
  }
  for (size_t i = 0; i < combination_size; i++) {
    std::string wrapped_module_name = fmt::format("wrapped_{}", i);
    std::shared_ptr<ModuleConstraintGroup> module_group
        = std::make_shared<ModuleConstraintGroup>(_block, wrapped_module_name);
    // Mark the insts in the abort list
    std::unordered_set<odb::dbInst*> abort_insts;
    for (const auto& module_name : abort[i]) {
      auto abort_module_name = module_name;
      std::replace(
          abort_module_name.begin(), abort_module_name.end(), '/', '.');
      odb::dbModule* db_module = _block->findModule(abort_module_name.c_str());
      if (db_module) {
        for (odb::dbInst* inst : db_module->getInsts()) {
          abort_insts.insert(inst);
        }
      } else {
        _logger->report("Module {} not found in abort list", abort_module_name);
        return false;
      }
    }
    // Add instances to the module group, skipping those in the abort list
    for (const auto& module_name : combination[i]) {
      auto combination_module_name = module_name;
      std::replace(combination_module_name.begin(),
                   combination_module_name.end(),
                   '/',
                   '.');
      odb::dbModule* db_module
          = _block->findModule(combination_module_name.c_str());
      if (db_module) {
        for (odb::dbInst* inst : db_module->getInsts()) {
          if (abort_insts.find(inst) == abort_insts.end()) {
            module_group->addInst(inst);
          } else {
            _logger->report(
                "Instance {} is in the abort list and will be skipped",
                inst->getName());
          }
        }
      } else {
        _logger->report("Module {} not found in combination list",
                        combination_module_name);
        return false;
      }
    }
    _module_groups.insert(module_group);
  }
  return true;
}

void ChipletModuleWrapper::wrapModule(
    std::shared_ptr<ModuleConstraintGroup> module_group)
{
  _logger->report("Wrapping module group: {}", module_group->getName());
  module_group->createBlock(_block);
}

void ChipletModuleWrapper::unwrapModule(
    std::shared_ptr<ModuleConstraintGroup> module_group)
{
  _logger->report("Unwrapping module group: {}", module_group->getName());
  module_group->collapseBlock(module_group->getWrappedInst());
}

void ChipletModuleWrapper::runWrap(
    std::vector<std::vector<std::string>>& combination,
    std::vector<std::vector<std::string>>& abort)
{
  if (initModuleGroups(combination, abort)) {
    _logger->report("Module groups initialized, {} groups created",
                    _module_groups.size());
  } else {
    _logger->report("Failed to initialize module groups");
  }
  // Run the wrapping and unwrapping process
  for (auto& module_group : _module_groups) {
    if (module_group->getInsts().size() == 1 || module_group->getInsts().empty()) {
      _logger->report("Module group {} skip unwrapping", module_group->getName());
      continue;
    }
    wrapModule(module_group);
  }
}

void ChipletModuleWrapper::runUnwrap()
{
  _logger->report("Unwrapping all module groups");
  for (auto& module_group : _module_groups) {
    if (module_group->getInsts().size() == 1 || module_group->getInsts().empty()) {
      _logger->report("Module group {} skip unwrapping", module_group->getName());
      continue;
    }
    unwrapModule(module_group);
  }
}

void ChipletModuleWrapper::Test()
{
}

ChipletRegionCreater::ChipletRegionCreater(odb::dbDatabase* db,
                                           odb::dbBlock* block,
                                           utl::Logger* logger)
    : _db(db), _block(block), _logger(logger)
{
}

ChipletRegionCreater::~ChipletRegionCreater()
{
  // Destructor implementation (if needed)
}

odb::dbGroup* ChipletRegionCreater::createGroup(std::string group_name,
                                                std::set<odb::dbInst*>& insts)
{
  odb::dbGroup* group = odb::dbGroup::create(_block, group_name.c_str());
  if (!group) {
    _logger->report("Failed to create group: {}", group_name);
    return nullptr;
  }
  _logger->report("Group {} created successfully", group_name);
  for (auto inst : insts) {
    group->addInst(inst);
  }
  return group;
}

odb::dbRegion* ChipletRegionCreater::createRegion(std::string region_name,
                                                  odb::dbGroup* group,
                                                  int64_t xMin,
                                                  int64_t yMin,
                                                  int64_t xMax,
                                                  int64_t yMax)
{
  odb::dbRegion* region = odb::dbRegion::create(_block, region_name.c_str());
  // region->setRegionType(odb::dbRegionType::SUGGESTED);
  if (!region) {
    _logger->report("Failed to create region: {}", region_name);
    return nullptr;
  }
  odb::dbBox::create(region, xMin, yMin, xMax, yMax);
  region->addGroup(group);
  _logger->report("Region {} created successfully", region_name);
  return region;
}

void ChipletRegionCreater::printRegionInfo(odb::dbRegion* region,
                                           std::string file_name)
{
  std::ofstream ofs(file_name);
  if (!ofs.is_open()) {
    _logger->report("Failed to open file: {}", file_name);
    return;
  }
  ofs << "Region information:\n";
  ofs << "Region name: " << region->getName() << "\n";
  ofs << "Bounding boxes: " << "\n";
  for (auto box : region->getBoundaries()) {
    ofs << "  - (" << box->xMin() << ", " << box->yMin() << ") to ("
        << box->xMax() << ", " << box->yMax() << ")\n";
  }
  ofs << "Instances:\n";
  for (auto group : region->getGroups()) {
    for (auto inst : group->getInsts()) {
      ofs << " - Instance name: " << inst->getName() << "\n";
      ofs << " - Master name: " << inst->getMaster()->getName() << "\n";
      ofs << " - Instance location: " << inst->getLocation().getX() << " x "
          << inst->getLocation().getY() << "\n";
      if (inst->isBlock()) {
        ofs << "  IsBlock\n";
      }
    }
  }
  ofs.close();
}

}  // namespace par