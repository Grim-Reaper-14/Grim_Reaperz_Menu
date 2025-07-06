#ifndef FILE_SYSTEM_HPP
#define FILE_SYSTEM_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <functional>

namespace fs = std::filesystem;

class File_System {
private:
    struct FileInfo {
        std::string path;
        std::string created_time;
        std::string modified_time;
        size_t size;
        std::string content_type;
        
        std::string to_json() const {
            std::stringstream ss;
            ss << "{\n";
            ss << "  \"path\": \"" << path << "\",\n";
            ss << "  \"created_time\": \"" << created_time << "\",\n";
            ss << "  \"modified_time\": \"" << modified_time << "\",\n";
            ss << "  \"size\": " << size << ",\n";
            ss << "  \"content_type\": \"" << content_type << "\"\n";
            ss << "}";
            return ss.str();
        }
    };
    
    struct MonitoringData {
        int files_created;
        int files_modified;
        int files_deleted;
        int files_read;
        std::string last_operation;
        std::string last_operation_time;
        
        std::string to_json() const {
            std::stringstream ss;
            ss << "{\n";
            ss << "  \"files_created\": " << files_created << ",\n";
            ss << "  \"files_modified\": " << files_modified << ",\n";
            ss << "  \"files_deleted\": " << files_deleted << ",\n";
            ss << "  \"files_read\": " << files_read << ",\n";
            ss << "  \"last_operation\": \"" << last_operation << "\",\n";
            ss << "  \"last_operation_time\": \"" << last_operation_time << "\"\n";
            ss << "}";
            return ss.str();
        }
    };
    
    std::map<std::string, FileInfo> file_registry;
    MonitoringData monitoring_stats;
    std::mutex fs_mutex;
    bool monitoring_enabled;
    std::string monitoring_log_path;
    
    std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    void log_operation(const std::string& operation, const std::string& file_path = "") {
        std::lock_guard<std::mutex> lock(fs_mutex);
        monitoring_stats.last_operation = operation;
        monitoring_stats.last_operation_time = get_current_timestamp();
        
        if (monitoring_enabled) {
            std::ofstream log_file(monitoring_log_path, std::ios::app);
            if (log_file.is_open()) {
                log_file << "{\n";
                log_file << "  \"timestamp\": \"" << monitoring_stats.last_operation_time << "\",\n";
                log_file << "  \"operation\": \"" << operation << "\",\n";
                log_file << "  \"file_path\": \"" << file_path << "\",\n";
                log_file << "  \"thread_id\": \"" << std::this_thread::get_id() << "\"\n";
                log_file << "},\n";
                log_file.close();
            }
        }
    }
    
    void update_file_info(const std::string& file_path, const std::string& content_type = "json") {
        FileInfo info;
        info.path = file_path;
        info.created_time = get_current_timestamp();
        info.modified_time = get_current_timestamp();
        info.content_type = content_type;
        
        if (fs::exists(file_path)) {
            info.size = fs::file_size(file_path);
            auto ftime = fs::last_write_time(file_path);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            auto time_t = std::chrono::system_clock::to_time_t(sctp);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            info.modified_time = ss.str();
        } else {
            info.size = 0;
        }
        
        file_registry[file_path] = info;
    }

public:
    File_System(bool enable_monitoring = true, const std::string& log_path = "file_system_monitor.json") 
        : monitoring_enabled(enable_monitoring), monitoring_log_path(log_path) {
        monitoring_stats = {0, 0, 0, 0, "initialized", get_current_timestamp()};
        
        if (monitoring_enabled) {
            // Initialize monitoring log file
            std::ofstream log_file(monitoring_log_path);
            if (log_file.is_open()) {
                log_file << "[\n";
                log_file.close();
            }
        }
        
        log_operation("File_System initialized");
    }
    
    ~File_System() {
        if (monitoring_enabled) {
            // Close monitoring log file properly
            std::ofstream log_file(monitoring_log_path, std::ios::app);
            if (log_file.is_open()) {
                log_file << "{\n";
                log_file << "  \"timestamp\": \"" << get_current_timestamp() << "\",\n";
                log_file << "  \"operation\": \"File_System destroyed\",\n";
                log_file << "  \"file_path\": \"\",\n";
                log_file << "  \"thread_id\": \"" << std::this_thread::get_id() << "\"\n";
                log_file << "}\n]\n";
                log_file.close();
            }
        }
    }
    
    // Create JSON file with data
    bool create_json_file(const std::string& file_path, const std::map<std::string, std::string>& data) {
        try {
            std::ofstream file(file_path);
            if (!file.is_open()) {
                log_operation("Failed to create file", file_path);
                return false;
            }
            
            file << "{\n";
            auto it = data.begin();
            while (it != data.end()) {
                file << "  \"" << it->first << "\": \"" << it->second << "\"";
                if (++it != data.end()) {
                    file << ",";
                }
                file << "\n";
            }
            file << "}\n";
            file.close();
            
            update_file_info(file_path, "json");
            monitoring_stats.files_created++;
            log_operation("Created JSON file", file_path);
            return true;
        } catch (const std::exception& e) {
            log_operation("Error creating JSON file: " + std::string(e.what()), file_path);
            return false;
        }
    }
    
