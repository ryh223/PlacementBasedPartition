#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm> // 使用 std::find

namespace par{
class ModuleManager {
public:
    ModuleManager() {}

    // 处理一行输入
    void processLine(const std::string& line) {
        std::vector<std::string> currentModules = splitLine(line);  // 分割行得到模块
        std::vector<std::string> currentCombine;
        std::vector<std::string> currentAbort;

        // 遍历当前行的每个模块
        for (const auto& module : currentModules) {
            bool isChildOfPrevious = false;

            // 检查当前模块是否为之前某行模块的子模块
            for (size_t i = 0; i < combine.size(); ++i) {
                for (const auto& existingModule : combine[i]) {
                    if (isSubmodule(existingModule, module)) {
                        // 确保模块只添加一次到对应行的 abort 中
                        if (std::find(abort[i].begin(), abort[i].end(), module) == abort[i].end()) {
                            abort[i].push_back(module);  // 将子模块添加到对应行的 abort 中
                            isChildOfPrevious = true; // 标记该模块已经被添加
                        }
                        break; // 找到父模块后不再继续检查
                    }
                }
                if (isChildOfPrevious) break; // 如果已经找到父模块，停止对后续父模块的检查
            }

            currentCombine.push_back(module);
        }

        // 将当前行的 combine 和 abort 保存起来
        combine.push_back(currentCombine);
        abort.push_back(currentAbort);
    }

    // 从文件读取每一行并处理
    void processFile(const std::string& filename) {
        std::ifstream file(filename);
        std::string line;

        if (file.is_open()) {
            while (std::getline(file, line)) {
                processLine(line);  // 处理每一行
            }
            file.close();
        } else {
            std::cerr << "Unable to open " << filename << std::endl;
        }
    }

    // 输出结果
    void printResults() {
        std::cout << "Combine: \n";
        for (size_t i = 0; i < combine.size(); ++i) {
            std::cout << "Combine[" << i << "]: ";
            for (const auto& module : combine[i]) {
                std::cout << module << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "Abort: \n";
        for (size_t i = 0; i < abort.size(); ++i) {
            std::cout << "Abort[" << i << "]: ";
            for (const auto& module : abort[i]) {
                std::cout << module << " ";
            }
            std::cout << std::endl;
        }
    }

    // Getter 函数
    std::vector<std::vector<std::string>>& getCombine() {
        return combine;
    }

    std::vector<std::vector<std::string>>& getAbort() {
        return abort;
    }

private:
    std::vector<std::vector<std::string>> combine;  // 存储每行的 combine 模块
    std::vector<std::vector<std::string>> abort;    // 存储每行的 abort 模块

    // 检查是否为子模块
    bool isSubmodule(const std::string& parent, const std::string& child) {
        return child.find(parent) == 0 && child.size() > parent.size() && child[parent.size()] == '/';
    }

    // 模拟将行分割成模块，实际应用中可以根据实际需求修改
    std::vector<std::string> splitLine(const std::string& line) {
        std::vector<std::string> modules;
        size_t start = 0;
        size_t end = line.find(" ");
        while (end != std::string::npos) {
            modules.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find(" ", start);
        }
        modules.push_back(line.substr(start, end));
        return modules;
    }
};
}