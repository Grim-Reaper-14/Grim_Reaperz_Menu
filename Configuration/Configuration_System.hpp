#pragma once
#include <imgui.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <unordered_map>

using json = nlohmann::json;

class ConfigSystem {
public:
    struct ComponentState {
        std::string id;
        bool is_open = true;
        ImVec2 position = {0, 0};
        ImVec2 size = {0, 0};
        float slider_float = 0.0f;
        int slider_int = 0;
        bool checkbox = false;
        ImVec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    };

    ConfigSystem() = default;
    ~ConfigSystem() = default;

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    void RenderUI();

    bool SaveConfig(const std::string& filename);
    bool LoadConfig(const std::string& filename);

    void AddComponent(const std::string& id);
    void ApplyConfig();

private:
    json SerializeComponent(const ComponentState& state);
    ComponentState DeserializeComponent(const json& j);
    std::vector<std::string> GetConfigFiles();

    std::unordered_map<std::string, ComponentState> components_;
    std::string current_config_file_;
    std::vector<std::string> config_files_;
    bool is_initialized_ = false;
};
