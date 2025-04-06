#pragma once
#include <string>
#include <vector>
#include <utility>

class LibreHardwareMonitorBridge {
public:
    static void Initialize();
    static void Cleanup();
    static std::vector<std::pair<std::string, float>> GetTemperatures();

private:
    static bool initialized;
};
