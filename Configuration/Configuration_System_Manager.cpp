#include "Configuration_System_Manager.hpp"
#include <fstream>
#include <iostream>

bool ConfigurationSystemManager::Initialize() {
    try {
        if (!fs::exists(config_dir_)) {
            fs::create_directory(config_dir_);
            std::cout << "[ConfigManager] Created directory: " << config_dir_ << std::endl;
            CreateDefaultConfigs();
        } else {
            std::cout << "[ConfigManager] Directory already exists: " << config_dir_ << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ConfigManager] Failed to initialize: " << e.what() << std::endl;
        return false;
    }
}

void ConfigurationSystemManager::CreateDefaultConfigs() {
    try {
        std::ofstream def(config_dir_ + "/default.json");
        def << "{\n  \"components\": {}\n}";
        def.close();

        std::ofstream example(config_dir_ + "/example.json");
        example << "{\n  \"components\": {\n    \"Window1\": {\n      \"is_open\": true,\n      \"position\": [100,100],\n      \"size\": [300,200],\n      \"slider_float\": 0.5,\n      \"slider_int\": 42,\n      \"checkbox\": true,\n      \"color\": [1.0, 0.0, 0.0, 1.0]\n    }\n  }\n}";
        example.close();

        std::cout << "[ConfigManager] Created default configs." << std::endl;
    } catch (...) {
        std::cerr << "[ConfigManager] Failed to create default configs." << std::endl;
    }
}
