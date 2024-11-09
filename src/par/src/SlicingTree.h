#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <cmath>

namespace par{

typedef std::pair<double, double> utilization;
class Chiplet;

class Width_Heights{
    public:
        double width;
        double height;
        size_t left_index;
        size_t right_index;
        Width_Heights() = default;
        Width_Heights(double w, double h, size_t l, size_t r): width(w), height(h), left_index(l), right_index(r) {}
        double getArea() const { return width * height; }
};

class ChipletBlock{
    public:
        std::string name;

        ChipletBlock* parent = NULL;
        ChipletBlock* leftchild = NULL;
        ChipletBlock* rightchild = NULL;

        utilization utilaization_constaint;

        double min_orientation_ratio;
        double max_orientation_ratio;

        std::vector<Width_Heights> shapes;
        size_t shapes_index;

        double x_coordination;
        double y_coordination;

        ChipletBlock() {}

        ChipletBlock(std::string n): name(n) {}
    
        ChipletBlock(std::string n, double area, double min_ratio, double max_ratio, size_t shapes_num, utilization _utilization): name(n), min_orientation_ratio(min_ratio), max_orientation_ratio(max_ratio), shapes_index(0), utilaization_constaint(_utilization) {
            if(shapes_num < 3) {
                std::cerr << "Error: shapes_num should be greater than 2" << std::endl;
                exit(1);
            }
            if(shapes_num % 2 == 0) {
                std::cerr << "Error: shapes_num should be odd" << std::endl;
                exit(1);
            }
            //create a vector<Width_Heights> from min_ratio to max_ratio with shapes_num elements
            double ratio = min_ratio;
            double step = (1 - min_ratio) / ((shapes_num - 1)/2);
            for(size_t i = 0; i < (shapes_num + 1) / 2; i++) {
                shapes.push_back(Width_Heights(sqrt(area / ratio), sqrt(area * ratio), -1, -1));
                ratio += step;
            }
            for(size_t i = 1; i < (shapes_num + 1) / 2; i++) {
                ratio = 1/(1 - step * i);
                shapes.push_back(Width_Heights(sqrt(area / ratio), sqrt(area * ratio), -1, -1));
            }
        }
};


class SlicingTree{
    public:
        std::vector<ChipletBlock*> blocks;
        std::pair<double, double> width_height_constaints;

        SlicingTree() = default;

        SlicingTree(double w, double h, std::vector<ChipletBlock>);

        ~SlicingTree();

        void makeMove();

        void refresh();

        std::vector<Chiplet> genetateSolution(size_t wh_index);

    private:
        bool isValid();

        void _init();

        void check_block(ChipletBlock* block);

        void scoreUp(ChipletBlock* block);

        void updatePointers();

        void updateScore(ChipletBlock* block);

        void horizontalNodeSizing(ChipletBlock* leftchild, ChipletBlock* rightchild, ChipletBlock* block);

        void verticalNodeSizing(ChipletBlock* leftchild, ChipletBlock* rightchild, ChipletBlock* block);

        Chiplet create_chiplet_max_width(ChipletBlock* block, size_t index, double max_width);

        Chiplet create_chiplet_max_height(ChipletBlock* block, size_t index, double max_height);
};
}