    // Create JSON file with custom JSON content
    bool create_json_file_raw(const std::string& file_path, const std::string& json_content) {
        try {
            std::ofstream file(file_path);
            if (!file.is_open()) {
                log_operation("Failed to create raw JSON file", file_path);
                return false;
            }
            
            file << json_content;
            file.close();
            
            update_file_info(file_path, "json");
            monitoring_stats.files_created++;
            log_operation("Created raw JSON file", file_path);
            return true;
        } catch (const std::exception& e) {
            log_operation("Error creating raw JSON file: " + std::string(e.what()), file_path);
            return false;
        }
    }
    
    // Read JSON file content
    std::string read_json_file(const std::string& file_path) {
        try {
            std::ifstream file(file_path);
            if (!file.is_open()) {
                log_operation("Failed to read file", file_path);
                return "";
            }
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            
            monitoring_stats.files_read++;
            log_operation("Read JSON file", file_path);
            return buffer.str();
        } catch (const std::exception& e) {
            log_operation("Error reading JSON file: " + std::string(e.what()), file_path);
            return "";
        }
    }
    
    // Update existing JSON file
    bool update_json_file(const std::string& file_path, const std::map<std::string, std::string>& new_data) {
        try {
            if (!fs::exists(file_path)) {
                log_operation("File does not exist for update", file_path);
                return false;
            }
            
            std::ofstream file(file_path);
            if (!file.is_open()) {
                log_operation("Failed to open file for update", file_path);
                return false;
            }
            
            file << "{\n";
            auto it = new_data.begin();
            while (it != new_data.end()) {
                file << "  \"" << it->first << "\": \"" << it->second << "\"";
                if (++it != new_data.end()) {
                    file << ",";
                }
                file << "\n";
            }
            file << "}\n";
            file.close();
            
            update_file_info(file_path, "json");
            monitoring_stats.files_modified++;
            log_operation("Updated JSON file", file_path);
            return true;
        } catch (const std::exception& e) {
            log_operation("Error updating JSON file: " + std::string(e.what()), file_path);
            return false;
        }
    }
    
    // Delete file
    bool delete_file(const std::string& file_path) {
        try {
            if (!fs::exists(file_path)) {
                log_operation("File does not exist for deletion", file_path);
                return false;
            }
            
            fs::remove(file_path);
            file_registry.erase(file_path);
            monitoring_stats.files_deleted++;
            log_operation("Deleted file", file_path);
            return true;
        } catch (const std::exception& e) {
            log_operation("Error deleting file: " + std::string(e.what()), file_path);
            return false;
        }
    }
    
    // Get file information as JSON
    std::string get_file_info_json(const std::string& file_path) {
        auto it = file_registry.find(file_path);
        if (it != file_registry.end()) {
            return it->second.to_json();
        }
        return "{}";
    }
    
    // Get monitoring statistics as JSON
    std::string get_monitoring_stats_json() {
        std::lock_guard<std::mutex> lock(fs_mutex);
        return monitoring_stats.to_json();
    }
    
    // Get all registered files as JSON
    std::string get_all_files_json() {
        std::stringstream ss;
        ss << "{\n  \"files\": [\n";
        
        auto it = file_registry.begin();
        while (it != file_registry.end()) {
            ss << "    " << it->second.to_json();
            if (++it != file_registry.end()) {
                ss << ",";
            }
            ss << "\n";
        }
        
        ss << "  ],\n";
        ss << "  \"total_files\": " << file_registry.size() << "\n";
        ss << "}\n";
        return ss.str();
    }
    
    // Export monitoring data to file
    bool export_monitoring_data(const std::string& export_path) {
        try {
            std::ofstream file(export_path);
            if (!file.is_open()) {
                return false;
            }
            
            file << "{\n";
            file << "  \"monitoring_statistics\": " << get_monitoring_stats_json() << ",\n";
            file << "  \"registered_files\": " << get_all_files_json() << "\n";
            file << "}\n";
            file.close();
            
            log_operation("Exported monitoring data", export_path);
            return true;
        } catch (const std::exception& e) {
            log_operation("Error exporting monitoring data: " + std::string(e.what()), export_path);
            return false;
        }
    }
    
    // Enable/disable monitoring
    void set_monitoring(bool enabled) {
        monitoring_enabled = enabled;
        log_operation(enabled ? "Monitoring enabled" : "Monitoring disabled");
    }
    
    // Check if file exists
    bool file_exists(const std::string& file_path) {
        return fs::exists(file_path);
    }
    
    // Get file size
    size_t get_file_size(const std::string& file_path) {
        if (fs::exists(file_path)) {
            return fs::file_size(file_path);
        }
        return 0;
    }
    
    // List files in directory as JSON
    std::string list_directory_json(const std::string& directory_path) {
        std::stringstream ss;
        ss << "{\n  \"directory\": \"" << directory_path << "\",\n";
        ss << "  \"files\": [\n";
        
        try {
            bool first = true;
            for (const auto& entry : fs::directory_iterator(directory_path)) {
                if (!first) ss << ",\n";
                ss << "    {\n";
                ss << "      \"name\": \"" << entry.path().filename().string() << "\",\n";
                ss << "      \"path\": \"" << entry.path().string() << "\",\n";
                ss << "      \"is_directory\": " << (entry.is_directory() ? "true" : "false") << ",\n";
                ss << "      \"size\": " << (entry.is_regular_file() ? entry.file_size() : 0) << "\n";
                ss << "    }";
                first = false;
            }
        } catch (const std::exception& e) {
            log_operation("Error listing directory: " + std::string(e.what()), directory_path);
        }
        
        ss << "\n  ]\n}\n";
        log_operation("Listed directory", directory_path);
        return ss.str();
    }
};

#endif // FILE_SYSTEM_HPP