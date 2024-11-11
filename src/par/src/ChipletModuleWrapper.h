/**
 * This file is part of the chiplet partitioner project,
 * aiming to wrap the inst of the same module into a block.
 */
#pragma once
#include <cmath>  // Include cmath for sqrt function
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "db_sta/dbNetwork.hh"
#include "odb/db.h"  // Include the necessary OpenDB headers
#include "utl/Logger.h"

// #define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x) std::cout << x << std::endl
#else
#define DEBUG_PRINT(x)
#endif

namespace odb {
class dbBlock;
class dbInst;
class dbChip;
class dbDatabase;
}  // namespace odb

namespace par {

class ModuleConstraintGroup
{
  /**
   * @class ModuleConstraintGroup
   * @brief A class that manages a group of instances within a module and
   * provides functionality to create a child block and move instances to it.
   *
   * This class encapsulates a set of instances (`insts`) that belong to the
   * same module and provides methods to manage these instances. It also allows
   * creating a child block, moving instances to it, and reconstructing the
   * connections within the child block.
   *
   * @details
   * - `insts_`: A set containing the instances of the same module.
   * - `child_block_`: A block that contains the instances.
   * - `block_name_`: The name of the block.
   * - `wrapped_inst_`: The instance that wraps the child block.
   * - `width_`, `height_`, `area_`: Dimensions and area of the block.
   *
   * @note The destructor deletes the `wrapped_inst_`.
   */
 private:
  odb::dbInst* wrapped_inst_{nullptr};
  odb::dbBlock* child_block_{nullptr};
  std::set<odb::dbInst*> insts_;
  std::string block_name_;
  int64_t width_{0};
  int64_t height_{0};
  int64_t std_cell_area_{0};
  int64_t macro_area_{0};
 public:
  /**
   * @brief Constructor to initialize the ModuleConstraintGroup with a block
   * name.
   * @param top_block The top block that contains the child block.
   * @param block_name The name of the wrapped instance and child block.
   */
  ModuleConstraintGroup(odb::dbBlock* top_block, std::string block_name)
  {
    block_name_ = block_name;
    child_block_ = odb::dbBlock::create(top_block, block_name.c_str());
    width_ = height_ = std_cell_area_ = macro_area_ = 0;
  }
  /**
   * @brief Destructor to clean up resources.
   */
  ~ModuleConstraintGroup() {}
  /**
   * @brief Get the name of the block.
   * @return The name of the block.
   */
  std::string getName() { return block_name_; }
  /**
   * @brief Get the child block.
   * @return A pointer to the child block.
   */
  odb::dbBlock* getBlock() { return child_block_; }
  /**
   * @brief Get the set of instances.
   * @return A reference to the set of instances.
   */
  std::set<odb::dbInst*>& getInsts() { return insts_; }
  /**
   * @brief Add an instance to the set.
   * @param inst The instance to add.
   */
  void addInst(odb::dbInst* inst)
  {
    insts_.insert(inst);
    
  }
  /**
   * @brief Remove an instance from the set.
   * @param inst The instance to remove.
   */
  void removeInst(odb::dbInst* inst)
  {
    insts_.erase(inst);
    DEBUG_PRINT("Removed instance: " << inst->getName());
  }
  odb::dbInst* getWrappedInst() { return wrapped_inst_; }
  int64_t getWidth() { return width_; }
  int64_t getHeight() { return height_; }
  int64_t getArea() { double util = 0.9; return int64_t(std_cell_area_ / util) + macro_area_; }
  int64_t getStdCellArea() { return std_cell_area_; }
  int64_t getMacroArea() { return macro_area_; }

  /**
   * @brief Create a child block and move instances to it.
   *
   * A wrapper instance is then created based on the child block and the child
   * block is connected to the parent block.
   * @param top_block The top block that contains the child block.
   * @return `true` if the child block and wrapper instance are successfully
   * created, `false` otherwise.
   */
  bool createBlock(odb::dbBlock* top_block);
  /**
   * @brief Destroy the child block and wrapper instance.
   * @return `true` if the child block and wrapper instance are successfully
   * destroyed, `false` otherwise.
   * @note The child block and wrapper instance are destroyed.
   */
  bool collapseBlock(odb::dbInst* block_inst);
};

class ChipletModuleWrapper
{
 private:
  ChipletModuleWrapper() = default;

 public:
  static ChipletModuleWrapper& getInstance()
  {
    static ChipletModuleWrapper instance;
    return instance;
  }
  ~ChipletModuleWrapper();
  void printDesignInfo(std::string file_name);
  void printModuleInfo(std::string file_name);
  std::shared_ptr<ModuleConstraintGroup> findWrapperInst(odb::dbInst* inst){
    for(std::shared_ptr<ModuleConstraintGroup> module_group : _module_groups){
      if(module_group->getWrappedInst() == inst){
        return module_group;
      }
    }
    return std::shared_ptr<ModuleConstraintGroup>();
  };
  // Initialize the module groups with combination and abort
  bool initModuleGroups(std::vector<std::vector<std::string>>& combination,
                        std::vector<std::vector<std::string>>& abort);
  bool isIgnoreInst(odb::dbInst* inst);
  bool initModuleGroups(std::set<std::string>& block_names);
  // When the module is wrapped, the insts of the same module will be wrapped
  // into a block, and the wrapper_inst will be created. The insts will be
  // removed from the block.
  void wrapModule(std::shared_ptr<ModuleConstraintGroup> module_group);
  // When the module is unwrapped, the insts will be added back to the block,
  // and the wrapper_inst will be removed.
  void unwrapModule(std::shared_ptr<ModuleConstraintGroup> module_group);
  void runWrap(std::vector<std::vector<std::string>>& combination,
               std::vector<std::vector<std::string>>& abort);
  void runUnwrap();
  std::set<std::shared_ptr<ModuleConstraintGroup>>& getModuleGroups()
  {
    return _module_groups;
  }
  void Test();
  void setOpenROAD(odb::dbDatabase* db,
                   odb::dbBlock* block,
                   utl::Logger* logger)
  {
    _db = db;
    _block = block;
    _logger = logger;
  }

 private:
  // a group that contains the dbInsts and the macro they created
  odb::dbDatabase* _db;
  odb::dbBlock* _block;
  utl::Logger* _logger;
  std::set<std::shared_ptr<ModuleConstraintGroup>> _module_groups;
};

class ChipletRegionCreater
{
 public:
  ChipletRegionCreater(odb::dbDatabase* db,
                       odb::dbBlock* block,
                       utl::Logger* logger);
  ~ChipletRegionCreater();
  odb::dbGroup* createGroup(std::string group_name,
                            std::set<odb::dbInst*>& insts);
  odb::dbRegion* createRegion(std::string region_name,
                              odb::dbGroup* group,
                              int64_t xMin,
                              int64_t yMin,
                              int64_t xMax,
                              int64_t yMax);
  odb::dbRegion* createRegion(std::string region_name,
                              std::set<odb::dbInst*>& insts,
                              int64_t xMin,
                              int64_t yMin,
                              int64_t xMax,
                              int64_t yMax);
  void printRegionInfo(odb::dbRegion* region, std::string file_name);

 private:
  // a group that contains the dbInsts and the macro they created
  odb::dbDatabase* _db;
  odb::dbBlock* _block;
  utl::Logger* _logger;
};

}  // namespace par