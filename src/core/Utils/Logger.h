#pragma once
#include <string>
#include <fstream>
#include <mutex>

class Logger {
private:
    static std::ofstream logFile;
    static std::mutex logMutex;
    static bool consoleOutputEnabled; // Flag for console output
    static void WriteLog(const std::string& level, const std::string& message);
    static std::wstring ConvertToWideString(const std::string& utf8Str); // Helper for UTF-8 to wide string conversion

public:
    static void Initialize(const std::string& logFilePath);
    static void EnableConsoleOutput(bool enable); // Method to enable/disable console output
    static void Info(const std::string& message);
    static void Error(const std::string& message);
    static void Warning(const std::string& message);
};
