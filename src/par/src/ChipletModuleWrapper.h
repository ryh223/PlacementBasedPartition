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

#include "odb/db.h"  // Include the necessary OpenDB headers
#include "utl/Logger.h"
#include "db_sta/dbNetwork.hh"

#define DEBUG

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
  odb::dbInst* wrapped_inst_;
  odb::dbBlock* child_block_;
  std::set<odb::dbInst*> insts_;
  std::string block_name_;
  int64_t width_;
  int64_t height_;
  int64_t area_;

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
    width_ = height_ = area_ = 0;
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
    DEBUG_PRINT("Added instance: " << inst->getName());
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
  /**
   * @brief Create a child block and move instances to it.
   *
   * A wrapper instance is then created based on the child block and the child
   * block is connected to the parent block.
   * @param top_block The top block that contains the child block.
   * @return `true` if the child block and wrapper instance are successfully
   * created, `false` otherwise.
   */
  bool createBlock(odb::dbBlock* top_block, utl::Logger* logger)
  {
    // travel the insts to get the area of the block
    DEBUG_PRINT("Calculating area of the block...");
    for (auto& inst : insts_) {
      odb::dbMaster* master = inst->getMaster();
      DEBUG_PRINT("Instance: " << inst->getName() << " Master: " << master->getName());
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
      old_new_insts_map[inst] = odb::dbInst::create(child_block_,
                                                    inst->getMaster(),
                                                    inst->getName().c_str(),
                                                    true);
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
          DEBUG_PRINT("Net connected to instance within the group: "
                      << net->getName());
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
            odb::dbBTerm* bterm = odb::dbBTerm::create(inside_net,
                                              outside_net->getName().c_str());
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
    // create the wrapper cell
    // Library library;
    // odb::dbLib* library = top_block->getDataBase()->findLib(block_name_.c_str());
    // std::shared_ptr<sta::dbNetwork> sta_db_network = std::make_shared<sta::dbNetwork>();
    // sta_db_network->init(top_block->getDataBase(), logger);
    // sta_db_network->setBlock(top_block);
    // sta_db_network->makeLibrary(library);
    // connect cross nets to the wrapper inst
    DEBUG_PRINT("Connecting cross nets to the wrapper instance...");
    for (auto it = outside_net_bterm_map.begin(); it != outside_net_bterm_map.end(); ++it) {
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
    odb::dbMaster* wrapped_inst_master = wrapped_inst_->getMaster();
    wrapped_inst_master->setWidth(width_);
    wrapped_inst_master->setHeight(height_);
    DEBUG_PRINT("Warpper instance INFO: " << wrapped_inst_master->getArea() << " "
                                          << wrapped_inst_master->getWidth() << " "
                                          << wrapped_inst_master->getHeight());
    DEBUG_PRINT("Wrapper instance bounding box created successfully\n");
    return true;
  }
};

class ChipletModuleWrapper
{
 public:
  ChipletModuleWrapper(odb::dbDatabase* db,
                       odb::dbBlock* block,
                       utl::Logger* logger,
                       std::vector<std::vector<std::string>>& combination,
                       std::vector<std::vector<std::string>>& abort);
  ~ChipletModuleWrapper();
  void printDesignInfo(std::string file_name);
  void printModuleInfo(std::string file_name);
  // Initialize the module groups with combination and abort
  bool initModuleGroups(std::vector<std::vector<std::string>>& combination,
                        std::vector<std::vector<std::string>>& abort);
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
  std::set<std::shared_ptr<ModuleConstraintGroup>> _module_groups;
};

class ChipletRegionCreater
{
 public:
  ChipletRegionCreater(odb::dbDatabase* db,
                       odb::dbBlock* block,
                       utl::Logger* logger);
  ~ChipletRegionCreater();
  odb::dbGroup* createGroup(std::string group_name, std::set<odb::dbInst*>& insts);
  odb::dbRegion* createRegion(std::string region_name, odb::dbGroup* group, int64_t xMin, int64_t yMin, int64_t xMax, int64_t yMax);
  void printRegionInfo(odb::dbRegion* region, std::string file_name);
 private:
  // a group that contains the dbInsts and the macro they created
  odb::dbDatabase* _db;
  odb::dbBlock* _block;
  utl::Logger* _logger;
};

}  // namespace par