#ifndef FILE_SYSTEM_HPP  
#define FILE_SYSTEM_HPP  

#include <string>  
#include <thread>  
#include <mutex>  
#include <condition_variable>  
#include <queue>  
#include <fstream>  
#include <filesystem>  

namespace fs = std::filesystem;  

class FileSystem {  
private:  
    std::string appName;  
    size_t maxLogSize;  
    std::string appDir;  
    std::string backupDir;  
    std::string logFile;  
    bool isMonitoring;  
    std::thread monitorThread;  
    std::mutex logMutex;  
    std::condition_variable monitorCv;  
    std::queue<std::string> logQueue;  

    void monitorDirectory();  
    void processLogQueue();  
    bool needsLogRotation();  
    void rotateLogFile();  

public:  
    FileSystem(const std::string& appName, size_t maxLogSize);  
    ~FileSystem();  

    std::string getAppDataPath();  
    bool initialize();  
    bool createFile(const std::string& filename, const std::string& content);  
    std::string readFile(const std::string& filename);  
    bool backupFiles();  
    void startMonitoring();  
    void stopMonitoring();  
    void log(const std::string& message); // Public method declaration for logging
};  

#endif // FILE_SYSTEM_HPP

