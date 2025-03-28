// 文件名：LibreHardwareMonitorBridge.h
#pragma once
#include <vector>
#include <string>

class LibreHardwareMonitorBridge {
public:
    static void Initialize();
    static std::vector<std::pair<std::string, float>> GetTemperatures();
};