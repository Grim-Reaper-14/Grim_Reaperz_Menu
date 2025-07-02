#ifndef FILE_SYSTEM_HPP
#define FILE_SYSTEM_HPP

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <unordered_map>

namespace fs = std::filesystem;

class FileSystem {
public:
    enum class LogLevel { INFO, WARN, ERROR };

    FileSystem(const std::string& appName, size_t maxLogSize, const std::string& logFileBaseName = "log");
    ~FileSystem();

    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;
    FileSystem(FileSystem&&) = delete;
    FileSystem& operator=(FileSystem&&) = delete;

    std::string getAppDataPath() const;
    bool initialize();
    bool createFile(const std::string& filename, const std::string& content);
    std::string readFile(const std::string& filename);
    bool backupFiles();

    void startMonitoring();
    void stopMonitoring();

    void startAsyncBackup();
    void stopAsyncBackup();

    void log(const std::string& message, LogLevel level = LogLevel::INFO);

private:
    const std::string appName_;
    const size_t maxLogSize_;
    fs::path appDataDir_;
    fs::path backupDir_;
    fs::path logFilePath_;

    std::string baseLogFileName_;
    std::string activeLogFileName_;

    std::atomic<bool> isMonitoring_{false};
    std::atomic<bool> isBackingUp_{false};

    std::thread monitorThread_;
    std::thread backupThread_;
    std::mutex logMutex_;
    std::condition_variable monitorCv_;
    std::queue<std::string> logQueue_;

    std::unordered_map<std::string, fs::file_time_type> fileTimes_;

    void monitorDirectory();
    void processLogQueue();
    bool needsLogRotation();
    void rotateLogFile();

    void compressOldLogs();
    void watchFilesForChanges();

    bool createDirectoriesIfNotExist(const fs::path& path);
    std::string currentTimestamp() const;
    std::string currentDate() const;
    std::string logLevelToString(LogLevel level) const;
    std::string toJsonLog(const std::string& timestamp, const std::string& level, const std::string& message) const;
};

#endif // FILE_SYSTEM_HPP
