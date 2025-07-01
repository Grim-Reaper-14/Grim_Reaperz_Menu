// Folder_System.cpp
#include "Folder_System.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

FolderSystem::FolderSystem(const std::string& appName, size_t maxLogSize)
    : appName(appName),
    fileSystem(std::make_unique<FileSystem>(appName, maxLogSize)) {
#ifdef _WIN32
    char* appDataPath = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appDataPath, &len, "APPDATA") == 0 && appDataPath != nullptr) {
        baseAppDataPath = appDataPath;
        free(appDataPath); // Free the allocated memory
    } else {
        baseAppDataPath = "C:/Temp";
    }
#else
    baseAppDataPath = "/tmp"; // Fallback
#endif
    baseAppDataPath += "/Tutones_External_Mod_Menu/" + appName;
}

FolderSystem::~FolderSystem() {
    stopMonitoring();
}

bool FolderSystem::initialize() {
    try {
        // Initialize the underlying file system
        if (!fileSystem->initialize()) {
            log("Failed to initialize file system");
            return false;
        }

        // Create base directory if not already created
        fs::create_directories(baseAppDataPath);
        log("Folder system initialized at: " + baseAppDataPath);
        return true;
    }
    catch (const fs::filesystem_error& e) {
        log("Error initializing folder system: " + std::string(e.what()));
        return false;
    }
}

bool FolderSystem::createFolder(const std::string& folderName) {
    std::lock_guard<std::mutex> lock(folderMutex);
    try {
        std::string folderPath = getFolderPath(folderName);
        if (fs::exists(folderPath)) {
            log("Folder already exists: " + folderName);
            return false;
        }

        fs::create_directory(folderPath);
        managedFolders.push_back(folderName);
        log("Created folder: " + folderName);
        return true;
    }
    catch (const fs::filesystem_error& e) {
        log("Error creating folder " + folderName + ": " + e.what());
        return false;
    }
}

bool FolderSystem::deleteFolder(const std::string& folderName) {
    std::lock_guard<std::mutex> lock(folderMutex);
    try {
        std::string folderPath = getFolderPath(folderName);
        if (!fs::exists(folderPath)) {
            log("Folder does not exist: " + folderName);
            return false;
        }

        fs::remove_all(folderPath);
        managedFolders.erase(
            std::remove(managedFolders.begin(), managedFolders.end(), folderName),
            managedFolders.end()
        );
        log("Deleted folder: " + folderName);
        return true;
    }
    catch (const fs::filesystem_error& e) {
        log("Error deleting folder " + folderName + ": " + e.what());
        return false;
    }
}

bool FolderSystem::createFileInFolder(const std::string& folderName, const std::string& filename, const std::string& content) {
    std::lock_guard<std::mutex> lock(folderMutex);
    try {
        std::string folderPath = getFolderPath(folderName);
        if (!fs::exists(folderPath)) {
            log("Folder does not exist for file creation: " + folderName);
            return false;
        }

        // Temporarily adjust FileSystem's base path for this operation
        std::string originalPath = fileSystem->getAppDataPath() + "/Tutones_External_Mod_Menu/" + appName;
        std::string tempPath = originalPath + "/" + folderName;
        // Note: FileSystem needs a method to set temporary path or we handle path manually
        std::string fullPath = tempPath + "/" + filename;
        std::ofstream file(fullPath);
        if (!file.is_open()) {
            log("Failed to create file " + filename + " in folder " + folderName);
            return false;
        }

        file << content;
        file.close();
        log("Created file " + filename + " in folder " + folderName);
        return true;
    }
    catch (const std::exception& e) {
        log("Error creating file in folder " + folderName + ": " + e.what());
        return false;
    }
}

std::string FolderSystem::readFileInFolder(const std::string& folderName, const std::string& filename) {
    std::lock_guard<std::mutex> lock(folderMutex);
    try {
        std::string folderPath = getFolderPath(folderName);
        if (!fs::exists(folderPath)) {
            log("Folder does not exist for reading: " + folderName);
            return "";
        }

        std::string fullPath = folderPath + "/" + filename;
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            log("Failed to read file " + filename + " from folder " + folderName);
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        log("Read file " + filename + " from folder " + folderName);
        return buffer.str();
    }
    catch (const std::exception& e) {
        log("Error reading file from folder " + folderName + ": " + e.what());
        return "";
    }
}

bool FolderSystem::backupFolder(const std::string& folderName) {
    std::lock_guard<std::mutex> lock(folderMutex);
    try {
        std::string folderPath = getFolderPath(folderName);
        if (!fs::exists(folderPath)) {
            log("Folder does not exist for backup: " + folderName);
            return false;
        }

        auto timestamp = std::chrono::system_clock::now();
        auto timestampStr = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            timestamp.time_since_epoch()).count());
        std::string backupPath = baseAppDataPath + "/backup/" + folderName + "_" + timestampStr;
        fs::create_directories(backupPath);

        for (const auto& entry : fs::directory_iterator(folderPath)) {
            fs::copy(entry.path(), backupPath + "/" + entry.path().filename().string());
        }

        log("Backed up folder " + folderName + " to " + backupPath);
        return true;
    }
    catch (const fs::filesystem_error& e) {
        log("Error backing up folder " + folderName + ": " + e.what());
        return false;
    }
}

void FolderSystem::startMonitoring() {
    fileSystem->startMonitoring();
    log("Started monitoring for all folders");
}

void FolderSystem::stopMonitoring() {
    fileSystem->stopMonitoring();
    log("Stopped monitoring for all folders");
}

std::string FolderSystem::getFolderPath(const std::string& folderName) {
    return baseAppDataPath + "/" + folderName;
}

void FolderSystem::log(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_s(&localTime, &now_c);
    std::stringstream ss;
    ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << " [FolderSystem] - " << message;
    fileSystem->log(ss.str());
}