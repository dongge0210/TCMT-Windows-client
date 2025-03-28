#pragma once
#include <string>
#include <fstream>
#include <mutex>

class Logger {
public:
    static void Initialize(const std::string& logPath = "system_monitor.log");
    static void Error(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message); // 添加 Warning 方法
    static void Log(const std::string& level, const std::string& message); // 添加 Log 方法声明
	static void Close(); // 添加 Close 方法声明

private:
    static std::ofstream logFile;
    static std::mutex logMutex; // 添加 logMutex 声明
};
