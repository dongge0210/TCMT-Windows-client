#include "Logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

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
        // 获取当前时间
        auto now = std::chrono::system_clock::now();
        auto time_now = std::chrono::system_clock::to_time_t(now);

        // 使用安全版本的localtime
        std::tm timeinfo;
        localtime_s(&timeinfo, &time_now);

        // 使用字符串流来构建日志消息
        std::stringstream ss;
        ss << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "]"
            << "[" << level << "] "
            << message
            << std::endl;

        // 写入日志文件
        logFile << ss.str();
        logFile.flush();
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

