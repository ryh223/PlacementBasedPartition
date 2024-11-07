#include "SlicingTree.h"
#include "ChipletPartitioner.h"
#include <stack>
#include <cassert>
#include <algorithm>

namespace par{
SlicingTree::SlicingTree(float w, float h, std::vector<ChipletBlock> blocks): width_height_constaints(w, h) {
    for(auto& block: blocks) {
        ChipletBlock* new_block = new ChipletBlock();
        *new_block = block;
        this->blocks.push_back(new_block);
    }
}

SlicingTree::~SlicingTree() {
    for(auto& block: blocks) {
        delete block;
    }
}

void SlicingTree::makeMove() {
    bool move_valid = false;

    while(!move_valid){
        int move_type = rand() & 1;
        if(move_type){
            size_t block_id = rand() % blocks.size();
            std::string block_name = blocks[block_id]->name;
            if(block_name == "H" || block_name == "V"){
                blocks[block_id]->name = (block_name == "H") ? "V" : "H";
                if(isValid()){
                    scoreUp(blocks[block_id]);
                    move_valid = true;
                } else {
                    blocks[block_id]->name = block_name;
                }
            }
        } else {
            size_t block_a_id = rand() % blocks.size();
            size_t block_b_id = rand() % blocks.size();
            while(block_a_id == block_b_id){
                block_b_id = rand() % blocks.size();
            }
            std::swap(blocks[block_a_id], blocks[block_b_id]);
            if(isValid()){
                updatePointers();
                scoreUp(blocks[block_a_id]);
                scoreUp(blocks[block_b_id]);
                move_valid = true;
            } else {
                std::swap(blocks[block_a_id], blocks[block_b_id]);
            }
        }
    }
}

bool SlicingTree::isValid(){
    int count = 0;
    std::string prev_name;
    std::string new_name;

    for (ChipletBlock *b : blocks){
        new_name = b->name;
        if (new_name == "H" or new_name == "V"){
            if (count < 2){
                return false;
            }

            // Don't allow two of same slice in a row
            if (new_name == prev_name){
                return false;
            }
            count -= 1;
        } else{
            count += 1;
        }
        prev_name = new_name;
    }

    return (count == 1);
}

void SlicingTree::scoreUp(ChipletBlock* block){
    updateScore(block);
    if (block->parent){
        scoreUp(block->parent);
    } 
    // else {
    //     float min_area = std::numeric_limits<float>::max();
    //     for (Width_Heights &wh : block->shapes){
    //         if(wh.width <= width_height_constaints.first && wh.height <= width_height_constaints.second){
    //             double area = wh.getArea();
    //             if (area < min_area){
    //                 min_area = area;
    //             }
    //         }
    //     }
    //     assert(min_area != std::numeric_limits<double>::max());
    //     return min_area;
    // }
}

void SlicingTree::updatePointers(){
    // Stack for evaluating slicing polish expression
    std::stack<ChipletBlock*> blockStack;

    // Go through all elements in polish slicing tree
    for (ChipletBlock *block_i : blocks){
        // Check if slice
        if (block_i->name == "H" or block_i->name == "V"){
            // Stack must be atleast size 2
            assert(blockStack.size() >= 2);

            // Get children
            ChipletBlock *a = blockStack.top();
            blockStack.pop();
            ChipletBlock *b = blockStack.top();
            blockStack.pop();
            
            // Mark parent/child
            a->parent = block_i;
            b->parent = block_i;
            block_i->leftchild = a;
            block_i->rightchild = b;
        }

        // Add result back to stack
        blockStack.push(block_i);
    }

    assert(blockStack.size() == 1);

    // Fix root node parent
    blockStack.top()->parent = NULL;
}

void SlicingTree::updateScore(ChipletBlock* block){
    if(block->name == "V"){
        assert(block->leftchild != NULL);
        assert(block->rightchild != NULL);
        verticalNodeSizing(block->leftchild, block->rightchild, block);
    }
    if(block->name == "H"){
        assert(block->leftchild != NULL);
        assert(block->rightchild != NULL);
        horizontalNodeSizing(block->leftchild, block->rightchild, block);
    }
}

std::vector<Chiplet> SlicingTree::genetateSolution(size_t wh_index){
    std::vector<Chiplet> solution;
    ChipletBlock *root = blocks[blocks.size() - 1];
    assert(root->name == "H" or root->name == "V");

    if(root->shapes[wh_index].width >= width_height_constaints.first && root->shapes[wh_index].height >= width_height_constaints.second){
        return solution;
    } else if (root->shapes[wh_index].width >= width_height_constaints.first * 1.2 ||
               root->shapes[wh_index].height >= width_height_constaints.second * 1.2){
        return solution;
    } else {
        std::stack<std::pair<ChipletBlock*, size_t>> helpStack;
        helpStack.push(std::make_pair(root, wh_index));
        while(!helpStack.empty()){
            std::pair<ChipletBlock*, size_t> block_info = helpStack.top();
            helpStack.pop();
            if(block_info.first->name == "H"){
                float max_width = block_info.first->shapes[block_info.second].width;
                ChipletBlock* leftchild = block_info.first->leftchild;
                ChipletBlock* rightchild = block_info.first->rightchild;
                size_t leftchild_index = block_info.first->shapes_index;
                size_t rightchild_index = block_info.first->shapes_index;
                leftchild->x_coordination = block_info.first->x_coordination;
                rightchild->x_coordination = block_info.first->x_coordination;
                leftchild->y_coordination = block_info.first->y_coordination;
                rightchild->y_coordination = block_info.first->y_coordination + block_info.first->shapes[block_info.second].height;
                if(leftchild->name != "H" && leftchild->name != "V"){
                    Chiplet _chiplet = create_chiplet_max_width(leftchild, leftchild_index, max_width);
                    solution.push_back(_chiplet);
                } else {
                    helpStack.push(std::make_pair(leftchild, leftchild_index));
                }
                if(rightchild->name != "H" && rightchild->name != "V"){
                    Chiplet _chiplet = create_chiplet_max_width(rightchild, rightchild_index, max_width);
                    solution.push_back(_chiplet);
                } else {
                    helpStack.push(std::make_pair(rightchild, rightchild_index));
                }
            } else if(block_info.first->name == "V"){
                float max_height = block_info.first->shapes[block_info.second].height;
                ChipletBlock* leftchild = block_info.first->leftchild;
                ChipletBlock* rightchild = block_info.first->rightchild;
                size_t leftchild_index = block_info.first->shapes_index;
                size_t rightchild_index = block_info.first->shapes_index;
                leftchild->x_coordination = block_info.first->x_coordination;
                rightchild->x_coordination = block_info.first->x_coordination + block_info.first->shapes[block_info.second].width;
                leftchild->y_coordination = block_info.first->y_coordination;
                rightchild->y_coordination = block_info.first->y_coordination;
                if(leftchild->name != "H" && leftchild->name != "V"){
                    Chiplet _chiplet = create_chiplet_max_height(leftchild, leftchild_index, max_height);
                    solution.push_back(_chiplet);
                } else {
                    helpStack.push(std::make_pair(leftchild, leftchild_index));
                }
                if(rightchild->name != "H" && rightchild->name != "V"){
                    Chiplet _chiplet = create_chiplet_max_height(rightchild, rightchild_index, max_height);
                    solution.push_back(_chiplet);
                } else {
                    helpStack.push(std::make_pair(rightchild, rightchild_index));
                }
            }
        }
    }
    return solution;
}

Chiplet SlicingTree::create_chiplet_max_width(ChipletBlock* block, size_t index, float max_width){
    Chiplet _chiplet;
    _chiplet.name = block->name;
    _chiplet.width = std::min(max_width, block->shapes[index].height * block->max_orientation_ratio);
    _chiplet.height = block->shapes[index].height;
    _chiplet.location = std::make_pair(block->x_coordination, block->y_coordination);
    _chiplet.utilization_constaint = block->utilaization_constaint;
    _chiplet.aspect_ratio = _chiplet.width / _chiplet.height;
    return _chiplet;
}

Chiplet SlicingTree::create_chiplet_max_height(ChipletBlock* block, size_t index, float max_height){
    Chiplet _chiplet;
    _chiplet.name = block->name;
    _chiplet.width = block->shapes[index].width;
    _chiplet.height = std::min(max_height, block->shapes[index].width / block->min_orientation_ratio);
    _chiplet.location = std::make_pair(block->x_coordination, block->y_coordination);
    _chiplet.utilization_constaint = block->utilaization_constaint;
    _chiplet.aspect_ratio = _chiplet.width / _chiplet.height;
    return _chiplet;
}

void SlicingTree::verticalNodeSizing(ChipletBlock* leftchild, ChipletBlock* rightchild, ChipletBlock* block){
    std::sort(
        leftchild->shapes.begin(), 
        leftchild->shapes.end(), 
        [](const Width_Heights &l, const Width_Heights &r){ return l.width < r.width; }
    );
    std::sort(
        rightchild->shapes.begin(), 
        rightchild->shapes.end(), 
        [](const Width_Heights &l, const Width_Heights &r){ return l.width < r.width; }
    );

    // Clear result sizes
    block->shapes.clear();

    int len_left = leftchild->shapes.size();
    int len_right = leftchild->shapes.size();
    int i = 0, j = 0;
    double new_width, new_height;
    Width_Heights left_wh, right_wh;

    // Traverse set of widths and height
    while (i < len_left and j < len_right){
        left_wh = leftchild->shapes[i];
        right_wh = rightchild->shapes[j];

        // Compute vertical slice
        new_width = left_wh.width + right_wh.width;
        new_height = std::max(left_wh.height, right_wh.height);

        // Create width height object storing which indexes 
        Width_Heights new_wh(new_width, new_height, i, j);

        // Add to result block
        block->shapes.push_back(new_wh);

        // Increment correct index
        if (new_height == left_wh.height){
            i++;
        }
        if (new_height == right_wh.height){
            j++;
        }
    }
}

void SlicingTree::horizontalNodeSizing(ChipletBlock* leftchild, ChipletBlock* rightchild, ChipletBlock* block){
    std::sort(
        leftchild->shapes.begin(), 
        leftchild->shapes.end(), 
        [](const Width_Heights &l, const Width_Heights &r){ return l.height < r.height; }
    );
    std::sort(
        rightchild->shapes.begin(), 
        rightchild->shapes.end(), 
        [](const Width_Heights &l, const Width_Heights &r){ return l.height < r.height; }
    );

    // Clear result sizes
    block->shapes.clear();

    int len_left = leftchild->shapes.size();
    int len_right = leftchild->shapes.size();
    int i = 0, j = 0;
    double new_width, new_height;
    Width_Heights left_wh, right_wh;

    // Traverse set of widths and height                
    while (i < len_left and j < len_right){
        left_wh = leftchild->shapes[i];
        right_wh = rightchild->shapes[j];

        // Compute horizontal slice
        new_width = std::max(left_wh.width, right_wh.width);
        new_height = left_wh.height + right_wh.height;

        // Create width height object storing which indexes 
        Width_Heights new_wh(new_width, new_height, i, j);

        // Add to result block
        block->shapes.push_back(new_wh);

        // Increment correct index
        if (new_width == left_wh.width){
            i++;
        }
        if (new_width == right_wh.width){
            j++;
        }
    }
}
}

