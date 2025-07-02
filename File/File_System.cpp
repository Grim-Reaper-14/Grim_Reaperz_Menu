// File_System.cpp
#include "File_System.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cstdlib>

FileSystem::FileSystem(const std::string& appName, size_t maxLogSize, const std::string& logFileBaseName)
    : appName_(appName), maxLogSize_(maxLogSize), baseLogFileName_(logFileBaseName)
{
    appDataDir_ = fs::current_path() / (appName_ + "_data");
    backupDir_ = appDataDir_ / "backup";
    activeLogFileName_ = baseLogFileName_ + "_" + currentDate() + ".txt";
    logFilePath_ = appDataDir_ / activeLogFileName_;
}

FileSystem::~FileSystem() {
    stopMonitoring();
    stopAsyncBackup();
}

std::string FileSystem::getAppDataPath() const {
    return appDataDir_.string();
}

bool FileSystem::initialize() {
    try {
        bool ok = createDirectoriesIfNotExist(appDataDir_) && createDirectoriesIfNotExist(backupDir_);
        if (!ok) return false;

        if (!fs::exists(logFilePath_)) {
            std::ofstream ofs(logFilePath_);
            if (!ofs) return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Initialize failed: " << e.what() << "\n";
        return false;
    }
}

bool FileSystem::createFile(const std::string& filename, const std::string& content) {
    try {
        fs::path filePath = appDataDir_ / filename;
        std::ofstream ofs(filePath);
        if (!ofs) return false;
        ofs << content;
        return true;
    } catch (...) {
        return false;
    }
}

std::string FileSystem::readFile(const std::string& filename) {
    try {
        fs::path filePath = appDataDir_ / filename;
        std::ifstream ifs(filePath);
        if (!ifs) return "";
        std::ostringstream ss;
        ss << ifs.rdbuf();
        return ss.str();
    } catch (...) {
        return "";
    }
}

bool FileSystem::backupFiles() {
    try {
        for (const auto& entry : fs::directory_iterator(appDataDir_)) {
            if (entry.is_regular_file() && entry.path() != logFilePath_) {
                fs::copy(entry.path(), backupDir_ / entry.path().filename(),
                         fs::copy_options::overwrite_existing);
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

void FileSystem::startMonitoring() {
    if (isMonitoring_) return;
    isMonitoring_ = true;
    monitorThread_ = std::thread(&FileSystem::monitorDirectory, this);
}

void FileSystem::stopMonitoring() {
    if (!isMonitoring_) return;
    isMonitoring_ = false;
    monitorCv_.notify_all();
    if (monitorThread_.joinable()) monitorThread_.join();
}

void FileSystem::startAsyncBackup() {
    if (isBackingUp_) return;
    isBackingUp_ = true;
    backupThread_ = std::thread([this]() {
        while (isBackingUp_) {
            backupFiles();
            compressOldLogs();
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    });
}

void FileSystem::stopAsyncBackup() {
    if (!isBackingUp_) return;
    isBackingUp_ = false;
    if (backupThread_.joinable()) backupThread_.join();
}

void FileSystem::log(const std::string& message, LogLevel level) {
    std::string timestamp = currentTimestamp();
    std::string levelStr = logLevelToString(level);
    std::string json = toJsonLog(timestamp, levelStr, message);

    {
        std::lock_guard<std::mutex> lock(logMutex_);
        logQueue_.push(json);
    }
    monitorCv_.notify_all();
}

void FileSystem::monitorDirectory() {
    while (isMonitoring_) {
        std::unique_lock<std::mutex> lock(logMutex_);
        monitorCv_.wait_for(lock, std::chrono::seconds(1), [this]() {
            return !logQueue_.empty() || !isMonitoring_;
        });
        if (!isMonitoring_) break;

        processLogQueue();
        watchFilesForChanges();
        if (needsLogRotation()) rotateLogFile();
    }
}

void FileSystem::processLogQueue() {
    std::ofstream ofs(logFilePath_, std::ios::app);
    while (!logQueue_.empty()) {
        ofs << logQueue_.front() << "\n";
        std::cout << logQueue_.front() << "\n"; // live console tail
        logQueue_.pop();
    }
}

bool FileSystem::needsLogRotation() {
    try {
        std::string today = currentDate();
        std::string expectedName = baseLogFileName_ + "_" + today + ".txt";
        if (activeLogFileName_ != expectedName) return true;
        if (fs::exists(logFilePath_) && fs::file_size(logFilePath_) >= maxLogSize_) return true;
        return false;
    } catch (...) {
        return false;
    }
}

void FileSystem::rotateLogFile() {
    try {
        std::string todayName = baseLogFileName_ + "_" + currentDate() + ".txt";
        if (activeLogFileName_ != todayName) {
            activeLogFileName_ = todayName;
            logFilePath_ = appDataDir_ / activeLogFileName_;
            std::ofstream ofs(logFilePath_);
        } else {
            std::string rotated = baseLogFileName_ + "_" + currentDate() + "_" + currentTimestamp() + ".txt";
            fs::rename(logFilePath_, appDataDir_ / rotated);
            std::ofstream ofs(logFilePath_);
        }
    } catch (const std::exception& e) {
        std::cerr << "Rotation failed: " << e.what() << "\n";
    }
}

void FileSystem::compressOldLogs() {
    try {
        for (const auto& entry : fs::directory_iterator(appDataDir_)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                if (path.extension() == ".txt" && path.filename() != activeLogFileName_) {
                    std::string zipName = path.stem().string() + ".zip";
                    std::string cmd = "zip -j \"" + (appDataDir_ / zipName).string() + "\" \"" + path.string() + "\"";
                    if (std::system(cmd.c_str()) == 0) {
                        fs::remove(path);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Compress failed: " << e.what() << "\n";
    }
}

void FileSystem::watchFilesForChanges() {
    try {
        for (const auto& entry : fs::directory_iterator(appDataDir_)) {
            if (entry.is_regular_file()) {
                auto path = entry.path();
                auto currentTime = fs::last_write_time(path);
                auto it = fileTimes_.find(path.string());
                if (it == fileTimes_.end()) {
                    fileTimes_[path.string()] = currentTime;
                } else if (currentTime != it->second) {
                    it->second = currentTime;
                    std::cout << "File changed: " << path.filename() << "\n";
                }
            }
        }
    } catch (...) {
        // ignore errors
    }
}

bool FileSystem::createDirectoriesIfNotExist(const fs::path& path) {
    try {
        if (!fs::exists(path)) return fs::create_directories(path);
        return true;
    } catch (...) {
        return false;
    }
}

std::string FileSystem::currentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return ss.str();
}

std::string FileSystem::currentDate() const {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d");
    return ss.str();
}

std::string FileSystem::logLevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

std::string FileSystem::toJsonLog(const std::string& timestamp, const std::string& level, const std::string& message) const {
    std::ostringstream ss;
    ss << "{"
       << "\"timestamp\":\"" << timestamp << "\","
       << "\"level\":\"" << level << "\","
       << "\"message\":\"" << message << "\""
       << "}";
    return ss.str();
}
