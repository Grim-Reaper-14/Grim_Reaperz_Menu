#pragma once

#include "../File/File_System.hpp"
#include <string>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

namespace fs = std::filesystem;

class FolderSystem {
public:
    FolderSystem(const std::string& appName, size_t maxLogSize = 1024 * 1024 /* 1MB */);
    ~FolderSystem();

    // Initialize the folder system and underlying file system
    bool initialize();

    // Create a new folder in the app's AppData directory
    bool createFolder(const std::string& folderName);

    // Delete a folder from the app's AppData directory
    bool deleteFolder(const std::string& folderName);

    // Create a file in a specific folder
    bool createFileInFolder(const std::string& folderName, const std::string& filename, const std::string& content);

    // Read a file from a specific folder
    std::string readFileInFolder(const std::string& folderName, const std::string& filename);

    // Backup all files in a specific folder
    bool backupFolder(const std::string& folderName);

    // Start monitoring all folders
    void startMonitoring();

    // Stop monitoring all folders
    void stopMonitoring();

private:
    std::string appName;
    std::string baseAppDataPath;
    std::unique_ptr<FileSystem> fileSystem;
    std::mutex folderMutex;
    std::vector<std::string> managedFolders;

    // Get full path for a folder
    std::string getFolderPath(const std::string& folderName);

    // Log folder operations
    void log(const std::string& message);

    // Grant LoggerSystem access to private members
    friend class LoggerSystem;
};
