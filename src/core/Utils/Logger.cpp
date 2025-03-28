#include "Logger.h"
#include <ctime>
#include <iomanip>

std::ofstream Logger::logFile;
std::mutex Logger::logMutex;

void Logger::Initialize(const std::string& logFilePath) {
    logFile.open(logFilePath, std::ios::app);
    if (!logFile.is_open()) {
        throw std::runtime_error("无法打开日志文件");
    }
}

void Logger::WriteLog(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        logFile << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "]"
                << "[" << level << "] " << message << std::endl;
    }
}

void Logger::Info(const std::string& message) {
    WriteLog("INFO", message);
}

void Logger::Error(const std::string& message) {
    WriteLog("ERROR", message);
}

void Logger::Warning(const std::string& message) {
    WriteLog("WARNING", message);
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      