#pragma once
#include <string>
#include <vector>
#include <utility>

class LibreHardwareMonitorBridge {
public:
    static void Initialize();
    static void Cleanup();  // 添加清理方法声明
    static std::vector<std::pair<std::string, float>> GetTemperatures();

private:
    static bool initialized;
};
