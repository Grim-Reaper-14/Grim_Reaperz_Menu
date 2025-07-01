// File_System.cpp
#include "File_System.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

FileSystem::FileSystem(const std::string& appName, size_t maxLogSize)
    : appName(appName),
    maxLogSize(maxLogSize),
    isMonitoring(false) {
    appDir = getAppDataPath() + "/Tutones_External_Mod_Menu/" + appName;
    backupDir = appDir + "/backup";
    logFile = appDir + "/app.log";
}

FileSystem::~FileSystem() {
    stopMonitoring();
}

std::string FileSystem::getAppDataPath() {
#ifdef _WIN32
    char* buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, "APPDATA") == 0 && buffer != nullptr) {
        std::string appDataPath(buffer);
        free(buffer); // Free the allocated memory
        return appDataPath;
    }
    return "C:/Temp";
#elif __linux__
    return std::getenv("HOME") ? std::string(std::getenv("HOME")) + "/.config" : "/tmp";
#else
    return "/tmp"; // Fallback
#endif
}

bool FileSystem::initialize() {
    try {
        // Create main directory
        fs::create_directories(appDir);

        // Create backup directory
        fs::create_directories(backupDir);

        log("File system initialized at: " + appDir);
        return true;
    }
    catch (const fs::filesystem_error& e) {
        log("Error initializing file system: " + std::string(e.what()));
        return false;
    }
}

bool FileSystem::createFile(const std::string& filename, const std::string& content) {
    try {
        std::string fullPath = appDir + "/" + filename;
        std::ofstream file(fullPath);
        if (!file.is_open()) {
            log("Failed to create file: " + filename);
            return false;
        }

        file << content;
        file.close();

        log("Created file: " + filename);
        return true;
    }
    catch (const std::exception& e) {
        log("Error creating file: " + std::string(e.what()));
        return false;
    }
}

std::string FileSystem::readFile(const std::string& filename) {
    try {
        std::string fullPath = appDir + "/" + filename;
        std::ifstream file(fullPath);
        if (!file.is_open()) {
            log("Failed to read file: " + filename);
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        log("Read file: " + filename);
        return buffer.str();
    }
    catch (const std::exception& e) {
        log("Error reading file: " + std::string(e.what()));
        return "";
    }
}

bool FileSystem::backupFiles() {
    try {
        auto timestamp = std::chrono::system_clock::now();
        auto timestampStr = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            timestamp.time_since_epoch()).count());

        std::string backupSubDir = backupDir + "/backup_" + timestampStr;
        fs::create_directory(backupSubDir);

        for (const auto& entry : fs::directory_iterator(appDir)) {
            if (entry.path().filename() != "backup" && entry.path().filename() != "app.log") {
                fs::copy(entry.path(), backupSubDir + "/" + entry.path().filename().string());
            }
        }

        log("Backup completed to: " + backupSubDir);
        return true;
    }
    catch (const fs::filesystem_error& e) {
        log("Backup failed: " + std::string(e.what()));
        return false;
    }
}

void FileSystem::startMonitoring() {
    if (isMonitoring) return;

    isMonitoring = true;
    monitorThread = std::thread(&FileSystem::monitorDirectory, this);
    log("Started directory monitoring");
}

void FileSystem::stopMonitoring() {
    if (!isMonitoring) return;

    isMonitoring = false;
    monitorCv.notify_all();
    if (monitorThread.joinable()) {
        monitorThread.join();
    }
    log("Stopped directory monitoring");
}

void FileSystem::monitorDirectory() {
    std::unique_lock<std::mutex> lock(logMutex);

    while (isMonitoring) {
        // Simple monitoring: check for new/deleted files
        static std::vector<std::string> lastFiles;
        std::vector<std::string> currentFiles;

        for (const auto& entry : fs::directory_iterator(appDir)) {
            if (entry.path().filename() != "backup" && entry.path().filename() != "app.log") {
                currentFiles.push_back(entry.path().filename().string());
            }
        }

        // Detect changes
        for (const auto& file : currentFiles) {
            if (std::find(lastFiles.begin(), lastFiles.end(), file) == lastFiles.end()) {
                log("New file detected: " + file);
            }
        }
        for (const auto& file : lastFiles) {
            if (std::find(currentFiles.begin(), currentFiles.end(), file) == currentFiles.end()) {
                log("File deleted: " + file);
            }
        }

        lastFiles = currentFiles;

        // Wait for next check or stop signal
        monitorCv.wait_for(lock, std::chrono::seconds(5), [this] { return !isMonitoring; });
    }
}

void FileSystem::log(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_s(&localTime, &now_c); // Use localtime_s for safer time conversion

    std::stringstream ss;
    ss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S") << " - " << message;

    {
        std::lock_guard<std::mutex> lock(logMutex);
        logQueue.push(ss.str());
    }

    processLogQueue();
}

void FileSystem::processLogQueue() {
    std::lock_guard<std::mutex> lock(logMutex);

    if (needsLogRotation()) {
        rotateLogFile();
    }

    std::ofstream logStream(logFile, std::ios::app);
    if (!logStream.is_open()) return;

    while (!logQueue.empty()) {
        logStream << logQueue.front() << "\n";
        logQueue.pop();
    }
}

bool FileSystem::needsLogRotation() {
    try {
        return fs::exists(logFile) && fs::file_size(logFile) >= maxLogSize;
    }
    catch (const fs::filesystem_error&) {
        return false;
    }
}

void FileSystem::rotateLogFile() {
    try {
        auto timestamp = std::chrono::system_clock::now();
        auto timestampStr = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            timestamp.time_since_epoch()).count());

        std::string newLogFile = appDir + "/app_" + timestampStr + ".log";
        fs::rename(logFile, newLogFile);
        log("Rotated log file to: " + newLogFile);
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Log rotation failed: " << e.what() << std::endl;
    }
}
