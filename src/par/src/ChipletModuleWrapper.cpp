#include "ChipletModuleWrapper.h"

#include <fstream>
#include <iostream>
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
  initModuleGroups(combination, abort);
  _logger->report("Module groups initialized, {} groups created",
                  _module_groups.size());
  std::string file_name = "module_info.txt";
  printModuleInfo(file_name);
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
    ofs << "Module name: " << dbmodule->block_name << "\n";
    ofs << "Number of instances: " << dbmodule->insts.size() << "\n";
    // Iterate over instances
    for (const auto& inst : dbmodule->insts) {
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
  //   // Iterate over instances
  //   for (auto inst : _block->getInsts()) {
  //     ofs << "Instance name: " << inst->getName() << "\n";
  //     ofs << "Master name: " << inst->getMaster()->getName() << "\n";
  //     ofs << "Instance location: " << inst->getLocation().getX() << " x "
  //         << inst->getLocation().getY() << "\n";
  //     if (inst->isBlock()) {
  //       ofs << "IsBlock\n";
  //     }
  //   }

  // Iterate over nets
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
    std::shared_ptr<ModuleConstraintGroup> module_group
        = std::make_shared<ModuleConstraintGroup>();
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
            module_group->insts.push_back(inst);
          } else {
            _logger->report(
                "Instance {} is in the abort list and will be skipped",
                inst->getName());
          }
        }
      } else {
        _logger->report("Module {} not found in combination list",
                        combination_module_name);
      }
    }

    _module_groups.push_back(module_group);
  }

  return true;
}

void ChipletModuleWrapper::wrapModule(
    std::shared_ptr<ModuleConstraintGroup> module_group,
    std::string module_name)
{
  //   // Calculate the total area of the instances in the module group
  //   int total_area = 0;
  //   for (auto& inst : module_group->insts) {
  //     total_area += inst->getMaster()->getArea();
  //   }

  //   // Expand the total area with the utilization value (assuming utilization
  //   is a
  //   // percentage)
  //   float utilization = 0.5;  // Example utilization value
  //   int expanded_area = static_cast<int>(total_area / utilization);

  //   // Create a new block for the module
  //   odb::dbBlock* new_block = odb::dbBlock::create(_block,
  //   module_name.c_str()); if (!new_block) {
  //     std::cerr << "Failed to create new block" << std::endl;
  //     return;
  //   }

  //   // Copy the via table from the original block to the new block
  //   odb::dbBlock::copyViaTable(new_block, _block);

  //   // Create a new master for the module
  //   odb::dbMaster* master = odb::dbMaster::create(new_block,
  //   module_name.c_str()); if (!master) {
  //     std::cerr << "Failed to create master" << std::endl;
  //     return;
  //   }

  //   // Set macro unit properties
  //   master->setType(odb::dbMasterType::BLOCK);
  //   master->setWidth(static_cast<int>(
  //       sqrt(expanded_area)));  // Assuming square shape for simplicity
  //   master->setHeight(static_cast<int>(sqrt(expanded_area)));

  //   // Create a new instance and add it to the block
  //   odb::dbInst* inst = odb::dbInst::create(_block, master,
  //   module_name.c_str()); if (!inst) {
  //     std::cerr << "Failed to create instance" << std::endl;
  //     return;
  //   }

  //   // Set instance location and orientation
  //   inst->setLocation(500, 500);  // Example location
  //   inst->setOrient(odb::dbOrientType::R0);

  //   // Add inner instances to the created macro
  //   for (auto& inner_inst : module_group->insts) {
  //     new_block->addInst(inner_inst);
  //   }

  //   // Remove the instances to be wrapped from the original block
  //   for (auto& inner_inst : module_group->insts) {
  //     _block->removeInst(inner_inst);
  //   }

  //   _block->addInst(inst);
  //   // Update the module group
  //   module_group->wrapper_inst = inst;
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
  //   for (auto& module_group : _module_groups) {
  //     wrapModule(module_group,
  //                "wrapped_" +
  //                module_group->insts[0]->getMaster()->getName());
  //   }
}

}  // namespace par