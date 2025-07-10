#include "stdafx.h"
#include "Logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <io.h>
#include <fcntl.h>
#include <windows.h> // For MultiByteToWideChar
#include <algorithm> // For std::transform

std::ofstream Logger::logFile;
std::mutex Logger::logMutex;
bool Logger::consoleOutputEnabled = false; // Initialize console output flag
LogLevel Logger::currentLogLevel = LOG_INFO; // 默认日志等级为INFO
HANDLE Logger::hConsole = GetStdHandle(STD_OUTPUT_HANDLE); // 初始化控制台句柄

void Logger::Initialize(const std::string& logFilePath) {
    logFile.open(logFilePath, std::ios::binary | std::ios::app);
    if (!logFile.is_open()) {
        throw std::runtime_error("无法打开日志文件");
    }

    // Write UTF-8 BOM if the file is empty
    if (logFile.tellp() == 0) {
        const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
        logFile.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    }

    // Set console output to UTF-8
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
}

void Logger::EnableConsoleOutput(bool enable) {
    consoleOutputEnabled = enable;
}

void Logger::SetLogLevel(LogLevel level) {
    currentLogLevel = level;
}

LogLevel Logger::GetLogLevel() {
    return currentLogLevel;
}

void Logger::SetConsoleColor(ConsoleColor color) {
    if (hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
    }
}

void Logger::ResetConsoleColor() {
    if (hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(hConsole, 7); // 默认白色
    }
}

std::wstring Logger::ConvertToWideString(const std::string& utf8Str) {
    // Handle empty string case
    if (utf8Str.empty()) {
        return std::wstring();
    }
    
    // Get required buffer size
    int bufferSize = MultiByteToWideChar(
        CP_UTF8,                // Code page: UTF-8
        0,                      // Flags
        utf8Str.c_str(),        // Source UTF-8 string
        static_cast<int>(utf8Str.length()), // Source string length
        nullptr,                // Output buffer (null to get required size)
        0                       // Output buffer size
    );
    
    if (bufferSize == 0) {
        throw std::runtime_error("Failed to convert UTF-8 string to wide string");
    }
    
    // Create buffer to hold the wide string
    std::wstring wideStr(bufferSize, L'\0');
    
    // Convert the string
    if (!MultiByteToWideChar(
        CP_UTF8,                // Code page: UTF-8
        0,                      // Flags
        utf8Str.c_str(),        // Source UTF-8 string
        static_cast<int>(utf8Str.length()), // Source string length
        &wideStr[0],            // Output buffer
        bufferSize              // Output buffer size
    )) {
        throw std::runtime_error("Failed to convert UTF-8 string to wide string");
    }
    
    return wideStr;
}

void Logger::WriteLog(const std::string& level, const std::string& message, LogLevel msgLevel, ConsoleColor color) {
    // 检查日志等级过滤
    if (msgLevel < currentLogLevel) {
        return; // 跳过低于当前等级的日志
    }

    std::lock_guard<std::mutex> lock(logMutex);

    if (message.empty()) {
        throw std::invalid_argument("日志消息不能为空");
    }

    if (logFile.is_open()) {
        // Validate stream state
        if (!logFile.good()) {
            throw std::runtime_error("日志文件流状态无效");
        }

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_now = std::chrono::system_clock::to_time_t(now);

        // Use safe version of localtime
        std::tm timeinfo;
        localtime_s(&timeinfo, &time_now);

        // Construct log message
        std::stringstream ss;
        ss << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "]"
           << "[" << level << "] "
           << message
           << std::endl;

        std::string logEntry = ss.str();

        // Ensure buffer size is even
        if (logEntry.size() % 2 != 0) {
            logEntry += " "; // Add padding to make size even
        }

        // Write to log file
        logFile.write(logEntry.c_str(), logEntry.size());
        logFile.flush();

        // Optional console output with color
        if (consoleOutputEnabled) {
            // 构建时间戳部分
            std::stringstream timeStream;
            timeStream << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "]";
            std::string timeStr = timeStream.str();
            
            // 输出时间戳（默认白色）
            std::wstring wideTimeStr = ConvertToWideString(timeStr);
            std::wcout << wideTimeStr;
            
            // 输出等级标签（有颜色）
            SetConsoleColor(color);
            std::string levelStr = "[" + level + "] ";
            std::wstring wideLevelStr = ConvertToWideString(levelStr);
            std::wcout << wideLevelStr;
            ResetConsoleColor();
            
            // 输出消息内容（默认白色）
            std::wstring wideMessage = ConvertToWideString(message + "\n");
            std::wcout << wideMessage;
        }
    } else {
        throw std::runtime_error("日志文件未打开");
    }
}

void Logger::Trace(const std::string& message) {
    WriteLog("TRACE", message, LOG_TRACE, ConsoleColor::PURPLE);
}

void Logger::Debug(const std::string& message) {
    WriteLog("DEBUG", message, LOG_DEBUG, ConsoleColor::PURPLE);
}

void Logger::Info(const std::string& message) {
    WriteLog("INFO", message, LOG_INFO, ConsoleColor::LIGHT_GREEN);
}

void Logger::Warn(const std::string& message) {
    WriteLog("WARN", message, LOG_WARNING, ConsoleColor::YELLOW);
}

void Logger::Error(const std::string& message) {
    WriteLog("ERROR", message, LOG_ERROR, ConsoleColor::ORANGE);
}

void Logger::Critical(const std::string& message) {
    WriteLog("CRITICAL", message, LOG_CRITICAL, ConsoleColor::RED);//我不知道为什么Critical和Fatal意思一样，之前也看过，但是AI就是给它们分开了?
}

void Logger::Fatal(const std::string& message) {
    WriteLog("FATAL", message, LOG_FATAL, ConsoleColor::DARK_RED);
}
