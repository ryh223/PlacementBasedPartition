#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <cmath>

namespace par{

typedef std::pair<float, float> utilization;
class Chiplet;

class Width_Heights{
    public:
        float width;
        float height;
        size_t left_index;
        size_t right_index;
        Width_Heights() = default;
        Width_Heights(float w, float h, size_t l, size_t r): width(w), height(h), left_index(l), right_index(r) {}
        float getArea() const { return width * height; }
};

class ChipletBlock{
    public:
        std::string name;

        ChipletBlock* parent;
        ChipletBlock* leftchild;
        ChipletBlock* rightchild;

        utilization utilaization_constaint;

        float min_orientation_ratio;
        float max_orientation_ratio;

        std::vector<Width_Heights> shapes;
        size_t shapes_index;

        float x_coordination;
        float y_coordination;

        ChipletBlock() {}
    
        ChipletBlock(std::string n, float area, float min_ratio, float max_ratio, size_t shapes_num, utilization _utilization): name(n), min_orientation_ratio(min_ratio), max_orientation_ratio(max_ratio), shapes_index(0), utilaization_constaint(_utilization) {
            if(shapes_num < 3) {
                std::cerr << "Error: shapes_num should be greater than 2" << std::endl;
                exit(1);
            }
            //create a vector<Width_Heights> from min_ratio to max_ratio with shapes_num elements
            float ratio = min_ratio;
            float step = (max_ratio - min_ratio) / (shapes_num - 1);
            for(size_t i = 0; i < shapes_num; i++) {
                shapes.push_back(Width_Heights(sqrt(area / ratio), sqrt(area * ratio), -1, -1));
                ratio += step;
            }
        }
};


class SlicingTree{
    public:
        std::vector<ChipletBlock*> blocks;
        std::pair<float, float> width_height_constaints;

        SlicingTree(float w, float h, std::vector<ChipletBlock>);

        ~SlicingTree();

        void makeMove();

        std::vector<Chiplet> genetateSolution(size_t wh_index);

    private:
        bool isValid();

        void scoreUp(ChipletBlock* block);

        void updatePointers();

        void updateScore(ChipletBlock* block);

        void horizontalNodeSizing(ChipletBlock* leftchild, ChipletBlock* rightchild, ChipletBlock* block);

        void verticalNodeSizing(ChipletBlock* leftchild, ChipletBlock* rightchild, ChipletBlock* block);

        Chiplet create_chiplet_max_width(ChipletBlock* block, size_t index, float max_width);

        Chiplet create_chiplet_max_height(ChipletBlock* block, size_t index, float max_height);
};
}

