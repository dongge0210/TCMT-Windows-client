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

std::ofstream Logger::logFile;
std::mutex Logger::logMutex;
bool Logger::consoleOutputEnabled = false; // Initialize console output flag

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

void Logger::WriteLog(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);

    if (message.empty()) {
        throw std::invalid_argument("Log message cannot be empty");
    }

    if (logFile.is_open()) {
        // Validate stream state
        if (!logFile.good()) {
            throw std::runtime_error("Log file stream is in an invalid state");
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

        // Optional console output
        if (consoleOutputEnabled) {
            std::wstring wideLogEntry = ConvertToWideString(logEntry);
            std::wcout << wideLogEntry; // Output to console only once
        }
    } else {
        throw std::runtime_error("Log file is not open");
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

