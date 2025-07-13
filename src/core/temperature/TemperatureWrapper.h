#pragma once

#include <string>
#include <vector>
#include <utility>

// 本机C++包装器类，用于调用托管的LibreHardwareMonitorBridge
class TemperatureWrapper {
public:
    static void Initialize();
    static void Cleanup();
    static std::vector<std::pair<std::string, double>> GetTemperatures();
    static bool IsInitialized();

private:
    static bool initialized;
};
