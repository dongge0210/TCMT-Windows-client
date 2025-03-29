#pragma once
#include <string>
#include <fstream>
#include <mutex>

class Logger {
private:
    static std::ofstream logFile;
    static std::mutex logMutex;
    static void WriteLog(const std::string& level, const std::string& message);

public:
    static void Initialize(const std::string& logFilePath);
    static void Info(const std::string& message);
    static void Error(const std::string& message);
    static void Warning(const std::string& message);
};
