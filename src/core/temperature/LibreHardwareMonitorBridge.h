#pragma once
#include <string>
#include <vector>
#include <utility>

class LibreHardwareMonitorBridge {
public:
    static void Initialize();
    // 改为 vector<pair> 返回类型
    static std::vector<std::pair<std::string, float>> GetTemperatures();
private:
    static bool initialized;
};
