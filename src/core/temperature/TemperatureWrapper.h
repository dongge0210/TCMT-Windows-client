#pragma once
#include <string>
#include <vector>
#include <utility>

// 前向声明，避免在头文件中包含GPU头
class GpuInfo;

class TemperatureWrapper {
public:
    static void Initialize();
    static void Cleanup();
    static std::vector<std::pair<std::string, double> > GetTemperatures();
    static bool IsInitialized();

private:
    static bool initialized;
};
