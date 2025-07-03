#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <algorithm> // Added for std::transform

// 日志等级枚举
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5,
    FATAL = 6
};

class Logger {
private:
    static std::ofstream logFile;
    static std::mutex logMutex;
    static bool consoleOutputEnabled; // Flag for console output
    static LogLevel currentLogLevel; // 当前日志等级过滤器
    static void WriteLog(const std::string& level, const std::string& message, LogLevel msgLevel);
    static std::wstring ConvertToWideString(const std::string& utf8Str); // Helper for UTF-8 to wide string conversion

public:
    static void Initialize(const std::string& logFilePath);
    static void EnableConsoleOutput(bool enable); // Method to enable/disable console output
    static void SetLogLevel(LogLevel level); // 设置日志等级过滤器
    static LogLevel GetLogLevel(); // 获取当前日志等级
    
    // Log level methods (ordered by severity: Trace < Debug < Info < Warning < Error < Critical < Fatal)
    static void Trace(const std::string& message);   // 最详细的信息，通常只在调试时使用
    static void Debug(const std::string& message);   // 调试信息
    static void Info(const std::string& message);    // 一般信息
    static void Warning(const std::string& message); // 警告信息
    static void Error(const std::string& message);   // 错误信息
    static void Critical(const std::string& message); // 严重错误
    static void Fatal(const std::string& message);   // 致命错误
};
