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
#include <vector> // For std::vector used in UTF-8 to UTF-16 conversion

std::ofstream Logger::logFile;
std::mutex Logger::logMutex;
bool Logger::consoleOutputEnabled = true; // Initialize console output flag
LogLevel Logger::currentLogLevel = LOG_DEBUG; // 默认日志等级为INFO
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

    // 设置控制台编码为UTF-8，确保中文显示正确
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
    
    // 启用控制台虚拟终端序列处理（如果支持）
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        GetConsoleMode(hOut, &dwMode);
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
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

[[nodiscard]] bool Logger::IsInitialized() {
    return logFile.is_open();
}

void Logger::SetConsoleColor(ConsoleColor color) {
    if (hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
    }
}

void Logger::ResetConsoleColor() {
    if (hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(hConsole, static_cast<WORD>(7)); // 默认白色，显式转换WORD，消除C4365
    }
}

std::wstring Logger::ConvertToWideString(const std::string& utf8Str) {
    // Handle empty string case
    if (utf8Str.empty()) {
        return std::wstring();
    }
    // Get required buffer size
    int bufferSize = MultiByteToWideChar(
        CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.length()), nullptr, 0);
    if (bufferSize == 0) {
        throw std::runtime_error("Failed to convert UTF-8 string to wide string");
    }
    // Create buffer to hold the wide string
    std::wstring wideStr(bufferSize, L'\0');
    // Convert the string
    if (!MultiByteToWideChar(
        CP_UTF8, 0, utf8Str.c_str(), static_cast<int>(utf8Str.length()), &wideStr[0], bufferSize)) {
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
    // 限制日志消息长度，防止极端内存占用
    constexpr size_t MAX_LOG_LENGTH = 4096;
    if (message.empty()) {
        throw std::invalid_argument("日志消息不能为空");
    }
    if (message.length() > MAX_LOG_LENGTH) {
        throw std::invalid_argument("日志消息过长");
    }
    if (logFile.is_open()) {
        if (!logFile.good()) {
            throw std::runtime_error("日志文件流状态无效");
        }
        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_now = std::chrono::system_clock::to_time_t(now);
        std::tm timeinfo;
        if (localtime_s(&timeinfo, &time_now) != 0) {
            throw std::runtime_error("localtime_s 失败");
        }
        std::stringstream ss;
        ss << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "]"
           << "[" << level << "] "
           << message
           << std::endl;
        std::string logEntry = ss.str();
        try {
            logFile.write(logEntry.c_str(), logEntry.size());
            logFile.flush();
        } catch (const std::exception& ex) {
            // 日志写入异常，输出到标准错误
            std::cerr << "日志写入异常: " << ex.what() << std::endl;
        }
        // Enhanced console output with proper UTF-8 support and selective coloring
        if (consoleOutputEnabled) {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hConsole != INVALID_HANDLE_VALUE) {
                std::stringstream timeStamp;
                timeStamp << "[" << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S") << "]";
                std::string timeStr = timeStamp.str();
                std::string levelTag = "[" + level + "]";
                DWORD written;
                int timeWideLength = MultiByteToWideChar(CP_UTF8, 0, timeStr.c_str(), -1, nullptr, 0);
                if (timeWideLength > 0) {
                    std::vector<wchar_t> timeWideText(static_cast<size_t>(timeWideLength));
                    if (MultiByteToWideChar(CP_UTF8, 0, timeStr.c_str(), -1, timeWideText.data(), timeWideLength)) {
                        WriteConsoleW(hConsole, timeWideText.data(), static_cast<DWORD>(timeWideLength - 1), &written, NULL);
                    }
                }
                SetConsoleColor(color);
                int levelWideLength = MultiByteToWideChar(CP_UTF8, 0, levelTag.c_str(), -1, nullptr, 0);
                if (levelWideLength > 0) {
                    std::vector<wchar_t> levelWideText(static_cast<size_t>(levelWideLength));
                    if (MultiByteToWideChar(CP_UTF8, 0, levelTag.c_str(), -1, levelWideText.data(), levelWideLength)) {
                        WriteConsoleW(hConsole, levelWideText.data(), static_cast<DWORD>(levelWideLength - 1), &written, NULL);
                    }
                }
                ResetConsoleColor();
                WriteConsoleW(hConsole, L" ", 1, &written, NULL);
                int msgWideLength = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
                if (msgWideLength > 0) {
                    std::vector<wchar_t> msgWideText(static_cast<size_t>(msgWideLength));
                    if (MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, msgWideText.data(), msgWideLength)) {
                        WriteConsoleW(hConsole, msgWideText.data(), static_cast<DWORD>(msgWideLength - 1), &written, NULL);
                    }
                }
                WriteConsoleW(hConsole, L"\n", 1, &written, NULL);
            }
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
