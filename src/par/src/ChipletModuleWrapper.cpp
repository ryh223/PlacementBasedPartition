#include "ChipletModuleWrapper.h"

#include <fstream>
#include <iostream>
#include <set>
#include <unordered_set>

namespace par {

ChipletModuleWrapper::ChipletModuleWrapper(
    odb::dbDatabase* db,
    odb::dbBlock* block,
    utl::Logger* logger,
    std::vector<std::vector<std::string>>& combination,
    std::vector<std::vector<std::string>>& abort)
    : _db(db), _block(block), _logger(logger)
{
  // Initialize the module groups with combination and abort
  _logger->report("Initializing module groups, {} combinations and {} aborts",
                  combination.size(),
                  abort.size());
  if (initModuleGroups(combination, abort)) {
    _logger->report("Module groups initialized, {} groups created",
                    _module_groups.size());
    std::string file_name = "module_info.txt";
    printModuleInfo(file_name);
  } else {
    _logger->report("Failed to initialize module groups");
  }
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

  //   // Iterate over nets
  //   for (auto net : _block->getNets()) {
  //     ofs << "Net name: " << net->getName() << "\n";
  //     ofs << "Net connections: " << net->getITerms().size() << " drivers, "
  //         << net->getBTerms().size() << " loads\n";

  //     // Print ITerms information
  //     ofs << "ITerms:\n";
  //     for (auto iterm : net->getITerms()) {
  //       ofs << "  Instance: " << iterm->getInst()->getName()
  //           << ", Pin: " << iterm->getMTerm()->getName() << "\n";
  //     }

  //     // Print BTerms information
  //     ofs << "BTerms:\n";
  //     for (auto bterm : net->getBTerms()) {
  //       ofs << "  BTerm: " << bterm->getName()
  //           << " Block:  " << bterm->getBlock()->getName() << "\n";
  //     }
  //   }

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
  module_group->createBlock(_block, _logger);
}

void ChipletModuleWrapper::unwrapModule(
    std::shared_ptr<ModuleConstraintGroup> module_group)
{
  //   // Add instances back to the block
  //   for (auto& inst : module_group->insts) {
  //     _block->addInst(inst);
  //   }

  //   // Remove the wrapper instance
  //   _block->removeInst(module_group->wrapper_inst);
  //   module_group->wrapper_inst = nullptr;
}

void ChipletModuleWrapper::run()
{
  // Run the wrapping and unwrapping process
  for (auto& module_group : _module_groups) {
    wrapModule(module_group);
  }
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