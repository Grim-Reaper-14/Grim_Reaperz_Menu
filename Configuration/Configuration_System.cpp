#include "configuration_system.hpp"
#include <iostream>

namespace fs = std::filesystem;

bool ConfigSystem::Initialize(ID3D11Device*, ID3D11DeviceContext*) {
    // You can use device/context for texture saving later if needed
    is_initialized_ = true;
    config_files_ = GetConfigFiles();
    return true;
}

void ConfigSystem::AddComponent(const std::string& id) {
    if (components_.count(id) == 0) {
        ComponentState state;
        state.id = id;
        components_[id] = state;
    }
}

bool ConfigSystem::SaveConfig(const std::string& filename) {
    json j;
    for (const auto& [id, state] : components_) {
        j["components"][id] = SerializeComponent(state);
    }

    try {
        std::ofstream file(filename);
        file << j.dump(4);
        file.close();
        config_files_ = GetConfigFiles(); // refresh
        current_config_file_ = filename;
        return true;
    } catch (std::exception& e) {
        std::cerr << "Failed to save config: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigSystem::LoadConfig(const std::string& filename) {
    try {
        std::ifstream file(filename);
        json j;
        file >> j;
        file.close();

        for (auto& [id, data] : j["components"].items()) {
            components_[id] = DeserializeComponent(data);
        }
        current_config_file_ = filename;
        return true;
    } catch (std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        return false;
    }
}

void ConfigSystem::ApplyConfig() {
    for (auto& [id, state] : components_) {
        if (state.is_open) {
            ImGui::SetNextWindowPos(state.position, ImGuiCond_Always);
            ImGui::SetNextWindowSize(state.size, ImGuiCond_Always);
        }
    }
}

void ConfigSystem::RenderUI() {
    ImGui::Begin("Config System", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::Button("Refresh configs"))
        config_files_ = GetConfigFiles();

    ImGui::Text("Available configs:");
    for (const auto& file : config_files_) {
        if (ImGui::Selectable(file.c_str(), current_config_file_ == file)) {
            LoadConfig(file);
        }
    }

    if (ImGui::Button("Save current config")) {
        std::string name = "config_" + std::to_string(time(nullptr)) + ".json";
        SaveConfig(name);
    }

    ImGui::End();
}

json ConfigSystem::SerializeComponent(const ComponentState& s) {
    return {
        {"id", s.id},
        {"is_open", s.is_open},
        {"position", {s.position.x, s.position.y}},
        {"size", {s.size.x, s.size.y}},
        {"slider_float", s.slider_float},
        {"slider_int", s.slider_int},
        {"checkbox", s.checkbox},
        {"color", {s.color.x, s.color.y, s.color.z, s.color.w}}
    };
}

ConfigSystem::ComponentState ConfigSystem::DeserializeComponent(const json& j) {
    ComponentState s;
    s.id = j.value("id", "");
    s.is_open = j.value("is_open", true);

    auto pos = j.value("position", std::vector<float>{0, 0});
    if (pos.size() == 2) s.position = ImVec2(pos[0], pos[1]);

    auto size = j.value("size", std::vector<float>{0, 0});
    if (size.size() == 2) s.size = ImVec2(size[0], size[1]);

    s.slider_float = j.value("slider_float", 0.0f);
    s.slider_int = j.value("slider_int", 0);
    s.checkbox = j.value("checkbox", false);

    auto col = j.value("color", std::vector<float>{1,1,1,1});
    if (col.size() == 4) s.color = ImVec4(col[0], col[1], col[2], col[3]);

    return s;
}

std::vector<std::string> ConfigSystem::GetConfigFiles() {
    std::vector<std::string> files;
    try {
        for (auto& p : fs::directory_iterator(".")) {
            if (p.path().extension() == ".json") {
                files.push_back(p.path().filename().string());
            }
        }
    } catch (...) {}
    return files;
}
