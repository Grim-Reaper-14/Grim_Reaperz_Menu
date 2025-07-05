#pragma once
#include "Configuration_System.hpp"  // include your existing system
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class ConfigurationSystemManager {
public:
    ConfigurationSystemManager() = default;
    ~ConfigurationSystemManager() = default;

    // Create "Configurations" directory and default files if missing
    bool Initialize();

private:
    void CreateDefaultConfigs();
    const std::string config_dir_ = "Configurations";
};
