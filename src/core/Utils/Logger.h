#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <algorithm> // Added for std::transform
#include <windows.h> // For console color support

// 日志等级枚举
enum LogLevel {
    LOG_TRACE = 0,
    LOG_DEBUG = 1,
    LOG_INFO = 2,
    LOG_WARNING = 3,
    LOG_ERROR = 4,
    LOG_CRITICAL = 5,
    LOG_FATAL = 6
};

// 控制台颜色枚举
enum class ConsoleColor {
    WHITE = 15,      // 白色
    PURPLE = 13,     // 紫色 - TRACE & DEBUG
    GREEN = 10,      // 绿色  
    LIGHT_GREEN = 10, // 亮绿色 - INFO
    YELLOW = 14,     // 黄色 - WARNING
    ORANGE = 12,     // 红色(作为橙色) - ERROR
    RED = 12,        // 红色 - CRITICAL
    DARK_RED = 4     // 深红色 - FATAL
};

class Logger {
private:
    static std::ofstream logFile;
    static std::mutex logMutex;
    static bool consoleOutputEnabled; // Flag for console output
    static LogLevel currentLogLevel; // 当前日志等级过滤器
    static HANDLE hConsole; // 控制台句柄
    static void WriteLog(const std::string& level, const std::string& message, LogLevel msgLevel, ConsoleColor color);
    static std::wstring ConvertToWideString(const std::string& utf8Str); // Helper for UTF-8 to wide string conversion
    static void SetConsoleColor(ConsoleColor color); // 设置控制台颜色
    static void ResetConsoleColor(); // 重置控制台颜色

public:
    static void Initialize(const std::string& logFilePath);
    static void EnableConsoleOutput(bool enable); // Method to enable/disable console output
    static void SetLogLevel(LogLevel level); // 设置日志等级过滤器
    static LogLevel GetLogLevel(); // 获取当前日志等级
    
    // Log level methods (ordered by severity: Trace < Debug < Info < Warning < Error < Critical < Fatal)
    static void Trace(const std::string& message);   // 最详细的信息，通常只在调试时使用 (白色)
    static void Debug(const std::string& message);   // 调试信息 (绿色)
    static void Info(const std::string& message);    // 一般信息 (绿色)
    static void Warn(const std::string& message);    // 警告信息 (黄色)
    static void Error(const std::string& message);   // 错误信息 (橙色)
    static void Critical(const std::string& message); // 严重错误 (红色)
    static void Fatal(const std::string& message);   // 致命错误 (深红色)
};
