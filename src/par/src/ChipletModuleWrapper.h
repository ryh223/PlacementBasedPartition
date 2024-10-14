/**
 * This file is part of the chiplet partitioner project,
 * aiming to wrap the inst of the same module into a block.
 */
#pragma once
#include <string>
#include <vector>

#include "odb/db.h"  // Include the necessary OpenDB headers
#include "utl/Logger.h"

namespace odb {
class dbBlock;
class dbInst;
class dbChip;
class dbDatabase;
}  // namespace odb

namespace par {

struct ModuleConstraintGroup
{
  std::vector<odb::dbInst*> insts;
  odb::dbInst* wrapper_inst;
};

class ChipletModuleWrapper
{
 public:
  ChipletModuleWrapper(odb::dbDatabase* db,
                       odb::dbBlock* block,
                       utl::Logger* logger,
                       std::vector<std::string>& combination,
                       std::vector<std::string>& abort);
  ~ChipletModuleWrapper();
  void printDesignInfo();
  // Initialize the module groups with combination and abort
  void initModuleGroups(std::vector<std::string>& combination,
                        std::vector<std::string>& abort);
  // When the module is wrapped, the insts of the same module will be wrapped
  // into a block, and the wrapper_inst will be created. The insts will be
  // removed from the block.
  void wrapModule(std::shared_ptr<ModuleConstraintGroup> module_group);
  // When the module is unwrapped, the insts will be added back to the block,
  // and the wrapper_inst will be removed.
  void unwrapModule(std::shared_ptr<ModuleConstraintGroup> module_group);
  void run();

 private:
  // a group that contains the dbInsts and the macro they created
  odb::dbDatabase* _db;
  odb::dbBlock* _block;
  utl::Logger* _logger;
  std::vector<std::shared_ptr<ModuleConstraintGroup>> _module_groups;
};

}  // namespace par