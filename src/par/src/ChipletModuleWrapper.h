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

class Chiplet;

class ModuleConstraintGroup
{
  /**
   * @class ModuleConstraintGroup
   * @brief A class that manages a group of instances within a module and
   * provides functionality to create a child block and move instances to it.
   *
   * This class encapsulates a set of instances (`insts_`) that belong to the
   * same module and provides methods to manage these instances. It also allows
   * creating a child block, moving instances to it, and reconstructing the
   * connections within the child block.
   *
   * @details
   * - `wrapped_inst_`: The instance that wraps the child block.
   * - `child_block_`: A block that contains the instances.
   * - `group_`: A group that contains the instances.
   * - `insts_`: A set containing the instances of the same module.
   * - `block_name_`: The name of the block.
   * - `width_`, `height_`: Dimensions of the block.
   * - `std_cell_area_`, `macro_area_`: Areas of standard cells and macros.
   *
   * @note The destructor deletes the `wrapped_inst_`.
   */
 private:
  Chiplet* chiplet_{nullptr};
  odb::dbInst* wrapped_inst_{nullptr};
  odb::dbBlock* child_block_{nullptr};
  odb::dbGroup* group_{nullptr};
  std::set<odb::dbInst*> insts_;
  std::set<odb::dbModInst*> mod_insts_;
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
   * @brief Get the chiplet.
   */
  Chiplet* getChiplet() { return chiplet_; }
  /**
   * @brief Set the chiplet.
   * @param chiplet The chiplet to set.
   */
  void setChiplet(Chiplet* chiplet) { chiplet_ = chiplet; }
  /**
   * @brief Add an instance to the set.
   * @param inst The instance to add.
   */
  void addInst(odb::dbInst* inst)
  {
    if (inst->getMaster()->isBlock()) {
      macro_area_ += inst->getMaster()->getArea();
    } else {
      std_cell_area_ += inst->getMaster()->getArea();
    }
    insts_.insert(inst);
    if(group_ == nullptr){
      group_ = odb::dbGroup::create(inst->getBlock(), block_name_.c_str());
    }
    group_->addInst(inst);
  }
  /**
   * @brief Remove an instance from the set.
   * @param inst The instance to remove.
   */
  void removeInst(odb::dbInst* inst)
  {
    if (inst->getMaster()->isBlock()) {
      macro_area_ -= inst->getMaster()->getArea();
    } else {
      std_cell_area_ -= inst->getMaster()->getArea();
    }
    insts_.erase(inst);
    group_->removeInst(inst);
    DEBUG_PRINT("Removed instance: " << inst->getName());
  }
  /**
   * @brief Get the group of instances.
   * @return A pointer to the group.
   */
  odb::dbGroup* getGroup() { return group_; }
  /**
   * @brief Clear all instances from the set and destroy the group.
   */
  void clearInsts()
  {
    macro_area_ = 0;
    std_cell_area_ = 0;
    if(group_ != nullptr){
      odb::dbGroup::destroy(group_);
      group_ = nullptr;
    }
    insts_.clear();
  }
  /**
   * @brief Get the wrapped instance.
   * @return A pointer to the wrapped instance.
   */
  odb::dbInst* getWrappedInst() { return wrapped_inst_; }
  /**
   * @brief Get the width of the block.
   * @return The width of the block.
   */
  int64_t getWidth() { return width_; }
  /**
   * @brief Get the height of the block.
   * @return The height of the block.
   */
  int64_t getHeight() { return height_; }
  /**
   * @brief Get the total area of the block.
   * @return The total area of the block.
   */
  int64_t getArea() { 
    return std_cell_area_ + macro_area_; 
  }
  /**
   * @brief Get the area of standard cells in the block.
   * @return The area of standard cells.
   */
  int64_t getStdCellArea() { return std_cell_area_; }
  /**
   * @brief Get the area of macros in the block.
   * @return The area of macros.
   */
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
  bool checkModInst(odb::dbModInst* mod_inst){
    return _grouped_mod_insts.find(mod_inst) != _grouped_mod_insts.end();
  }
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
  std::set<odb::dbModInst*> _grouped_mod_insts;
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
                              std::set<odb::dbGroup*>& group,
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